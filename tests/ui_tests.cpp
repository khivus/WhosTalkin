#include "controller.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <cstdlib>
#include <iostream>
#include <map>

using namespace wt;
namespace {
int checks = 0;
void check(bool good, const char* description) {
    ++checks;
    if (!good) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
class MockSource : public EventSource {
public:
    std::map<SpeakerKey, ClientInfo> clients;
    ClientInfo client(SpeakerKey key) override { return clients.at(key); }
    std::vector<Speaker> snapshot(std::optional<std::uint64_t>) override { return {}; }
    std::uint64_t activeServer() override { return 1; }
    void log(const QString&) override {}
};
void wait(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid(), "temporary settings directory");
    {
        // An empty scroll area must not consume its default minimum viewport
        // height and squeeze the header out of a compact window.
        SpeakerWindow window;
        Settings settings;
        window.applySettings(settings); window.showPassive(); wait(5);
        auto* header = window.findChild<QWidget*>("speakerTitleBar");
        check(window.rect().contains(QRect(header->mapTo(&window, QPoint()), header->size())),
              "empty auto-height window contains the complete header");
        check(header->height() >= header->layout()->minimumSize().height(),
              "empty header has enough height for all controls");
        const int normalWidth = window.width();
        settings.scale = 75; window.applySettings(settings); wait(5);
        check(window.width() < normalWidth, "75 percent scale also reduces window width");
        auto* label = window.findChild<QLabel*>("speakerTitle");
        const auto normalFont = label->font().pointSizeF();
        app.setStyleSheet("QWidget { font-size: 14pt; } QLabel { padding: 5px; } QToolButton { min-height: 35px; padding: 8px; }");
        window.applySettings(settings); wait(5);
        check(label->font().pointSizeF() <= 7.0 && label->font().pointSizeF() <= normalFont,
              "75 percent font survives a host TeamSpeak stylesheet");
        check(header->height() >= header->layout()->minimumSize().height() &&
              window.rect().contains(QRect(header->mapTo(&window, QPoint()), header->size())),
              "host stylesheet cannot clip empty header controls");
        const QString images = qEnvironmentVariable("WHOSTALKIN_TEST_IMAGES");
        if (!images.isEmpty()) QDir().mkpath(images);
        const std::vector<Speaker> speakers = {
            {{1,1}, "PlayerOne", "A", SpeechKind::Normal, false, 1, std::nullopt},
            {{1,2}, "PlayerTwo", "A", SpeechKind::IncomingWhisper, false, 2, std::nullopt}
        };
        for (const bool frameless : {false, true}) {
            settings.frameless = frameless;
            for (const int scale : {75, 90, 100, 150, 200}) {
                settings.scale = scale; window.applySettings(settings);
                window.setSpeakers(speakers, speakers.size()); wait(5);
                const int activeHeight = window.height();
                check(header->height() >= header->layout()->minimumSize().height(),
                      "active header fits at each scale in framed and frameless modes");
                check(qAbs(label->font().pointSizeF() - 9.0 * scale / 100.0) < 0.05,
                      "all supported scales override host font size");
                if (!images.isEmpty()) window.grab().save(images + QString("/active-%1-%2.png").arg(scale).arg(frameless));
                window.setSpeakers({}, 0); wait(5);
                bool contained = window.rect().contains(QRect(header->mapTo(&window, QPoint()), header->size()));
                for (auto* button : header->findChildren<QToolButton*>())
                    contained = contained && header->rect().contains(QRect(button->mapTo(header, QPoint()), button->size()));
                check(contained && label->contentsRect().height() >= label->fontMetrics().height(),
                      "last speaker stopping leaves complete header text and buttons at each scale");
                check(window.height() < activeHeight && !window.findChild<QWidget*>("speakerScroll")->isVisible(),
                      "silence collapses only the list area at each scale");
                if (!images.isEmpty()) window.grab().save(images + QString("/empty-%1-%2.png").arg(scale).arg(frameless));
            }
        }
        settings.autoHeight = false; window.applySettings(settings); window.resize(600, 400);
        settings.scale = 100; window.applySettings(settings); wait(5);
        check(window.size() == QSize(300, 200), "manual window dimensions scale proportionally too");
        app.setStyleSheet("");
    }
    auto source = std::make_unique<MockSource>();
    auto* mock = source.get();
    mock->clients[{1,1}] = {u8"同名", "A", false, false, true, false, false};
    mock->clients[{1,2}] = {u8"同名", "A", false, false, true, false, false};
    mock->clients[{2,1}] = {"Other", "B", false, false, true, false, false};
    mock->clients[{1,3}] = {"Me", "A", true, false, true, false, false};
    auto c = std::make_unique<Controller>(std::move(source), dir.path() + "/test.ini");
    c->initialize();
    auto* canvas = c->window()->findChild<QWidget*>("speakerCanvas");
    auto count = [&] { return canvas->property("speakerCount").toInt(); };
    check(count() == 0 && c->window()->isVisible(), "visible window starts empty");
    auto talk = [&](SpeakerKey key, int status, bool whisper = false) {
        Event e; e.key = key; e.talkStatus = status; e.incomingWhisper = whisper; c->handle(e);
    };
    talk({1,1}, 1); check(count() == 1, "normal start creates row");
    talk({1,2}, 1); check(count() == 2, "same nickname does not merge rows");
    talk({1,1}, 1, true);
    check(count() == 2 && canvas->property("whisperCount").toInt() == 1, "normal to whisper updates same row without target");
    talk({1,1}, 0, false); check(count() == 1, "stop removes whisper regardless of flag");
    Event e; e.type = EventType::RemoveClient; e.key = {1,2}; c->handle(e);
    check(count() == 0, "disconnect while talking");
    for (int i = 0; i < 2000; ++i) { talk({1,1}, 1); talk({1,1}, 0); }
    check(count() == 0, "rapid PTT does not leave duplicate or inactive rows");
    talk({1,1}, 1); talk({2,1}, 1);
    e.type = EventType::ActiveServer; e.key.server = 2; c->handle(e);
    check(count() == 1, "server tab switch filters existing activity");
    auto s = c->settings(); s.allServers = true; s.maxSpeakers = 1; c->applySettings(s);
    check(count() == 1 && c->store().size() == 2, "maximum rows does not discard tracked activity");
    s.maxSpeakers = 12; s.removalDelay = 100; c->applySettings(s);
    talk({1,1}, 0); talk({1,1}, 1); wait(130);
    check(count() == 2, "restarted PTT cancels actual Qt expiry timer");
    talk({1,1}, 0); wait(130); check(count() == 1, "Qt timer removes expired row");
    talk({1,3}, 2); check(!c->store().find({1,3}), "STATUS_TALKING_WHILE_DISABLED never shown");
    talk({1,3}, 1); mock->clients[{1,3}].suppressed = true;
    e.type = EventType::RefreshClient; e.key = {1,0}; c->handle(e);
    check(!c->store().find({1,3}), "self mute clears own row through self update");
    c->hide(); check(!c->window()->isVisible(), "hide without stopping tracking");
    c->show(); check(c->window()->isVisible() && count() == 1, "reopen contains only current activity");
    s.frameless = true; s.darkTheme = false; s.opacity = 70; s.scale = 150; s.maxHeight = 140;
    c->applySettings(s);
    check(c->window()->height() <= 140 && c->window()->windowFlags().testFlag(Qt::FramelessWindowHint), "maximum height and frameless settings");
    c->openSettings();
    QPointer<QWidget> window = c->window();
    QPointer<QWidget> dialog;
    for (auto* w : QApplication::topLevelWidgets()) if (w->objectName() == "whostalkinSettings") dialog = w;
    check(dialog && dialog->isVisible(), "settings opened");
    c.reset(); check(!window && !dialog, "unload destroys visible window and settings synchronously");
    SettingsFile file(dir.path() + "/test.ini");
    const auto saved = file.load();
    check(saved.frameless && saved.opacity == 70 && saved.allServers && !file.geometry().isEmpty(), "settings and geometry restored from disk");
    std::cout << checks << " UI checks passed\n";
}
