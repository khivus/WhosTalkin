#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <algorithm>

namespace wt {
void Settings::normalize() {
    opacity = std::clamp(opacity, 20, 100);
    scale = std::clamp(scale, 75, 200);
    maxHeight = std::clamp(maxHeight, 120, 1600);
    maxSpeakers = std::clamp(maxSpeakers, 1, 100);
    removalDelay = std::clamp(removalDelay, 0, 300);
}
SettingsFile::SettingsFile(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    file_ = std::make_unique<QSettings>(path, QSettings::IniFormat);
    file_->setIniCodec("UTF-8");
}
SettingsFile::~SettingsFile() = default;
Settings SettingsFile::load() const {
    Settings s;
#define LOAD_BOOL(field) s.field = file_->value(QStringLiteral(#field), s.field).toBool()
#define LOAD_INT(field) s.field = file_->value(QStringLiteral(#field), s.field).toInt()
    LOAD_BOOL(alwaysOnTop); LOAD_BOOL(startVisible); LOAD_BOOL(hideTaskbar);
    LOAD_BOOL(frameless); LOAD_BOOL(darkTheme); LOAD_BOOL(autoHeight);
    LOAD_BOOL(showWhisper); LOAD_BOOL(showOwn); LOAD_BOOL(allServers); LOAD_BOOL(alphabetical);
    LOAD_INT(opacity); LOAD_INT(scale); LOAD_INT(maxHeight); LOAD_INT(maxSpeakers); LOAD_INT(removalDelay);
#undef LOAD_BOOL
#undef LOAD_INT
    s.normalize();
    return s;
}
void SettingsFile::save(const Settings& input) {
    auto s = input;
    s.normalize();
#define SAVE(field) file_->setValue(QStringLiteral(#field), s.field)
    SAVE(alwaysOnTop); SAVE(startVisible); SAVE(hideTaskbar); SAVE(frameless); SAVE(darkTheme);
    SAVE(autoHeight); SAVE(showWhisper); SAVE(showOwn); SAVE(allServers); SAVE(alphabetical);
    SAVE(opacity); SAVE(scale); SAVE(maxHeight); SAVE(maxSpeakers); SAVE(removalDelay);
#undef SAVE
}
QByteArray SettingsFile::geometry() const { return file_->value("geometry").toByteArray(); }
void SettingsFile::saveGeometry(const QByteArray& geometry) { file_->setValue("geometry", geometry); }
bool SettingsFile::sync() { file_->sync(); return file_->status() == QSettings::NoError; }
}
