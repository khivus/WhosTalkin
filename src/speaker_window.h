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
    std::function<void()> onSettings;
    std::function<void()> onHidden;
protected:
    void closeEvent(QCloseEvent* event) override;
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
};
}
