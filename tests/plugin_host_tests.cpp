#include "ts3_functions.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>
#include <QWidget>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
int checks = 0;
int allocations = 0;
bool queriesOnGui = true;
QString configPath;
uint64 activeServer = 1;
using Key = std::pair<uint64, anyID>;
struct Client { std::string name; int talking = 0; bool whisper = false; bool muted = false; };
std::map<Key, Client> clients;
void check(bool good, const char* description) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
void queried() { queriesOnGui = queriesOnGui && QThread::currentThread() == qApp->thread(); }
template<typename T> T* memory(std::size_t count) {
    ++allocations; return static_cast<T*>(std::calloc(count, sizeof(T)));
}
unsigned int release(void* p) { queried(); std::free(p); --allocations; return ERROR_ok; }
unsigned int clientString(uint64 server, anyID client, std::size_t flag, char** out) {
    queried();
    if (flag != CLIENT_NICKNAME || !clients.count({server,client})) return ERROR_parameter_invalid;
    const auto& name = clients.at({server,client}).name;
    *out = memory<char>(name.size() + 1); std::memcpy(*out, name.c_str(), name.size() + 1); return ERROR_ok;
}
unsigned int serverString(uint64, std::size_t, char** out) {
    queried(); *out = memory<char>(5); std::memcpy(*out, "Test", 5); return ERROR_ok;
}
unsigned int clientInt(uint64 server, anyID client, std::size_t flag, int* out) {
    queried(); if (!clients.count({server,client})) return ERROR_parameter_invalid;
    *out = 0;
    if (flag == CLIENT_FLAG_TALKING) *out = clients.at({server,client}).talking;
    if (flag == CLIENT_INPUT_MUTED) *out = clients.at({server,client}).muted ? 1 : 0;
    if (flag == CLIENT_INPUT_HARDWARE) *out = 1;
    return ERROR_ok;
}
unsigned int ownId(uint64, anyID* out) { queried(); *out = 99; return ERROR_ok; }
unsigned int receiving(uint64 server, anyID client, int* out) {
    queried(); *out = clients.at({server,client}).whisper ? 1 : 0; return ERROR_ok;
}
unsigned int outgoing(uint64, anyID, int* out) { queried(); *out = 0; return ERROR_ok; }
unsigned int handlers(uint64** out) { queried(); *out = memory<uint64>(3); (*out)[0] = 1; (*out)[1] = 2; return ERROR_ok; }
unsigned int clientList(uint64 server, anyID** out) {
    queried(); *out = memory<anyID>(clients.size() + 1);
    std::size_t i = 0;
    for (const auto& pair : clients) if (pair.first.first == server) (*out)[i++] = pair.first.second;
    return ERROR_ok;
}
uint64 currentServer() { queried(); return activeServer; }
void config(char* path, std::size_t length) {
    queried(); const auto utf8 = QDir::toNativeSeparators(configPath).toUtf8();
    const auto copied = std::min(length - 1, static_cast<std::size_t>(utf8.size()));
    std::memcpy(path, utf8.constData(), copied); path[copied] = '\0';
}
unsigned int log(const char* message, LogLevel, const char*, uint64) { std::cerr << message << '\n'; return ERROR_ok; }
template<typename F> F resolve(QLibrary& dll, const char* name) {
    const auto result = dll.resolve(name); check(result != nullptr, name); return reinterpret_cast<F>(result);
}
void drain() { for (int i = 0; i < 4; ++i) QCoreApplication::processEvents(); }
QWidget* window() {
    for (auto* w : QApplication::topLevelWidgets()) if (w->objectName() == "whostalkinWindow") return w;
    return nullptr;
}
int count() {
    auto* w = window(); check(w != nullptr, "plugin window exists");
    return w->findChild<QWidget*>("speakerCanvas")->property("speakerCount").toInt();
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
#ifdef _WIN32
    wchar_t runtimePath[4096]{};
    GetModuleFileNameW(GetModuleHandleW(L"Qt5Core.dll"), runtimePath, 4096);
    std::cout << "Qt runtime: " << QString::fromWCharArray(runtimePath).toUtf8().constData() << '\n';
#endif
    check(argc == 2, "DLL path supplied");
    QTemporaryDir temp; check(temp.isValid(), "temporary config directory"); configPath = temp.path();
    clients[{1,1}] = {u8"同名", 0, false, false};
    clients[{1,2}] = {u8"同名", 0, false, false};
    clients[{2,1}] = {"Other server", 0, false, false};
    clients[{1,99}] = {"Me", 0, false, false};
    TS3Functions f{};
    f.freeMemory = release; f.getClientVariableAsString = clientString; f.getServerVariableAsString = serverString;
    f.getClientVariableAsInt = clientInt; f.getClientID = ownId; f.getServerConnectionHandlerList = handlers;
    f.getClientList = clientList; f.isReceivingWhisper = receiving; f.isWhispering = outgoing;
    f.getCurrentServerConnectionHandlerID = currentServer; f.getConfigPath = config; f.logMessage = log;
    QLibrary dll(QString::fromLocal8Bit(argv[1]));
    dll.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    check(dll.load(), qPrintable(dll.errorString()));
    const auto version = resolve<int(*)()>(dll, "ts3plugin_apiVersion");
    auto pointers = resolve<void(*)(TS3Functions)>(dll, "ts3plugin_setFunctionPointers");
    const auto init = resolve<int(*)()>(dll, "ts3plugin_init");
    const auto shutdown = resolve<void(*)()>(dll, "ts3plugin_shutdown");
    const auto talk = resolve<void(*)(uint64,int,int,anyID)>(dll, "ts3plugin_onTalkStatusChangeEvent");
    const auto menu = resolve<void(*)(uint64,PluginMenuType,int,uint64)>(dll, "ts3plugin_onMenuItemEvent");
    const auto menus = resolve<void(*)(PluginMenuItem***,char**)>(dll, "ts3plugin_initMenus");
    const auto freePlugin = resolve<void(*)(void*)>(dll, "ts3plugin_freeMemory");
    const auto command = resolve<int(*)(uint64,const char*)>(dll, "ts3plugin_processCommand");
    const auto tab = resolve<void(*)(uint64)>(dll, "ts3plugin_currentServerConnectionChanged");
    const auto connection = resolve<void(*)(uint64,int,unsigned int)>(dll, "ts3plugin_onConnectStatusChangeEvent");
    const auto update = resolve<void(*)(uint64,anyID,anyID,const char*,const char*)>(dll, "ts3plugin_onUpdateClientEvent");
    const auto move = resolve<void(*)(uint64,anyID,uint64,uint64,int,const char*)>(dll, "ts3plugin_onClientMoveEvent");
    check(version() == 26, "plugin API 26");
    pointers(f); check(init() == 0, "native DLL initialization"); drain();
    check(count() == 0, "no permanent channel list");
    PluginMenuItem** items = nullptr; char* icon = reinterpret_cast<char*>(1); menus(&items, &icon);
    check(items && items[0] && items[1] && items[2] && !items[3] && !icon, "SDK menu memory and icon contract");
    for (int i = 0; items[i]; ++i) freePlugin(items[i]); freePlugin(items);
    std::thread audio([&] { talk(1,STATUS_TALKING,0,1); talk(1,STATUS_TALKING,1,2); }); audio.join();
    check(count() == 0, "worker callbacks enqueue without touching GUI"); drain();
    check(count() == 2, "simultaneous normal and incoming whisper with duplicate names");
    auto* canvas = window()->findChild<QWidget*>("speakerCanvas");
    check(canvas->property("whisperCount").toInt() == 1, "whisper boolean displayed without guessed target");
    talk(1,STATUS_TALKING,1,1); drain();
    check(count() == 2 && canvas->property("whisperCount").toInt() == 2, "normal to whisper replaces same identity");
    talk(1,STATUS_NOT_TALKING,0,2); drain(); check(count() == 1, "whisper stop removes immediately");
    move(1,1,10,0,LEAVE_VISIBILITY,""); drain(); check(count() == 0, "talking client disconnect removes row");
    std::thread rapid([&] { for (int i=0; i<2000; ++i) { talk(1,1,0,1); talk(1,0,0,1); } }); rapid.join();
    drain(); check(count() == 0, "ordered worker PTT callbacks leave no stale rows");
    talk(1,1,0,1); talk(2,1,0,1); drain();
    tab(2); drain(); check(count() == 1, "active server changes");
    connection(2,STATUS_DISCONNECTED,0); drain(); check(count() == 0, "server disconnect removes all its speakers");
    tab(1); drain(); clients[{1,1}].name = "Renamed"; clients[{1,1}].talking = 1;
    update(1,1,0,"",""); drain(); check(count() == 1, "nickname update retains transmitting speaker");
    clients[{1,1}].muted = true; update(1,1,0,"",""); drain(); check(count() == 0, "mute removes speaker");
    clients[{1,1}].muted = false; talk(1,1,0,1); drain();
    check(count() == 1, "new transmitting event after unmute restores activity");
    talk(1,0,0,1); drain();
    talk(1,STATUS_TALKING_WHILE_DISABLED,0,99); drain(); check(count() == 0, "disabled own microphone never shown");
    check(command(1,"hide") == 0, "hide command handled"); drain(); check(!window()->isVisible(), "hide command applied");
    command(1,"show"); drain(); check(window()->isVisible(), "show command applied");
    menu(1,PLUGIN_MENU_TYPE_GLOBAL,2,0); drain();
    QPointer<QWidget> trackedWindow = window();
    QPointer<QWidget> trackedDialog;
    for (auto* w : QApplication::topLevelWidgets()) if (w->objectName() == "whostalkinSettings") trackedDialog = w;
    check(trackedDialog && trackedDialog->isVisible(), "native plugin settings menu");
    // Queue more plugin code, then destroy its receiver without draining it.
    talk(1,1,0,2); command(1,"toggle"); shutdown();
    check(!trackedWindow && !trackedDialog, "shutdown destroys visible GUI and cancels queued callbacks");
    check(allocations == 0 && queriesOnGui, "SDK allocations freed and every SDK lookup ran on GUI thread");
    check(dll.unload(), "DLL actually unloaded"); drain();
    check(dll.load(), "DLL reload"); pointers = resolve<void(*)(TS3Functions)>(dll, "ts3plugin_setFunctionPointers");
    pointers(f);
    const auto reinit = resolve<int(*)()>(dll, "ts3plugin_init");
    const auto reshutdown = resolve<void(*)()>(dll, "ts3plugin_shutdown");
    std::atomic<bool> initialized{false}; int initResult = -1;
    std::thread lifecycleInit([&] { initResult = reinit(); initialized = true; });
    while (!initialized.load()) { QCoreApplication::processEvents(); QThread::msleep(1); }
    lifecycleInit.join(); check(initResult == 0, "worker-thread re-enable without client restart");
    QPointer<QWidget> reloadedWindow = window();
    std::atomic<bool> stopped{false};
    std::thread lifecycleShutdown([&] { reshutdown(); stopped = true; });
    while (!stopped.load()) { QCoreApplication::processEvents(); QThread::msleep(1); }
    lifecycleShutdown.join(); check(!reloadedWindow, "worker-thread shutdown destroys GUI before returning");
    check(dll.unload(), "second unload");
    std::cout << checks << " native plugin host checks passed\n";
}
