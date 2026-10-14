#include "controller.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>
#include <map>

namespace {
class MockSource : public wt::EventSource {
public:
    std::map<wt::SpeakerKey, wt::ClientInfo> clients;
    std::uint64_t active = 1;
    wt::ClientInfo client(wt::SpeakerKey key) override { return clients[key]; }
    std::vector<wt::Speaker> snapshot(std::optional<std::uint64_t>) override { return {}; }
    std::uint64_t activeServer() override { return active; }
    void log(const QString& message) override { qWarning().noquote() << message; }
};
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("WhosTalkin");
    app.setApplicationName("WhosTalkinSimulator");
    const auto arguments = app.arguments();
    const int previewIndex = arguments.indexOf("--preview");
    const bool preview = previewIndex >= 0 && previewIndex + 1 < arguments.size();
    QTemporaryDir temporary;
    auto source = std::make_unique<MockSource>();
    auto* mock = source.get();
    mock->clients[{1,1}] = {"PlayerOne", "Test server A", false, false, true, false, false};
    mock->clients[{1,2}] = {"PlayerTwo", "Test server A", false, false, true, false, false};
    mock->clients[{1,3}] = {u8"Алексей · 日本語", "Test server A", false, false, true, false, false};
    mock->clients[{2,1}] = {"PlayerOne", "Test server B", false, false, true, false, false};
    mock->clients[{1,4}] = {"My microphone", "Test server A", true, false, true, false, false};
    const auto settingsPath = preview ? temporary.path() + "/preview.ini"
        : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/simulator.ini";
    wt::Controller controller(std::move(source), settingsPath);
    controller.initialize();
    QWidget controls;
    controls.setWindowTitle("WhosTalkin — Event Simulator");
    auto* layout = new QVBoxLayout(&controls);
    auto* info = new QLabel("Toggle simulated talk callbacks. The speaker window uses the same UI and state as the plugin.");
    info->setWordWrap(true); layout->addWidget(info);
    auto* server = new QComboBox(&controls);
    server->addItems({"Active server A", "Active server B"}); layout->addWidget(server);
    QObject::connect(server, QOverload<int>::of(&QComboBox::currentIndexChanged), &controls, [&](int index) {
        wt::Event e; e.type = wt::EventType::ActiveServer; e.key.server = index == 0 ? 1 : 2; controller.handle(e);
    });
    for (const auto& pair : mock->clients) {
        const auto key = pair.first;
        auto* group = new QGroupBox(QString::fromUtf8(pair.second.nickname.c_str()), &controls);
        auto* row = new QHBoxLayout(group);
        auto* talking = new QCheckBox("Talking", group);
        auto* whisper = new QCheckBox("Whisper", group);
        row->addWidget(talking); row->addWidget(whisper);
        auto update = [&, key, talking, whisper] {
            auto& c = mock->clients[key]; c.talking = talking->isChecked();
            c.outgoingWhisper = c.own && whisper->isChecked();
            wt::Event e; e.key = key; e.talkStatus = talking->isChecked() ? 1 : 0;
            e.incomingWhisper = !c.own && whisper->isChecked(); controller.handle(e);
        };
        QObject::connect(talking, &QCheckBox::toggled, &controls, update);
        QObject::connect(whisper, &QCheckBox::toggled, &controls, update);
        auto* disconnect = new QPushButton("Disconnect", group); row->addWidget(disconnect);
        QObject::connect(disconnect, &QPushButton::clicked, &controls, [&, key, talking] {
            talking->setChecked(false);
            wt::Event e; e.type = wt::EventType::RemoveClient; e.key = key; controller.handle(e);
        });
        layout->addWidget(group);
    }
    auto* settings = new QPushButton("Settings", &controls); layout->addWidget(settings);
    QObject::connect(settings, &QPushButton::clicked, &controls, [&] { controller.openSettings(); });
    auto* toggle = new QPushButton("Show / hide speaker window", &controls); layout->addWidget(toggle);
    QObject::connect(toggle, &QPushButton::clicked, &controls, [&] { controller.toggle(); });
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, &QApplication::quit);
    controls.resize(530, 360);
    if (preview) {
        auto s = controller.settings(); s.frameless = true; s.opacity = 100; controller.applySettings(s);
        wt::Event e; e.key = {1,1}; e.talkStatus = 1; controller.handle(e);
        e.key = {1,2}; e.incomingWhisper = true; controller.handle(e);
        e.key = {1,3}; e.incomingWhisper = false; controller.handle(e);
        QTimer::singleShot(200, &controls, [&] {
            const bool saved = controller.window()->grab().save(arguments[previewIndex + 1]);
            app.exit(saved ? 0 : 1);
        });
    } else controls.show();
    return app.exec();
}
