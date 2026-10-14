#include "controller.h"
#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace wt {
Controller::Controller(std::unique_ptr<EventSource> source, const QString& path)
    : source_(std::move(source)), settingsFile_(path), settings_(settingsFile_.load()) {
    clock_.start();
    expiry_.setSingleShot(true);
    expiry_.setTimerType(Qt::PreciseTimer);
    connect(&expiry_, &QTimer::timeout, this, [this] {
        store_.expire(clock_.elapsed()); render(); armExpiry();
    });
    filter_.configure(settings_.includeOnly, settings_.people);
    floatingGeometry_ = settingsFile_.geometry();
    floatingSize_ = settingsFile_.geometrySize();
    floatingScale_ = settingsFile_.geometryScale();
    dockSize_ = settingsFile_.dockSize();
    dockSizeSave_.setSingleShot(true);
    dockSizeSave_.setInterval(250);
    connect(&dockSizeSave_, &QTimer::timeout, this, [this] { saveGeometry(); });
    ensureWindow();
}
Controller::~Controller() {
    shuttingDown_ = true;
    expiry_.stop();
    if (dialog_) delete dialog_.data();
    if (peopleDialog_) delete peopleDialog_.data();
    saveGeometry();
    if (dock_) {
        if (host_) host_->removeDockWidget(dock_);
        delete dock_.data(); // Also deletes the embedded speaker widget.
    }
    delete window_.data(); // QPointer is null if the host already deleted it.
    store_.clear();
}
void Controller::initialize() {
    activeServer_ = source_->activeServer();
    seed(); render();
    updatePresentation();
    if (settings_.startVisible) show();
}
void Controller::seed(std::optional<std::uint64_t> server) {
    for (const auto& s : source_->snapshot(server))
        store_.start(s.key, s.nickname, s.serverName, s.kind, s.own, s.uniqueId);
}
void Controller::handle(const Event& event) {
    switch (event.type) {
    case EventType::Talk:
        if (event.talkStatus == 1) { // STATUS_TALKING; status 2 is local talking while disabled.
            const auto info = source_->client(event.key);
            if (info.suppressed) { store_.remove(event.key); break; }
            const auto kind = event.incomingWhisper ? SpeechKind::IncomingWhisper
                : (info.own && info.outgoingWhisper ? SpeechKind::OutgoingWhisper : SpeechKind::Normal);
            store_.start(event.key, info.nickname, info.serverName, kind, info.own, info.uniqueId);
            if (filter_.rename(info.uniqueId, info.nickname)) persistFilters();
        } else {
            // Muted local speech and disconnects must never linger, even with a delay enabled.
            store_.stop(event.key, clock_.elapsed(), event.talkStatus == 0 ? settings_.removalDelay : 0);
        }
        break;
    case EventType::RemoveClient:
        store_.remove(event.key); filter_.removeClient(event.key); refreshPeopleDialog(); break;
    case EventType::ClearServer:
        store_.removeServer(event.key.server); filter_.removeServer(event.key.server); refreshPeopleDialog(); break;
    case EventType::ClientMoved: {
        const auto info = source_->client(event.key);
        if (info.own) {
            store_.removeNormalOnServer(event.key.server);
            seed(event.key.server); // Once per local channel change; never periodic polling.
        } else if (const auto* s = store_.find(event.key)) {
            if (s->kind == SpeechKind::Normal && event.oldChannel != event.newChannel)
                store_.remove(event.key);
        }
        break;
    }
    case EventType::RefreshClient: {
        std::vector<SpeakerKey> keys;
        if (event.key.client == 0) {
            for (const auto& s : store_.visible(event.key.server, false, true))
                if (s.own) keys.push_back(s.key);
        } else if (store_.find(event.key)) keys.push_back(event.key);
        for (const auto key : keys) {
            const auto info = source_->client(key);
            if (info.suppressed || (info.talking && !*info.talking)) store_.remove(key);
            else {
                store_.rename(key, info.nickname);
                if (filter_.rename(info.uniqueId, info.nickname)) persistFilters();
            }
        }
        break;
    }
    case EventType::Rename: store_.rename(event.key, event.nickname); break;
    case EventType::ActiveServer: activeServer_ = event.key.server; break;
    case EventType::Connected:
        store_.removeServer(event.key.server); filter_.removeServer(event.key.server);
        seed(event.key.server); refreshPeopleDialog(); break;
    }
    render(); armExpiry();
}
void Controller::render() {
    ensureWindow();
    auto speakers = store_.visible(activeServer_, settings_.allServers, settings_.showOwn);
    speakers.erase(std::remove_if(speakers.begin(), speakers.end(), [this](const Speaker& s) {
        return !filter_.allows(s);
    }), speakers.end());
    if (settings_.alphabetical) {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(speakers.begin(), speakers.end(), [&collator](const Speaker& a, const Speaker& b) {
            const int order = collator.compare(QString::fromUtf8(a.nickname.c_str()), QString::fromUtf8(b.nickname.c_str()));
            return order == 0 ? a.key < b.key : order < 0;
        });
    } else {
        std::sort(speakers.begin(), speakers.end(), [](const Speaker& a, const Speaker& b) {
            return a.sequence < b.sequence;
        });
    }
    const auto total = speakers.size();
    if (total > static_cast<std::size_t>(settings_.maxSpeakers)) speakers.resize(settings_.maxSpeakers);
    window_->setSpeakers(speakers, total);
}
void Controller::armExpiry() {
    const auto next = store_.nextExpiry();
    if (!next) { expiry_.stop(); return; }
    expiry_.start(static_cast<int>(std::clamp<std::int64_t>(*next - clock_.elapsed(), 1, Settings::maxRemovalDelay)));
}
void Controller::applySettings(Settings settings) {
    settings.normalize(); settings_ = settings;
    filter_.configure(settings_.includeOnly, settings_.people);
    settingsFile_.save(settings_);
    if (settings_.removalDelay == 0) store_.expire(INT64_MAX);
    ensureWindow(); window_->applySettings(settings_);
    updatePresentation();
    render(); armExpiry(); saveGeometry(); refreshPeopleDialog();
}
void Controller::openSettings() {
    if (dialog_) { dialog_->show(); dialog_->raise(); dialog_->activateWindow(); return; }
    // Parentless so minimizing TS3 cannot minimize the utility or its settings.
    auto* dialog = new QDialog(nullptr);
    dialog_ = dialog;
    dialog->setObjectName("whostalkinSettings");
    dialog->setWindowTitle("WhosTalkin Settings");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_QuitOnClose, false);
    auto* layout = new QVBoxLayout(dialog);
    auto* tabs = new QTabWidget(dialog);
    layout->addWidget(tabs);
    auto* windowPage = new QWidget(tabs);
    auto* windowLayout = new QVBoxLayout(windowPage);
    auto* windowGroup = new QGroupBox("Window", dialog);
    auto* windowForm = new QFormLayout(windowGroup);
    auto checkbox = [dialog](QFormLayout* form, const QString& text, bool value) {
        auto* box = new QCheckBox(text, dialog); box->setChecked(value); form->addRow(box); return box;
    };
    auto spin = [dialog](QFormLayout* form, const QString& label, int value, int min, int max, const QString& suffix) {
        auto* box = new QSpinBox(dialog); box->setRange(min, max); box->setValue(value);
        box->setSuffix(suffix); form->addRow(label, box); return box;
    };
    auto* display = new QComboBox(dialog);
    display->setObjectName("displayMode");
    display->addItems({"Independent floating window", "Dock in TeamSpeak window (experimental)"});
    display->setCurrentIndex(settings_.integrate ? 1 : 0);
    windowForm->addRow("Display", display);
    auto* hostNote = new QLabel("Docking uses a compatible Qt main window when available. Otherwise the floating window remains available.", dialog);
    hostNote->setWordWrap(true);
    windowForm->addRow(hostNote);
    auto* top = checkbox(windowForm, "Always on top", settings_.alwaysOnTop);
    auto* start = checkbox(windowForm, "Open when plugin starts", settings_.startVisible);
    auto* taskbar = checkbox(windowForm, "Hide from taskbar", settings_.hideTaskbar);
    auto* frameless = checkbox(windowForm, "Frameless", settings_.frameless);
    auto* dark = checkbox(windowForm, "Dark theme", settings_.darkTheme);
    auto* autoHeight = checkbox(windowForm, "Adjust height to speakers", settings_.autoHeight);
    auto* opacity = spin(windowForm, "Opacity", settings_.opacity, 20, 100, "%");
    auto* scale = spin(windowForm, "Scale", settings_.scale, 75, 200, "%");
    auto* height = spin(windowForm, "Maximum height", settings_.maxHeight, 120, 1600, " px");
    windowLayout->addWidget(windowGroup);
    windowLayout->addStretch();
    tabs->addTab(windowPage, "Window");
    auto* speakerPage = new QWidget(tabs);
    auto* speakerLayout = new QVBoxLayout(speakerPage);
    auto* speakersGroup = new QGroupBox("Speakers", dialog);
    auto* form = new QFormLayout(speakersGroup);
    auto* activeOnly = checkbox(form, "Speaking users (plus configured removal delay)", true);
    activeOnly->setEnabled(false);
    auto* whisper = checkbox(form, "Show whisper indicator", settings_.showWhisper);
    auto* target = checkbox(form, "Show whisper target (unavailable in API 26)", false);
    target->setEnabled(false);
    target->setToolTip("The receiving SDK exposes a whisper boolean, without a target type or ID.");
    auto* own = checkbox(form, "Show my microphone activity", settings_.showOwn);
    auto* max = spin(form, "Maximum displayed", settings_.maxSpeakers, 1, 100, "");
    auto* columns = spin(form, "Columns", settings_.columns, 0, Settings::maxColumns, "");
    columns->setObjectName("speakerColumns");
    columns->setSpecialValueText("Auto");
    columns->setToolTip("Auto fits columns to the available width and scale. Choose 1 for a single list, or 2–8 for a fixed count.");
    auto* fill = new QComboBox(dialog);
    fill->setObjectName("columnFill");
    fill->addItems({"Left to right, then down", "Top to bottom, then right"});
    fill->setCurrentIndex(settings_.stackColumns ? 1 : 0);
    fill->setToolTip("Top to bottom fills the available dock/window height before using the next column. With automatic floating height, it fills up to the maximum height. Overflow scrolls vertically.");
    form->addRow("Column fill", fill);
    auto* order = new QComboBox(dialog);
    order->addItems({"Speaking start time", "Alphabetical"});
    order->setCurrentIndex(settings_.alphabetical ? 1 : 0);
    form->addRow("Order", order);
    auto* filters = new QPushButton("People filters...", dialog);
    filters->setObjectName("peopleFiltersButton");
    form->addRow(filters);
    connect(filters, &QPushButton::clicked, this, [this] { openPeopleFilters(); });
    speakerLayout->addWidget(speakersGroup);
    auto* behavior = new QGroupBox("Behavior", dialog);
    auto* behaviorForm = new QFormLayout(behavior);
    auto* servers = checkbox(behaviorForm, "All connected servers (available audio events only)", settings_.allServers);
    auto* delay = spin(behaviorForm, "Removal delay", settings_.removalDelay, 0, Settings::maxRemovalDelay, " ms");
    delay->setObjectName("removalDelay");
    delay->setToolTip("0 removes immediately. Up to 5000 ms (5 seconds) keeps a stopped speaker visible so you can notice brief speech. Disconnects and muting remove immediately.");
    speakerLayout->addWidget(behavior);
    speakerLayout->addStretch();
    tabs->addTab(speakerPage, "Speakers");
    auto enableFloating = [=](int mode) {
        for (auto* widget : std::vector<QWidget*>{top, taskbar, frameless, autoHeight, opacity, height})
            widget->setEnabled(mode == 0);
    };
    connect(display, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, enableFloating);
    enableFloating(display->currentIndex());
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=] {
        auto s = settings_;
        s.integrate = display->currentIndex() == 1;
        s.alwaysOnTop = top->isChecked(); s.startVisible = start->isChecked();
        s.hideTaskbar = taskbar->isChecked(); s.frameless = frameless->isChecked();
        s.darkTheme = dark->isChecked(); s.autoHeight = autoHeight->isChecked();
        s.opacity = opacity->value(); s.scale = scale->value(); s.maxHeight = height->value();
        s.showWhisper = whisper->isChecked(); s.showOwn = own->isChecked();
        s.maxSpeakers = max->value(); s.columns = columns->value(); s.alphabetical = order->currentIndex() == 1;
        s.stackColumns = fill->currentIndex() == 1;
        s.allServers = servers->isChecked(); s.removalDelay = delay->value();
        applySettings(s); dialog->accept();
    });
    dialog->show();
}
}
