#include "controller.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QMainWindow>
#include <QResizeEvent>
#include <QShowEvent>
#include <algorithm>

namespace wt {
namespace {
class SpeakerDock : public QDockWidget {
public:
    explicit SpeakerDock(QMainWindow* host) : QDockWidget("WhosTalkin", host) {}
    std::function<void()> onClosed;
    std::function<void()> onResized;
    std::function<void()> onShown;
protected:
    void resizeEvent(QResizeEvent* event) override {
        QDockWidget::resizeEvent(event);
        if (onResized) onResized();
    }
    void showEvent(QShowEvent* event) override {
        QDockWidget::showEvent(event);
        if (onShown) onShown();
    }
    void closeEvent(QCloseEvent* event) override {
        event->ignore(); hide(); if (onClosed) onClosed();
    }
};
}
QDockWidget* Controller::dock() const { return dock_.data(); }
void Controller::ensureWindow() {
    if (window_) return;
    window_ = new SpeakerWindow;
    window_->onSettings = [this] { openSettings(); };
    window_->onPeopleFilters = [this] { openPeopleFilters(); };
    window_->onPersonMenu = [this](SpeakerKey key, const QPoint& position) { openPersonMenu(key, position); };
    window_->onHidden = [this] { hide(); };
    window_->applySettings(settings_);
    restoreFloatingGeometry();
    window_->ensureOnScreen();
}
void Controller::restoreFloatingGeometry() {
    if (!floatingGeometry_.isEmpty()) window_->restoreGeometry(floatingGeometry_);
    if (floatingSize_.isValid()) {
        const qreal ratio = static_cast<qreal>(settings_.scale) / floatingScale_;
        window_->resize(qRound(floatingSize_.width() * ratio), qRound(floatingSize_.height() * ratio));
    }
}
void Controller::captureDockSize() {
    if (shuttingDown_ || dockSizePendingRestore_ || !dock_ || !host_ ||
        !dock_->isVisible() || !host_->isVisible() || host_->isMinimized()) return;
    const auto size = dock_->size();
    if (size.width() <= 0 || size.height() <= 0 || size == dockSize_) return;
    dockSize_ = size;
    dockSizeSave_.start(); // Coalesce splitter-drag writes; unload/hide flush immediately.
}
void Controller::restoreDockSize() {
    if (shuttingDown_ || !dockSizePendingRestore_ || dockSizeRestoreQueued_ || !dock_ || !host_) return;
    dockSizeRestoreQueued_ = true;
    QPointer<QDockWidget> target = dock_;
    // Restore after addDockWidget/show and the host's initial layout pass.
    QTimer::singleShot(0, this, [this, target] {
        if (shuttingDown_ || !target || target != dock_) return;
        if (!host_ || !host_->isVisible() || host_->isMinimized() || !target->isVisible()) {
            dockSizeRestoreQueued_ = false;
            return; // A later Show/window-state event retries a hidden startup.
        }
        const auto area = host_->dockWidgetArea(target);
        const bool side = area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea;
        const int available = std::max(1, side ? host_->width() : host_->height());
        const int wanted = std::clamp(side ? dockSize_.width() : dockSize_.height(), 1, available);
        host_->resizeDocks({target.data()}, {wanted}, side ? Qt::Horizontal : Qt::Vertical);
        // Ignore construction/layout resize events until Qt applies the requested size.
        QTimer::singleShot(0, this, [this, target] {
            if (shuttingDown_ || !target || target != dock_) return;
            dockSizeRestoreQueued_ = false;
            dockSizePendingRestore_ = false;
            captureDockSize();
        });
    });
}
bool Controller::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host_ && (event->type() == QEvent::Show || event->type() == QEvent::WindowStateChange))
        restoreDockSize();
    return QObject::eventFilter(watched, event);
}
void Controller::useHost(QMainWindow* host) {
    if (!host || host_ == host) return;
    // Do not replace an active dock's host. Switching display mode first is
    // required, so no child can be orphaned in another main window.
    if (dock_) return;
    disconnect(hostDestroyed_);
    if (host_) host_->removeEventFilter(this);
    host_ = host;
    host->installEventFilter(this);
    hostDestroyed_ = connect(host, &QObject::destroyed, this, [this] {
        saveGeometry(); // Flush the last observed size before host-owned children disappear.
        host_.clear(); hostSearchAllowed_ = false;
        // Let Qt finish deleting host-owned children before making a fallback.
        QTimer::singleShot(0, this, [this] {
            if (shuttingDown_) return;
            updatePresentation(); render();
            if (visibleRequested_) show();
        });
    });
}
void Controller::setHostParent(QWidget* parent) {
    for (auto* current = parent; current; current = current->parentWidget()) {
        if (auto* main = qobject_cast<QMainWindow*>(current)) {
            useHost(main); hostSearchAllowed_ = true; break;
        }
    }
    updatePresentation();
}
void Controller::updatePresentation() {
    ensureWindow();
    if (settings_.integrate && !host_ && hostSearchAllowed_) {
        // Use only public Qt types and ownership. No private widget names,
        // offsets, hooks or replacement of the client's existing layout.
        QMainWindow* candidate = nullptr;
        bool ambiguous = false;
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* main = qobject_cast<QMainWindow*>(widget)) {
                if (candidate) { ambiguous = true; break; }
                candidate = main;
            }
        }
        if (!ambiguous && candidate) useHost(candidate);
    }
    if (settings_.integrate && host_) {
        if (!dock_) {
            floatingGeometry_ = window_->saveGeometry();
            floatingSize_ = window_->size(); floatingScale_ = settings_.scale;
            settingsFile_.saveGeometry(floatingGeometry_, floatingSize_, floatingScale_);
            window_->hide();
            auto* dock = new SpeakerDock(host_);
            dock_ = dock;
            dockSizePendingRestore_ = dockSize_.isValid();
            dockSizeRestoreQueued_ = false;
            dock->setObjectName("WhosTalkinDock");
            dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
            dock->onClosed = [this] { hide(); };
            dock->onResized = [this] { captureDockSize(); };
            dock->onShown = [this] { restoreDockSize(); captureDockSize(); };
            window_->setParent(dock);
            window_->setEmbedded(true);
            dock->setWidget(window_);
            host_->addDockWidget(static_cast<Qt::DockWidgetArea>(settings_.dockArea), dock);
            connect(dock, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea area) {
                if (shuttingDown_ || area == Qt::NoDockWidgetArea) return;
                settings_.dockArea = static_cast<int>(area);
                settingsFile_.save(settings_);
                settingsFile_.sync();
            });
            window_->show();
            if (visibleRequested_) dock->show(); else dock->hide();
        }
    } else if (dock_) {
        saveGeometry();
        // Detaching the content can resize the empty dock; those teardown sizes
        // must not replace the last user-visible dimensions.
        auto* oldDock = static_cast<SpeakerDock*>(dock_.data());
        oldDock->onResized = {};
        oldDock->onShown = {};
        dockSizePendingRestore_ = false;
        dockSizeRestoreQueued_ = false;
        window_->hide();
        window_->setParent(nullptr);
        window_->setEmbedded(false);
        if (host_) host_->removeDockWidget(dock_);
        delete dock_.data();
        restoreFloatingGeometry();
        window_->ensureOnScreen();
        if (visibleRequested_) window_->showPassive();
    }
}
void Controller::show() {
    visibleRequested_ = true;
    updatePresentation(); render();
    if (dock_) { window_->show(); dock_->show(); }
    else window_->showPassive();
}
void Controller::hide() {
    visibleRequested_ = false;
    saveGeometry();
    if (dock_) dock_->hide(); else if (window_) window_->hide();
}
void Controller::toggle() { if (visibleRequested_) hide(); else show(); }
void Controller::saveGeometry() {
    captureDockSize();
    dockSizeSave_.stop();
    settingsFile_.saveDockSize(dockSize_);
    if (window_ && !dock_) {
        floatingGeometry_ = window_->saveGeometry();
        floatingSize_ = window_->size(); floatingScale_ = settings_.scale;
        settingsFile_.saveGeometry(floatingGeometry_, floatingSize_, floatingScale_);
    }
    if (!settingsFile_.sync()) source_->log("Could not save WhosTalkin settings; check config directory permissions.");
}
void Controller::resetPosition() {
    if (dock_ && host_) host_->addDockWidget(Qt::RightDockWidgetArea, dock_);
    else { ensureWindow(); window_->move(-32000, -32000); window_->ensureOnScreen(); }
    show();
}
}
