#pragma once
#include "person_filter.h"
#include <QByteArray>
#include <QString>
#include <QSize>
#include <memory>

class QSettings;
namespace wt {
struct Settings {
    bool alwaysOnTop = true;
    bool startVisible = true;
    bool hideTaskbar = true;
    bool frameless = false;
    bool darkTheme = true;
    bool autoHeight = true;
    bool showWhisper = true;
    bool showOwn = true;
    bool allServers = false;
    bool alphabetical = false;
    bool includeOnly = false;
    bool integrate = false;
    int dockArea = 2; // Qt::RightDockWidgetArea; persist only this plugin's dock area.
    PersonRules people;
    int opacity = 95;
    int scale = 100;
    int maxHeight = 420;
    int maxSpeakers = 12;
    int removalDelay = 0;
    void normalize();
};
class SettingsFile {
public:
    explicit SettingsFile(const QString& path);
    ~SettingsFile();
    Settings load() const;
    void save(const Settings& settings);
    QByteArray geometry() const;
    QSize geometrySize() const;
    int geometryScale() const;
    void saveGeometry(const QByteArray& geometry, const QSize& size, int scale);
    bool sync();
private:
    std::unique_ptr<QSettings> file_;
};
}
