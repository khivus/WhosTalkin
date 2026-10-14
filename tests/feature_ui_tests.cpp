#include "controller.h"
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDockWidget>
#include <QEventLoop>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>
#include <map>

using namespace wt;
namespace {
int checks = 0;
void check(bool good, const char* text) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
void wait() { QEventLoop loop; QTimer::singleShot(5, &loop, &QEventLoop::quit); loop.exec(); }
class MockSource : public EventSource {
public:
    std::map<SpeakerKey, ClientInfo> clients;
    int snapshots = 0;
    MockSource() {
        clients[{1,1}] = {"Same", "A", false, false, true, false, false, "identity-A"};
        clients[{1,2}] = {"Same", "A", false, false, true, false, false, "identity-B"};
        clients[{1,3}] = {"No UID", "A", false, false, true, false, false, ""};
        clients[{1,4}] = {"Same person B", "A", false, false, true, false, false, "identity-B"};
        clients[{2,10}] = {"Person A reconnected", "B", false, false, true, false, false, "identity-A"};
    }
    ClientInfo client(SpeakerKey key) override {
        const auto it = clients.find(key);
        if (it != clients.end()) return it->second;
        ClientInfo absent; absent.known = false; return absent;
    }
    std::vector<Speaker> snapshot(std::optional<std::uint64_t>) override { ++snapshots; return {}; }
    std::uint64_t activeServer() override { return 1; }
    void log(const QString&) override {}
};
int count(Controller& c) { return c.window()->findChild<QWidget*>("speakerCanvas")->property("speakerCount").toInt(); }
void talk(Controller& c, SpeakerKey key, int status = 1) { Event e; e.key = key; e.talkStatus = status; c.handle(e); }
void remove(Controller& c, SpeakerKey key) { Event e; e.type = EventType::RemoveClient; e.key = key; c.handle(e); }
QWidget* top(const char* name) {
    for (auto* widget : QApplication::topLevelWidgets()) if (widget->objectName() == name) return widget;
    return nullptr;
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary filter settings directory");
    const auto filterPath = directory.path() + "/filters.ini";
    {
        auto source = std::make_unique<MockSource>(); auto* mock = source.get();
        auto c = std::make_unique<Controller>(std::move(source), filterPath); c->initialize();
        talk(*c, {1,1}); talk(*c, {1,2});
        check(count(*c) == 2, "duplicate names start as separate people");
        check(c->filterPerson({1,1}, FilterAction::Exclude) && count(*c) == 1 && c->store().size() == 2,
              "exclusion hides the row and retains active event state");
        auto s = c->settings(); s.includeOnly = true; c->applySettings(s);
        check(count(*c) == 0, "include-only hides people without include rules");
        c->filterPerson({1,2}, FilterAction::Include); check(count(*c) == 1, "include action immediately reveals an active person");
        c->filterPerson({1,2}, FilterAction::Clear); check(count(*c) == 0, "clear rule follows mode default");
        s = c->settings(); s.includeOnly = false; s.allServers = true; c->applySettings(s);
        talk(*c, {2,10}); check(count(*c) == 1, "UID exclusion follows the same person across servers and client IDs");
        mock->clients[{1,1}].nickname = "Renamed";
        Event changed; changed.type = EventType::RefreshClient; changed.key = {1,1}; c->handle(changed);
        check(c->settings().people.at("identity-A").nickname == "Renamed", "saved identity label follows nickname change");
        remove(*c, {1,1}); mock->clients[{1,1}].uniqueId = "identity-C"; talk(*c, {1,1});
        check(count(*c) == 2, "old numeric client ID assigned to a new identity is not excluded");
        c->filterPerson({1,1}, FilterAction::Include);
        c->filterPerson({1,3}, FilterAction::Exclude); talk(*c, {1,3});
        check(count(*c) == 2 && !c->settings().people.count(""), "UID-unavailable rules are connection-only");
        remove(*c, {1,3}); talk(*c, {1,3});
        check(count(*c) == 3, "session exclusion is cleared before client ID reuse");
        remove(*c, {1,1}); remove(*c, {1,3});
        auto* canvas = c->window()->findChild<QWidget*>("speakerCanvas");
        QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(40, 10), canvas->mapToGlobal(QPoint(40, 10)));
        QApplication::sendEvent(canvas, &context);
        auto* menu = c->window()->findChild<QMenu*>("speakerPersonMenu");
        check(menu && menu->isVisible(), "right click on the actual canvas opens person actions");
        QAction* exclude = nullptr;
        for (auto* action : menu->actions()) if (action->text() == "Exclude this person") exclude = action;
        check(exclude, "canvas has exclude action");
        remove(*c, {1,2}); mock->clients[{1,2}].uniqueId = "identity-D"; talk(*c, {1,2});
        exclude->trigger();
        check(c->settings().people.count("identity-B") && !c->settings().people.count("identity-D") && count(*c) == 1,
              "open menu keeps the original UID even when its numeric client ID is reused");
        menu->close(); wait();
        c->filterPerson({1,4}, FilterAction::Include); // Connected but not transmitting.
        s = c->settings(); s.includeOnly = true; c->applySettings(s);
        check(count(*c) == 0, "including a quiet person never creates an inactive row");
        talk(*c, {1,4}); check(count(*c) == 1, "included person appears when a talk event arrives");
        talk(*c, {1,4}, 0); check(count(*c) == 0, "included person disappears on stop");
        check(!c->filterPerson({1,999}, FilterAction::Exclude), "disconnected unknown selection is ignored");
        check(mock->snapshots == 1, "filter actions never poll channel members");
        c->openPeopleFilters(); wait();
        auto* manager = top("whostalkinPeopleFilters");
        check(manager && manager->isVisible(), "manager is accessible even when the speaker list is empty");
        auto* table = manager->findChild<QTableWidget*>("peopleFiltersTable");
        check(table && table->rowCount() == 3, "manager displays saved rules, including quiet excluded people");
        auto* mode = manager->findChild<QComboBox*>("peopleFilterMode");
        mode->setCurrentIndex(0); check(!c->settings().includeOnly && count(*c) == 1, "manager changes filter mode immediately");
        mode->setCurrentIndex(1);
        QPointer<QWidget> managerReference = manager;
        c.reset(); check(!managerReference, "unload destroys filter manager synchronously");
    }
    {
        SettingsFile file(filterPath); const auto saved = file.load();
        check(saved.includeOnly && saved.people.at("identity-A").action == FilterAction::Exclude &&
              saved.people.at("identity-B").action == FilterAction::Include, "stable rules and mode round-trip through INI");
        auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), filterPath); c->initialize();
        talk(*c, {1,1}); talk(*c, {1,2}); check(count(*c) == 1, "reloaded filters distinguish identical nicknames");
    }
    {
        auto host = std::make_unique<QMainWindow>();
        auto* center = new QWidget; host->setCentralWidget(center);
        auto* other = new QDockWidget("Existing dock", host.get());
        other->setWidget(new QLabel("Existing content")); host->addDockWidget(Qt::LeftDockWidgetArea, other);
        host->resize(900, 650); host->show();
        auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), directory.path() + "/dock.ini");
        c->initialize(); talk(*c, {1,1}); wait();
        const auto floatingWidth = c->window()->width();
        auto s = c->settings(); s.integrate = true; c->applySettings(s); wait();
        check(c->dock() && c->window()->embedded() && !c->window()->isWindow() && c->dock()->widget() == c->window(),
              "opt-in integration creates a real Qt dock containing the speaker widget");
        check(host->centralWidget() == center && host->dockWidgetArea(other) == Qt::LeftDockWidgetArea,
              "embedding retains host content and existing dock placement");
        check(count(*c) == 1, "docked view uses the same current speaker state");
        c->hide(); check(c->dock()->isHidden(), "hide action hides the whole dock");
        c->show(); check(!c->dock()->isHidden() && c->window()->isVisible(), "reopen shows dock and its content");
        c->dock()->close(); c->toggle(); check(!c->dock()->isHidden(), "native dock close can be undone with the plugin toggle");
        host->addDockWidget(Qt::BottomDockWidgetArea, c->dock()); wait();
        check(c->settings().dockArea == Qt::BottomDockWidgetArea, "only the plugin's dock area is persisted");
        s = c->settings(); s.integrate = false; c->applySettings(s); wait();
        check(!c->dock() && c->window()->isWindow() && !c->window()->embedded() && c->window()->width() == floatingWidth,
              "returning to floating mode restores independent geometry");
        s = c->settings(); s.integrate = true; c->applySettings(s); wait();
        check(host->dockWidgetArea(c->dock()) == Qt::BottomDockWidgetArea, "re-embedding restores the plugin dock area");
        s = c->settings(); s.scale = 75; c->applySettings(s);
        s = c->settings(); s.integrate = false; c->applySettings(s); wait();
        check(c->window()->width() == qRound(floatingWidth * 0.75),
              "scale changed while docked also scales the restored floating width");
        s = c->settings(); s.integrate = true; c->applySettings(s); wait();
        QPointer<QWidget> oldView = c->window(); QPointer<QDockWidget> oldDock = c->dock();
        host.reset(); wait();
        check(!oldView && !oldDock && c->window() && !c->dock() && c->window()->isWindow() && count(*c) == 1,
              "host deletion destroys host-owned widgets and safely recreates the floating view");
        QPointer<QWidget> fallback = c->window(); c.reset(); check(!fallback, "fallback view is destroyed on plugin unload");
    }
    {
        QMainWindow first, second;
        first.setCentralWidget(new QWidget); second.setCentralWidget(new QWidget);
        auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), directory.path() + "/ambiguous.ini");
        c->initialize(); auto s = c->settings(); s.integrate = true; c->applySettings(s);
        check(!c->dock() && c->window()->isWindow(), "ambiguous host main windows keep a floating fallback");
        c->setHostParent(first.centralWidget()); wait();
        check(c->dock() && first.dockWidgetArea(c->dock()) != Qt::NoDockWidgetArea,
              "official configure parent can resolve the intended host safely");
        QPointer<QDockWidget> dock = c->dock(); QPointer<QWidget> view = c->window();
        c->openPeopleFilters(); c->openSettings();
        c.reset(); check(!dock && !view && first.centralWidget(), "unload removes visible dock without destroying host");
    }
    {
        auto previous = std::make_unique<QMainWindow>(); QMainWindow replacement;
        previous->setCentralWidget(new QWidget); replacement.setCentralWidget(new QWidget);
        auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), directory.path() + "/replace-host.ini");
        c->initialize(); c->setHostParent(previous->centralWidget()); c->setHostParent(replacement.centralWidget());
        previous.reset(); wait();
        auto s = c->settings(); s.integrate = true; c->applySettings(s);
        check(c->dock() && replacement.dockWidgetArea(c->dock()) != Qt::NoDockWidgetArea,
              "destroying a previous host does not invalidate a newly supplied host");
    }
    for (const auto area : {Qt::BottomDockWidgetArea, Qt::RightDockWidgetArea}) {
        const bool horizontal = area == Qt::BottomDockWidgetArea;
        const auto orientation = horizontal ? Qt::Vertical : Qt::Horizontal;
        const auto path = directory.path() + QString("/dock-size-%1.ini").arg(static_cast<int>(area));
        auto thickness = [horizontal](QDockWidget* dock) { return horizontal ? dock->height() : dock->width(); };
        int saved = 0;
        {
            QMainWindow host; host.setCentralWidget(new QWidget); host.resize(900, 650); host.show();
            auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), path); c->initialize();
            auto s = c->settings(); s.integrate = true; s.dockArea = area; c->applySettings(s); wait();
            const int wanted = horizontal ? 220 : 310;
            host.resizeDocks({c->dock()}, {wanted}, orientation); wait();
            saved = thickness(c->dock());
            check(qAbs(saved - wanted) <= 2, "manual dock resize sets the requested width or height");
            c->hide(); c->show(); wait();
            check(thickness(c->dock()) == saved, "hide and reopen retain the dock's dimensions");
            c.reset(); // Unload before a debounced geometry save could fire.
        }
        {
            // Recreate both objects, as happens during a complete TS restart.
            // The new main window starts hidden and has different dimensions.
            auto host = std::make_unique<QMainWindow>();
            auto* center = new QWidget; host->setCentralWidget(center);
            auto* other = new QDockWidget("Unrelated panel", host.get()); other->setWidget(new QLabel("Retained"));
            host->addDockWidget(Qt::LeftDockWidgetArea, other); host->resize(1100, 780);
            auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), path); c->initialize();
            host->show(); wait();
            check(c->dock() && host->dockWidgetArea(c->dock()) == area && qAbs(thickness(c->dock()) - saved) <= 2,
                  "full restart restores dock area and dimensions after the host is shown");
            check(host->centralWidget() == center && host->dockWidgetArea(other) == Qt::LeftDockWidgetArea,
                  "dock size restoration retains unrelated host content and dock placement");
            host->resizeDocks({c->dock()}, {saved + 25}, orientation); wait(); saved = thickness(c->dock());
            host.reset(); wait(); // Host children can disappear before plugin shutdown.
            c.reset();
            QSettings file(path, QSettings::IniFormat);
            const auto stored = file.value("dockSize").toSize();
            check(stored.isValid() && (horizontal ? stored.height() : stored.width()) == saved,
                  "last dock resize survives host destruction before the save timer fires");
        }
        {
            QMainWindow host; host.setCentralWidget(new QWidget); host.resize(1000, 700); host.show();
            auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), path); c->initialize(); wait();
            check(c->dock() && qAbs(thickness(c->dock()) - saved) <= 2,
                  "restart restores the latest size even when the previous host was destroyed first");
        }
        {
            SettingsFile file(path); auto s = file.load(); s.startVisible = false; file.save(s); file.sync();
        }
        {
            QMainWindow host; host.setCentralWidget(new QWidget); host.resize(1000, 700); host.show();
            auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), path); c->initialize(); wait();
            check(c->dock() && c->dock()->isHidden(), "saved hidden startup does not open the dock");
            c->show(); wait();
            check(qAbs(thickness(c->dock()) - saved) <= 2, "first show after a hidden startup restores saved dock size");
        }
        {
            SettingsFile file(path); auto s = file.load(); s.startVisible = true; file.save(s); file.sync();
        }
        {
            QMainWindow host; host.setCentralWidget(new QWidget); host.resize(1000, 700); host.showMinimized();
            auto c = std::make_unique<Controller>(std::make_unique<MockSource>(), path); c->initialize(); wait();
            host.showNormal(); wait();
            check(c->dock() && qAbs(thickness(c->dock()) - saved) <= 2,
                  "restoring a minimized startup window applies saved dock dimensions");
        }
    }
    std::cout << checks << " filter and docking UI checks passed\n";
}
