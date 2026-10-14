#pragma once
#include "event_source.h"
#include "settings.h"
#include "speaker_window.h"
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>

class QDialog;
namespace wt {
class Controller : public QObject {
public:
    Controller(std::unique_ptr<EventSource> source, const QString& settingsPath);
    ~Controller() override;
    void initialize();
    void handle(const Event& event);
    void show();
    void hide();
    void toggle();
    void openSettings();
    void resetPosition();
    void applySettings(Settings settings);
    SpeakerWindow* window() const { return window_.get(); }
    const SpeakerStore& store() const { return store_; }
    const Settings& settings() const { return settings_; }
private:
    void render();
    void armExpiry();
    void saveGeometry();
    void seed(std::optional<std::uint64_t> server = std::nullopt);
    std::unique_ptr<EventSource> source_;
    SettingsFile settingsFile_;
    Settings settings_;
    SpeakerStore store_;
    std::unique_ptr<SpeakerWindow> window_;
    QPointer<QDialog> dialog_;
    QTimer expiry_;
    QElapsedTimer clock_;
    std::uint64_t activeServer_ = 0;
};
}
