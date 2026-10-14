#include "controller.h"
#include "ts3_source.h"
#include "teamspeak/public_errors.h"
#include <QApplication>
#include <QDir>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>

#ifdef _WIN32
#define TS_EXPORT extern "C" __declspec(dllexport)
#else
#define TS_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
TS3Functions functions{};
std::mutex dispatchMutex;
wt::Controller* controller = nullptr;
enum Menu { Toggle = 1, Settings = 2, ResetPosition = 3 };

// The lock protects ONLY receiver lifetime and enqueue order. SDK calls and widget
// work are performed later by the GUI thread. There is no lock held while executing.
void dispatch(std::function<void(wt::Controller&)> action) {
    std::lock_guard<std::mutex> lock(dispatchMutex);
    if (!controller) return;
    auto* receiver = controller;
    QMetaObject::invokeMethod(receiver, [receiver, action = std::move(action)] { action(*receiver); },
                              Qt::QueuedConnection);
}
void event(wt::Event value) {
    dispatch([value = std::move(value)](wt::Controller& c) { c.handle(value); });
}
void simple(wt::EventType type, uint64 server, anyID client = 0) {
    wt::Event e; e.type = type; e.key = {server, client}; event(std::move(e));
}
void moved(uint64 server, anyID client, uint64 oldChannel, uint64 newChannel, int /*visibility*/) {
    if (!newChannel) { simple(wt::EventType::RemoveClient, server, client); return; }
    wt::Event e; e.type = wt::EventType::ClientMoved; e.key = {server, client};
    e.oldChannel = oldChannel; e.newChannel = newChannel; event(std::move(e));
}
void onGuiSync(const std::function<void()>& action) {
    if (QThread::currentThread() == qApp->thread()) action();
    else QMetaObject::invokeMethod(qApp, action, Qt::BlockingQueuedConnection);
}
template<typename T> T* allocate() { return static_cast<T*>(std::calloc(1, sizeof(T))); }
}

TS_EXPORT const char* ts3plugin_name() { return "WhosTalkin"; }
TS_EXPORT const char* ts3plugin_version() { return WHOSTALKIN_VERSION; }
TS_EXPORT const char* ts3plugin_author() { return "Khivus"; }
TS_EXPORT const char* ts3plugin_description() {
    return "A compact active-speaker window with incoming whisper detection. TeamSpeak 3.6.x, API 26, Qt 5.15.2.";
}
TS_EXPORT int ts3plugin_apiVersion() { return 26; }
TS_EXPORT void ts3plugin_setFunctionPointers(const TS3Functions f) { functions = f; }
TS_EXPORT void ts3plugin_registerPluginID(const char*) {} // No SDK calls here need a plugin ID.
TS_EXPORT int ts3plugin_requestAutoload() { return 1; }
TS_EXPORT int ts3plugin_offersConfigure() { return PLUGIN_OFFERS_CONFIGURE_QT_THREAD; }
TS_EXPORT void ts3plugin_freeMemory(void* data) { std::free(data); }
TS_EXPORT const char* ts3plugin_commandKeyword() { return "whostalkin"; }

