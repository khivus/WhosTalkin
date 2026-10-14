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
    removalDelay = std::clamp(removalDelay, 0, maxRemovalDelay);
    if (dockArea != 1 && dockArea != 2 && dockArea != 4 && dockArea != 8) dockArea = 2;
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
    LOAD_BOOL(includeOnly); LOAD_BOOL(integrate); LOAD_INT(dockArea);
    LOAD_INT(opacity); LOAD_INT(scale); LOAD_INT(maxHeight); LOAD_INT(maxSpeakers); LOAD_INT(removalDelay);
#undef LOAD_BOOL
#undef LOAD_INT
    const int count = file_->beginReadArray("people");
    for (int i = 0; i < count; ++i) {
        file_->setArrayIndex(i);
        const auto uid = file_->value("identity").toString().toUtf8().toStdString();
        const auto action = file_->value("rule").toString();
        if (uid.empty() || (action != "include" && action != "exclude")) continue;
        s.people[uid] = {file_->value("nickname").toString().toUtf8().toStdString(),
                        action == "include" ? FilterAction::Include : FilterAction::Exclude};
    }
    file_->endArray();
    s.normalize();
    return s;
}
void SettingsFile::save(const Settings& input) {
    auto s = input;
    s.normalize();
#define SAVE(field) file_->setValue(QStringLiteral(#field), s.field)
    SAVE(alwaysOnTop); SAVE(startVisible); SAVE(hideTaskbar); SAVE(frameless); SAVE(darkTheme);
    SAVE(autoHeight); SAVE(showWhisper); SAVE(showOwn); SAVE(allServers); SAVE(alphabetical);
    SAVE(includeOnly); SAVE(integrate); SAVE(dockArea);
    SAVE(opacity); SAVE(scale); SAVE(maxHeight); SAVE(maxSpeakers); SAVE(removalDelay);
#undef SAVE
    file_->remove("people");
    file_->beginWriteArray("people", static_cast<int>(s.people.size()));
    int index = 0;
    for (const auto& pair : s.people) {
        file_->setArrayIndex(index++);
        file_->setValue("identity", QString::fromUtf8(pair.first.c_str()));
        file_->setValue("nickname", QString::fromUtf8(pair.second.nickname.c_str()));
        file_->setValue("rule", pair.second.action == FilterAction::Include ? "include" : "exclude");
    }
    file_->endArray();
}
QByteArray SettingsFile::geometry() const { return file_->value("geometry").toByteArray(); }
QSize SettingsFile::geometrySize() const { return file_->value("geometrySize").toSize(); }
int SettingsFile::geometryScale() const {
    return std::clamp(file_->value("geometryScale", file_->value("scale", 100)).toInt(), 75, 200);
}
void SettingsFile::saveGeometry(const QByteArray& geometry, const QSize& size, int scale) {
    file_->setValue("geometry", geometry);
    file_->setValue("geometrySize", size);
    file_->setValue("geometryScale", scale);
}
bool SettingsFile::sync() { file_->sync(); return file_->status() == QSettings::NoError; }
}
