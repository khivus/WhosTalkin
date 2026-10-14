#pragma once
#include "settings.h"
#include "speaker_store.h"
#include <QPoint>
#include <QWidget>
#include <functional>

class QLabel;
class QScrollArea;
class QSizeGrip;
class QVBoxLayout;
namespace wt {
class SpeakerCanvas;
class SpeakerWindow : public QWidget {
public:
    SpeakerWindow();
    void applySettings(const Settings& settings);
    void setSpeakers(const std::vector<Speaker>& speakers, std::size_t total);
    void showPassive();
    void ensureOnScreen();
    void setEmbedded(bool embedded);
    bool embedded() const { return embedded_; }
    std::function<void()> onSettings;
    std::function<void()> onPeopleFilters;
    std::function<void(SpeakerKey, const QPoint&)> onPersonMenu;
    std::function<void()> onHidden;
protected:
    void closeEvent(QCloseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    int chromeHeight() const;
    void adjustListHeight();
    Settings settings_;
    QWidget* titleBar_;
    QLabel* title_;
    QScrollArea* scroll_;
    SpeakerCanvas* canvas_;
    QSizeGrip* grip_;
    QVBoxLayout* layout_;
    QPoint dragOffset_;
    bool dragging_ = false;
    bool embedded_ = false;
};
}
