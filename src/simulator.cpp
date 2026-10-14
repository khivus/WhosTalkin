#include "controller.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
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
    for (auto& pair : mock->clients) pair.second.uniqueId = "simulated-person-" + std::to_string(pair.first.client);
    const auto settingsPath = preview ? temporary.path() + "/preview.ini"
        : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/simulator.ini";
    wt::Controller controller(std::move(source), settingsPath);
    controller.initialize();
    QMainWindow controls;
    controls.setWindowTitle("WhosTalkin — Event Simulator");
    auto* central = new QWidget(&controls);
    controls.setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    controller.setHostParent(central);
    auto* info = new QLabel("Toggle simulated talk callbacks. The speaker window uses the same UI and state as the plugin.");
    info->setWordWrap(true); layout->addWidget(info);
    auto* server = new QComboBox(&controls);
    server->addItems({"Active server A", "Active server B"}); layout->addWidget(server);
    QObject::connect(server, QOverload<int>::of(&QComboBox::currentIndexChanged), &controls, [&](int index) {
        wt::Event e; e.type = wt::EventType::ActiveServer; e.key.server = index == 0 ? 1 : 2; controller.handle(e);
    });
    std::map<wt::SpeakerKey, QCheckBox*> talkBoxes;
    std::map<wt::SpeakerKey, QCheckBox*> whisperBoxes;
    for (const auto& pair : mock->clients) {
        const auto key = pair.first;
        auto* group = new QGroupBox(QString::fromUtf8(pair.second.nickname.c_str()), &controls);
        auto* row = new QHBoxLayout(group);
        auto* talking = new QCheckBox("Talking", group);
        auto* whisper = new QCheckBox("Whisper", group);
        talkBoxes[key] = talking; whisperBoxes[key] = whisper;
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
        for (const auto& option : std::vector<std::pair<QString, wt::FilterAction>>{
                 {"Include", wt::FilterAction::Include}, {"Exclude", wt::FilterAction::Exclude}, {"Reset", wt::FilterAction::Clear}}) {
            auto* button = new QPushButton(option.first, group); row->addWidget(button);
            QObject::connect(button, &QPushButton::clicked, &controls, [&, key, action = option.second] { controller.filterPerson(key, action); });
        }
        layout->addWidget(group);
    }
    auto* settings = new QPushButton("Settings", &controls); layout->addWidget(settings);
    QObject::connect(settings, &QPushButton::clicked, &controls, [&] { controller.openSettings(); });
    auto* filters = new QPushButton("People filters", &controls); layout->addWidget(filters);
    QObject::connect(filters, &QPushButton::clicked, &controls, [&] { controller.openPeopleFilters(); });
    auto* toggle = new QPushButton("Show / hide speaker window", &controls); layout->addWidget(toggle);
    QObject::connect(toggle, &QPushButton::clicked, &controls, [&] { controller.toggle(); });
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, &QApplication::quit);
    controls.resize(880, 470);
    if (preview) {
        auto s = controller.settings(); s.frameless = true; s.opacity = 100; controller.applySettings(s);
        talkBoxes.at({1,1})->setChecked(true);
        whisperBoxes.at({1,2})->setChecked(true); talkBoxes.at({1,2})->setChecked(true);
        talkBoxes.at({1,3})->setChecked(true);
        if (arguments.contains("--docked")) {
            s = controller.settings(); s.integrate = true; controller.applySettings(s); controls.show();
        }
        if (arguments.contains("--filters")) {
            controller.filterPerson({1,2}, wt::FilterAction::Exclude);
            controller.openPeopleFilters();
        }
        QTimer::singleShot(200, &controls, [&] {
            QWidget* target = controller.window();
            if (arguments.contains("--docked")) target = &controls;
            if (arguments.contains("--filters")) {
                for (auto* widget : QApplication::topLevelWidgets())
                    if (widget->objectName() == "whostalkinPeopleFilters") target = widget;
            }
            const bool saved = target->grab().save(arguments[previewIndex + 1]);
            app.exit(saved ? 0 : 1);
        });
    } else controls.show();
    return app.exec();
}
