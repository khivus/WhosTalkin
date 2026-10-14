#include "controller.h"
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>

using namespace wt;
namespace {
int checks = 0;
void check(bool good, const char* text) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
void wait() { QEventLoop loop; QTimer::singleShot(20, &loop, &QEventLoop::quit); loop.exec(); }
int columns(QWidget* canvas) { return canvas->property("columnCount").toInt(); }
int rows(QWidget* canvas) { return canvas->property("rowCount").toInt(); }
std::vector<Speaker> speakers() {
    const std::vector<std::string> names = {
        "Mashi Rasou", "Shana_Anabel", u8"Алексей · 日本語", "PlayerTwo",
        "A long nickname that should stay inside its own column", "PlayerSix", "My microphone"
    };
    std::vector<Speaker> result;
    for (std::size_t i = 0; i < names.size(); ++i)
        result.push_back({{1, static_cast<std::uint16_t>(i + 1)}, names[i], "Test server A",
                          i == 3 ? SpeechKind::IncomingWhisper : (i == 6 ? SpeechKind::OutgoingWhisper : SpeechKind::Normal),
                          i == 6, i + 1, std::nullopt, "person-" + std::to_string(i)});
    return result;
}
class MockSource : public EventSource {
public:
    ClientInfo client(SpeakerKey key) override {
        const auto s = speakers().at(key.client - 1);
        return {s.nickname, s.serverName, s.own, s.kind == SpeechKind::OutgoingWhisper,
                true, s.kind == SpeechKind::IncomingWhisper, false, s.uniqueId};
    }
    std::vector<Speaker> snapshot(std::optional<std::uint64_t>) override { return {}; }
    std::uint64_t activeServer() override { return 1; }
    void log(const QString&) override {}
};
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary columns settings directory");
    const auto people = speakers();
    {
        SpeakerWindow window;
        Settings s;
        s.maxHeight = 1600;
        window.applySettings(s);
        window.setSpeakers(people, people.size()); window.resize(300, 350); window.showPassive(); wait();
        auto* canvas = window.findChild<QWidget*>("speakerCanvas");
        check(columns(canvas) == 1 && rows(canvas) == 7, "automatic narrow view stays a single list");
        const int narrowHeight = window.height();
        window.resize(800, window.height()); wait();
        check(columns(canvas) == 3 && rows(canvas) == 3, "wide view wraps speakers across three columns");
        check(canvas->height() == 96 && window.height() < narrowHeight, "automatic height counts grid rows when width changes");
        SpeakerKey selected{};
        window.onPersonMenu = [&](SpeakerKey key, const QPoint&) { selected = key; };
        auto click = [&](const QPoint& point) {
            selected = {};
            QContextMenuEvent event(QContextMenuEvent::Mouse, point, canvas->mapToGlobal(point));
            QApplication::sendEvent(canvas, &event);
        };
        const int cellWidth = canvas->width() / columns(canvas);
        click(QPoint(cellWidth + 40, 16));
        check(selected == people[1].key, "second column targets the second speaker in left-to-right order");
        click(QPoint(cellWidth + 40, 48));
        check(selected == people[4].key, "second grid row targets the correct speaker");
        click(QPoint(2 * cellWidth + 40, 80));
        check(selected.client == 0, "unused final-row cell never targets a speaker");
        click(QPoint(canvas->width() - 1, 16));
        check(selected == people[2].key, "last pixel including column-width remainder targets the final column");
        window.resize(530, window.height()); wait();
        check(columns(canvas) == 2 && rows(canvas) == 4, "shrinking a wide view reflows into two columns");
        s.scale = 200; window.applySettings(s); window.resize(800, window.height()); wait();
        check(columns(canvas) == 1 && canvas->height() == 448, "automatic column width respects 200 percent text scale");
        s.scale = 75; window.applySettings(s); window.resize(800, window.height()); wait();
        check(columns(canvas) == 4 && rows(canvas) == 2, "75 percent scale allows more readable columns");
        s.scale = 100; s.columns = 3; window.applySettings(s); window.resize(300, window.height()); wait();
        check(columns(canvas) == 3 && rows(canvas) == 3, "fixed column count overrides automatic width selection");
        window.resize(800, window.height()); wait();
        check(columns(canvas) == 3 && canvas->height() == 96, "fixed columns survive width changes");
        s.columns = 1; window.applySettings(s); wait();
        check(columns(canvas) == 1 && rows(canvas) == 7, "explicit one-column mode retains a wide vertical list");
        s.columns = 0; s.autoHeight = false; window.applySettings(s); window.resize(800, 100); wait();
        auto* scroll = window.findChild<QScrollArea*>("speakerScroll");
        check(columns(canvas) == 3 && scroll->verticalScrollBar()->maximum() > 0,
              "grid overflow uses vertical scrolling at a manual height");
        check(scroll->horizontalScrollBar()->maximum() == 0, "columns fit the viewport without horizontal scrolling");
        scroll->verticalScrollBar()->setValue(32);
        click(QPoint(canvas->width() / columns(canvas) + 40, 48));
        check(selected == people[4].key, "right click retains the correct grid target after scrolling");
        s.stackColumns = true; s.columns = 3;
        window.applySettings(s); window.resize(800, 170); wait();
        check(rows(canvas) == 3, "vertical filling uses the manual window height before another column");
        click(QPoint(40, 48));
        check(selected == people[1].key, "vertical fill reads down the first column");
        click(QPoint(canvas->width() / 3 + 40, 16));
        check(selected == people[3].key, "vertical fill starts the next column after filling the height");
        window.resize(800, 300); wait();
        check(rows(canvas) == 7, "height-only resize stacks more speakers into the first column");
        click(QPoint(canvas->width() / 3 + 40, 16));
        check(selected.client == 0, "tall vertical view leaves the next column empty until needed");
        window.resize(800, 170); wait();
        click(QPoint(2 * (canvas->width() / 3) + 40, 16));
        check(rows(canvas) == 3 && selected == people[6].key, "shrinking height moves overflow to later columns");
        s.autoHeight = true; s.maxHeight = 200; window.applySettings(s); wait();
        check(rows(canvas) == 4 && window.height() <= 200,
              "automatic floating height fills vertically up to its configured maximum");
        click(QPoint(canvas->width() / 3 + 40, 16));
        check(selected == people[4].key, "automatic floating vertical fill has matching context targets");
        s.autoHeight = false; s.columns = 2; window.applySettings(s); window.resize(800, 100); wait();
        check(rows(canvas) == 4 && scroll->verticalScrollBar()->maximum() > 0,
              "vertical stacking extends into scrollable rows when the grid cannot fit all speakers");
        scroll->verticalScrollBar()->setValue(32);
        click(QPoint(canvas->width() / 2 + 40, 48));
        check(selected == people[5].key, "scrolled vertical column retains the correct context target");
        s.stackColumns = false; s.maxHeight = 1600;
        for (const int scale : {75, 100, 200}) {
            s.scale = scale; s.columns = 2; s.allServers = true; window.applySettings(s); wait();
            check(columns(canvas) == 2 && rows(canvas) == 4 && canvas->height() == 4 * qRound(32 * scale / 100.0),
                  "server labels and whisper badges retain scaled grid row geometry");
        }
        s.autoHeight = true; s.stackColumns = true; window.applySettings(s); window.setSpeakers({}, 0); wait();
        auto* header = window.findChild<QWidget*>("speakerTitleBar");
        check(rows(canvas) == 0 && window.rect().contains(QRect(header->mapTo(&window, QPoint()), header->size())),
              "empty multicolumn window preserves the complete header");
    }
    const auto path = directory.path() + "/columns.ini";
    check(SettingsFile(path).load().columns == 0 && !SettingsFile(path).load().stackColumns,
          "existing settings without columns default to Auto and left-to-right filling");
    {
        QMainWindow host;
        host.setCentralWidget(new QWidget);
        host.resize(900, 400); host.show();
        Controller c(std::make_unique<MockSource>(), path);
        c.setHostParent(host.centralWidget()); c.initialize();
        auto s = c.settings(); s.integrate = true; s.dockArea = Qt::BottomDockWidgetArea; s.removalDelay = 5000;
        c.applySettings(s);
        for (const auto& person : people) {
            Event e; e.key = person.key; e.talkStatus = 1;
            e.incomingWhisper = person.kind == SpeechKind::IncomingWhisper; c.handle(e);
        }
        wait();
        host.resizeDocks({c.dock()}, {200}, Qt::Vertical); wait();
        auto* canvas = c.window()->findChild<QWidget*>("speakerCanvas");
        check(c.dock() && c.window()->embedded() && columns(canvas) == 3 && rows(canvas) == 3,
              "real wide Qt dock automatically distributes speakers across columns");
        auto* scroll = c.window()->findChild<QScrollArea*>("speakerScroll");
        check(scroll->viewport()->height() >= canvas->height() && canvas->isVisible(),
              "wide dock displays all grid rows in its allocated space");
        const auto preview = qEnvironmentVariable("WHOSTALKIN_COLUMNS_PREVIEW");
        if (!preview.isEmpty() && !c.dock()->grab().save(preview)) return 1;
        c.openSettings(); wait();
        QDialog* dialog = nullptr;
        for (auto* top : QApplication::topLevelWidgets())
            if (top->objectName() == "whostalkinSettings") dialog = qobject_cast<QDialog*>(top);
        auto* choice = dialog ? dialog->findChild<QSpinBox*>("speakerColumns") : nullptr;
        check(choice && choice->specialValueText() == "Auto" && choice->value() == 0 && choice->maximum() == 8,
              "settings offer Auto and one through eight fixed columns");
        auto* fill = dialog->findChild<QComboBox*>("columnFill");
        check(fill && fill->currentIndex() == 0 && fill->count() == 2, "settings offer both column fill directions");
        choice->setValue(2);
        fill->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); wait();
        check(c.settings().columns == 2 && c.settings().stackColumns && columns(canvas) == 2 && rows(canvas) == 4,
              "saving vertical column fill immediately reflows the live dock");
        SpeakerKey selected{};
        c.window()->onPersonMenu = [&](SpeakerKey key, const QPoint&) { selected = key; };
        auto click = [&](const QPoint& point) {
            selected = {};
            QContextMenuEvent event(QContextMenuEvent::Mouse, point, canvas->mapToGlobal(point));
            QApplication::sendEvent(canvas, &event);
        };
        click(QPoint(canvas->width() / 2 + 40, 16));
        check(selected == people[4].key, "docked vertical fill uses the available height before the next column");
        const auto stackPreview = qEnvironmentVariable("WHOSTALKIN_COLUMNS_STACK_PREVIEW");
        if (!stackPreview.isEmpty() && !c.dock()->grab().save(stackPreview)) return 1;
        host.resize(900, 650); host.resizeDocks({c.dock()}, {400}, Qt::Vertical); wait();
        check(rows(canvas) == 7, "growing dock height stacks all seven speakers in the first column");
        click(QPoint(canvas->width() / 2 + 40, 16));
        check(selected.client == 0, "unused second dock column never opens a person's actions");
        host.resizeDocks({c.dock()}, {200}, Qt::Vertical); wait();
        check(rows(canvas) == 4, "shrinking dock height restores vertical wrapping");
        s = c.settings(); s.maxSpeakers = 4; c.applySettings(s); wait();
        check(canvas->property("speakerCount").toInt() == 4 && rows(canvas) == 4 && c.store().size() == 7,
              "row limit is a total speaker limit and preserves hidden tracking with columns");
        c.filterPerson(people[0].key, FilterAction::Exclude); wait();
        click(QPoint(40, 48));
        check(selected == people[2].key, "grid target follows the displayed filtered order");
        s = c.settings(); s.integrate = false; c.applySettings(s); wait();
        check(!c.dock() && columns(c.window()->findChild<QWidget*>("speakerCanvas")) == 2,
              "fixed column setting also applies when returning to a floating window");
    }
    const auto saved = SettingsFile(path).load();
    check(saved.columns == 2 && saved.stackColumns && saved.removalDelay == 5000,
          "columns, vertical fill and the five-second delay persist together");
    {
        Controller c(std::make_unique<MockSource>(), path); c.initialize();
        check(c.settings().columns == 2 && c.settings().stackColumns, "column choice and vertical fill restore after controller reload");
    }
    std::cout << checks << " columns UI checks passed\n";
}
