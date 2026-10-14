#pragma once
#include <QByteArray>
#include <QString>
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
    void saveGeometry(const QByteArray& geometry);
    bool sync();
private:
    std::unique_ptr<QSettings> file_;
};
}