TS_EXPORT int ts3plugin_init() {
    if (!qobject_cast<QApplication*>(QCoreApplication::instance()) || std::strcmp(qVersion(), "5.15.2") != 0) {
        if (functions.logMessage) functions.logMessage("WhosTalkin requires the TS3 Qt 5.15.2 GUI runtime.",
                                                       LogLevel_ERROR, "WhosTalkin", 0);
        return 1;
    }
    int result = 0;
    onGuiSync([&result] {
        try {
            char config[4096]{};
            if (functions.getConfigPath) functions.getConfigPath(config, sizeof(config));
            const QString directory = config[0] ? QString::fromUtf8(config)
                : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            auto* created = new wt::Controller(std::make_unique<wt::Ts3Source>(functions),
                                               QDir(directory).filePath("whostalkin.ini"));
            {
                std::lock_guard<std::mutex> lock(dispatchMutex);
                if (controller) { delete created; result = 1; return; }
                controller = created;
            }
            created->initialize();
        } catch (...) {
            result = 1;
            wt::Controller* failed;
            { std::lock_guard<std::mutex> lock(dispatchMutex); failed = controller; controller = nullptr; }
            delete failed;
        }
    });
    return result;
}
TS_EXPORT void ts3plugin_shutdown() {
    wt::Controller* removed;
    { std::lock_guard<std::mutex> lock(dispatchMutex); removed = controller; controller = nullptr; }
    if (!removed) return;
    // Lifecycle barrier only. Delete all windows, timers and receiver-context
    // queued functors synchronously BEFORE the client releases this DLL.
    onGuiSync([removed] { delete removed; });
}
TS_EXPORT void ts3plugin_configure(void*, void*) { dispatch([](wt::Controller& c) { c.openSettings(); }); }
TS_EXPORT int ts3plugin_processCommand(uint64, const char* command) {
    const QString text = QString::fromUtf8(command ? command : "").trimmed().toLower();
    if (text == "show") dispatch([](wt::Controller& c) { c.show(); });
    else if (text == "hide") dispatch([](wt::Controller& c) { c.hide(); });
    else if (text.isEmpty() || text == "toggle") dispatch([](wt::Controller& c) { c.toggle(); });
    else if (text == "settings") dispatch([](wt::Controller& c) { c.openSettings(); });
    else if (text == "reset") dispatch([](wt::Controller& c) { c.resetPosition(); });
    else return 1;
    return 0;
}
TS_EXPORT void ts3plugin_initMenus(PluginMenuItem*** menus, char** icon) {
    *icon = nullptr;
    *menus = static_cast<PluginMenuItem**>(std::calloc(4, sizeof(PluginMenuItem*)));
    if (!*menus) return;
    const char* names[] = {"Show / hide Active Speakers", "Settings", "Reset window position"};
    for (int i = 0; i < 3; ++i) {
        auto* item = allocate<PluginMenuItem>();
        if (!item) break; // null-terminated, client frees each item via ts3plugin_freeMemory.
        item->type = PLUGIN_MENU_TYPE_GLOBAL; item->id = i + 1;
        std::memcpy(item->text, names[i], std::strlen(names[i]) + 1);
        (*menus)[i] = item;
    }
}
TS_EXPORT void ts3plugin_onMenuItemEvent(uint64, PluginMenuType type, int id, uint64) {
    if (type != PLUGIN_MENU_TYPE_GLOBAL) return;
    if (id == Toggle) dispatch([](wt::Controller& c) { c.toggle(); });
    else if (id == Settings) dispatch([](wt::Controller& c) { c.openSettings(); });
    else if (id == ResetPosition) dispatch([](wt::Controller& c) { c.resetPosition(); });
}
TS_EXPORT void ts3plugin_initHotkeys(PluginHotkey*** hotkeys) {
    *hotkeys = static_cast<PluginHotkey**>(std::calloc(2, sizeof(PluginHotkey*)));
    if (!*hotkeys) return;
    auto* key = allocate<PluginHotkey>();
    if (!key) return;
    std::memcpy(key->keyword, "toggle", sizeof("toggle"));
    std::memcpy(key->description, "Show / hide Active Speakers", sizeof("Show / hide Active Speakers"));
    (*hotkeys)[0] = key;
}
TS_EXPORT void ts3plugin_onHotkeyEvent(const char* keyword) {
    if (keyword && std::strcmp(keyword, "toggle") == 0) dispatch([](wt::Controller& c) { c.toggle(); });
}
TS_EXPORT void ts3plugin_onTalkStatusChangeEvent(uint64 server, int status, int whisper, anyID client) {
    wt::Event e; e.key = {server, client}; e.talkStatus = status; e.incomingWhisper = whisper != 0;
    event(std::move(e));
}
TS_EXPORT void ts3plugin_currentServerConnectionChanged(uint64 server) { simple(wt::EventType::ActiveServer, server); }
TS_EXPORT void ts3plugin_onConnectStatusChangeEvent(uint64 server, int status, unsigned int) {
    if (status == STATUS_DISCONNECTED) simple(wt::EventType::ClearServer, server);
    else if (status == STATUS_CONNECTION_ESTABLISHED) simple(wt::EventType::Connected, server);
}
TS_EXPORT void ts3plugin_onServerStopEvent(uint64 server, const char*) { simple(wt::EventType::ClearServer, server); }
TS_EXPORT void ts3plugin_onUpdateClientEvent(uint64 server, anyID client, anyID, const char*, const char*) {
    simple(wt::EventType::RefreshClient, server, client);
}
TS_EXPORT void ts3plugin_onClientDisplayNameChanged(uint64 server, anyID client, const char*, const char*) {
    // Re-query CLIENT_NICKNAME: a TS display name may also contain a local alias.
    simple(wt::EventType::RefreshClient, server, client);
}
TS_EXPORT void ts3plugin_onClientSelfVariableUpdateEvent(uint64 server, int, const char*, const char*) {
    simple(wt::EventType::RefreshClient, server, 0);
}
TS_EXPORT void ts3plugin_onClientMoveEvent(uint64 s, anyID c, uint64 o, uint64 n, int v, const char*) { moved(s,c,o,n,v); }
TS_EXPORT void ts3plugin_onClientMoveSubscriptionEvent(uint64 s, anyID c, uint64 o, uint64 n, int v) { moved(s,c,o,n,v); }
TS_EXPORT void ts3plugin_onClientMoveTimeoutEvent(uint64 s, anyID c, uint64 o, uint64 n, int v, const char*) { moved(s,c,o,n,v); }
TS_EXPORT void ts3plugin_onClientMoveMovedEvent(uint64 s, anyID c, uint64 o, uint64 n, int v, anyID, const char*, const char*, const char*) { moved(s,c,o,n,v); }
TS_EXPORT void ts3plugin_onClientKickFromChannelEvent(uint64 s, anyID c, uint64 o, uint64 n, int v, anyID, const char*, const char*, const char*) { moved(s,c,o,n,v); }
TS_EXPORT void ts3plugin_onClientKickFromServerEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, const char*) { simple(wt::EventType::RemoveClient,s,c); }
TS_EXPORT void ts3plugin_onClientBanFromServerEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, uint64, const char*) { simple(wt::EventType::RemoveClient,s,c); }
