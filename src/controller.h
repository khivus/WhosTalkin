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
class QDockWidget;
class QMainWindow;
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
    void openPeopleFilters();
    bool filterPerson(SpeakerKey key, FilterAction action);
    void openPersonMenu(SpeakerKey key, const QPoint& position);
    void setHostParent(QWidget* parent);
    void resetPosition();
    void applySettings(Settings settings);
    SpeakerWindow* window() const { return window_.data(); }
    QDockWidget* dock() const;
    const SpeakerStore& store() const { return store_; }
    const Settings& settings() const { return settings_; }
private:
    void render();
    void armExpiry();
    void saveGeometry();
    void seed(std::optional<std::uint64_t> server = std::nullopt);
    void ensureWindow();
    void restoreFloatingGeometry();
    void updatePresentation();
    void useHost(QMainWindow* host);
    void persistFilters();
    void setPersonRule(SpeakerKey key, const std::string& uid, const std::string& name, FilterAction action);
    void refreshPeopleDialog();
    std::unique_ptr<EventSource> source_;
    SettingsFile settingsFile_;
    Settings settings_;
    SpeakerStore store_;
    PersonFilter filter_;
    QPointer<SpeakerWindow> window_;
    QPointer<QMainWindow> host_;
    QMetaObject::Connection hostDestroyed_;
    QPointer<QDockWidget> dock_;
    QByteArray floatingGeometry_;
    QSize floatingSize_;
    int floatingScale_ = 100;
    QPointer<QDialog> dialog_;
    QPointer<QDialog> peopleDialog_;
    QTimer expiry_;
    QElapsedTimer clock_;
    std::uint64_t activeServer_ = 0;
    bool visibleRequested_ = false;
    bool hostSearchAllowed_ = true;
    bool shuttingDown_ = false;
};
}
