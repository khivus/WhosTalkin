#include "speaker_window.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QSizeGrip>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace wt {
class SpeakerCanvas : public QWidget {
public:
    std::vector<Speaker> speakers;
    Settings settings;
    int rowHeight() const { return qRound(32 * settings.scale / 100.0); }
    explicit SpeakerCanvas(QWidget* parent) : QWidget(parent) {
        setObjectName("speakerCanvas");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void refresh() {
        setFixedHeight(static_cast<int>(speakers.size()) * rowHeight());
        setProperty("speakerCount", static_cast<int>(speakers.size()));
        const auto whispers = std::count_if(speakers.begin(), speakers.end(), [](const Speaker& s) {
            return s.kind == SpeechKind::IncomingWhisper;
        });
        setProperty("whisperCount", static_cast<int>(whispers));
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor text(settings.darkTheme ? "#edf0f5" : "#202733");
        const QColor muted(settings.darkTheme ? "#a7afbd" : "#5d6674");
        const int h = rowHeight();
        const qreal scale = settings.scale / 100.0;
        for (int i = 0; i < static_cast<int>(speakers.size()); ++i) {
            const auto& speaker = speakers[static_cast<std::size_t>(i)];
            const bool whisper = speaker.kind != SpeechKind::Normal;
            const QColor color(whisper && settings.showWhisper ? "#b98aff" : "#44c985");
            p.save();
            p.translate(13 * scale, i * h + h / 2.0);
            p.scale(scale, scale);
            p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(color);
            p.drawRoundedRect(QRectF(-3, -8, 6, 10), 3, 3);
            p.setBrush(Qt::NoBrush);
            QPainterPath arc;
            arc.moveTo(-6, -3); arc.lineTo(-6, 0); arc.cubicTo(-6, 8, 6, 8, 6, 0); arc.lineTo(6, -3);
            p.drawPath(arc);
            p.drawLine(QPointF(0, 6), QPointF(0, 10));
            p.drawLine(QPointF(-4, 10), QPointF(4, 10));
            p.restore();
            QString badge;
            if (whisper && settings.showWhisper)
                badge = speaker.kind == SpeechKind::OutgoingWhisper ? "WHISPER OUT" : "WHISPER";
            QFont badgeFont = font();
            badgeFont.setPointSizeF(8.0 * scale);
            badgeFont.setBold(true);
            const int badgeWidth = badge.isEmpty() ? 0 : QFontMetrics(badgeFont).horizontalAdvance(badge) + qRound(18 * scale);
            const int left = qRound(29 * scale);
            const int textWidth = std::max(0, width() - left - badgeWidth - qRound(9 * scale));
            p.setFont(font());
            p.setPen(text);
            const QString name = QString::fromUtf8(speaker.nickname.data(), static_cast<int>(speaker.nickname.size()));
            if (settings.allServers) {
                const QString server = QString::fromUtf8(speaker.serverName.data(), static_cast<int>(speaker.serverName.size()));
                p.drawText(QRect(left, i * h, textWidth, h * 3 / 5), Qt::AlignVCenter,
                           QFontMetrics(font()).elidedText(name, Qt::ElideRight, textWidth));
                p.setFont(badgeFont); p.setPen(muted);
                p.drawText(QRect(left, i * h + h / 2, textWidth, h / 2), Qt::AlignVCenter,
                           QFontMetrics(badgeFont).elidedText(server, Qt::ElideRight, textWidth));
            } else {
                p.drawText(QRect(left, i * h, textWidth, h), Qt::AlignVCenter,
                           QFontMetrics(font()).elidedText(name, Qt::ElideRight, textWidth));
            }
            p.setFont(badgeFont); p.setPen(color);
            p.drawText(QRect(width() - badgeWidth, i * h, badgeWidth - qRound(8 * scale), h),
                       Qt::AlignVCenter | Qt::AlignRight, badge);
        }
    }
};

SpeakerWindow::SpeakerWindow() : QWidget(nullptr) {
    setObjectName("whostalkinWindow");
    setWindowTitle("Active Speakers");
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    layout_ = new QVBoxLayout(this);
    layout_->setSpacing(0);
    titleBar_ = new QWidget(this);
    titleBar_->setObjectName("speakerTitleBar");
    auto* titleLayout = new QHBoxLayout(titleBar_);
    titleLayout->setContentsMargins(8, 0, 4, 0);
    title_ = new QLabel("Active Speakers", titleBar_);
    title_->setObjectName("speakerTitle");
    title_->setTextFormat(Qt::PlainText);
    titleLayout->addWidget(title_, 1);
    auto* settings = new QToolButton(titleBar_);
    settings->setText(QString::fromUtf8("\xe2\x9a\x99"));
    settings->setToolTip("Settings");
    settings->setObjectName("settingsButton");
    settings->setFocusPolicy(Qt::NoFocus);
    connect(settings, &QToolButton::clicked, this, [this] { if (onSettings) onSettings(); });
    titleLayout->addWidget(settings);
    auto* close = new QToolButton(titleBar_);
    close->setObjectName("closeButton");
    close->setText(QString::fromUtf8("\xc3\x97"));
    close->setToolTip("Hide (reopen from Plugins > WhosTalkin)");
    close->setFocusPolicy(Qt::NoFocus);
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(close);
    titleBar_->installEventFilter(this);
    title_->installEventFilter(this);
    layout_->addWidget(titleBar_);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName("speakerScroll");
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFocusPolicy(Qt::NoFocus);
    // QScrollArea's default minimum size hint is larger than a compact header.
    // Ignore that vertical hint; the content height is managed explicitly.
    scroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    scroll_->setMinimumHeight(0);
    canvas_ = new SpeakerCanvas(scroll_);
    scroll_->setWidget(canvas_);
    layout_->addWidget(scroll_, 1);
    grip_ = new QSizeGrip(this);
    grip_->setFixedSize(12, 10);
    layout_->addWidget(grip_, 0, Qt::AlignRight);
    resize(290, 100);
    applySettings(settings_);
}
void SpeakerWindow::applySettings(const Settings& settings) {
    const bool visible = isVisible();
    const auto position = pos();
    const auto sizeBefore = size();
    const int previousScale = settings_.scale;
    settings_ = settings;
    settings_.normalize();
    const qreal scale = settings_.scale / 100.0;
    const qreal resizeRatio = static_cast<qreal>(settings_.scale) / previousScale;
    Qt::WindowFlags flags = settings_.hideTaskbar ? Qt::Tool : Qt::Window;
    if (settings_.alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
    if (settings_.frameless) flags |= Qt::FramelessWindowHint;
    if (flags != windowFlags()) setWindowFlags(flags);
    setWindowOpacity(settings_.opacity / 100.0);
    QFont f = QApplication::font();
    f.setPointSizeF(9.0 * scale);
    setFont(f);
    layout_->setContentsMargins(qRound(3 * scale), qRound(3 * scale), qRound(3 * scale), qRound(2 * scale));
    auto* titleLayout = titleBar_->layout();
    titleLayout->setContentsMargins(qRound(8 * scale), 0, qRound(4 * scale), 0);
    titleLayout->setSpacing(qRound(6 * scale));
    grip_->setFixedSize(qRound(12 * scale), qRound(10 * scale));
    const QString bg = settings_.darkTheme ? "#20242d" : "#f4f5f8";
    const QString fg = settings_.darkTheme ? "#edf0f5" : "#202733";
    const QString hover = settings_.darkTheme ? "#363d4c" : "#dce1e9";
    // A host/application stylesheet takes precedence over QWidget::setFont.
    // Specify the scaled font in this window's stylesheet as well, scoped to
    // its children, and reset host padding/minimums on the header controls.
    setStyleSheet(QString("QWidget#whostalkinWindow, QWidget#whostalkinWindow QWidget { font-size: %4pt; }"
                          "QWidget#whostalkinWindow, QWidget#speakerCanvas { background: %1; color: %2; }"
                          "QWidget#speakerTitleBar { background: %1; }"
                          "QLabel#speakerTitle { color: %2; background: transparent; padding: 0; margin: 0; min-height: 0; }"
                          "QToolButton { color: %2; border: 0; margin: 0; min-height: 0; min-width: 0; padding: %5px; background: transparent; }"
                          "QToolButton:hover { background: %3; border-radius: 3px; }"
                          "QScrollArea { background: %1; border: 0; }")
                      .arg(bg, fg, hover, QString::number(9.0 * scale, 'f', 2), QString::number(qRound(4 * scale))));
    titleBar_->ensurePolished();
    title_->ensurePolished();
    for (auto* button : titleBar_->findChildren<QToolButton*>()) button->ensurePolished();
    titleLayout->invalidate();
    // Calculate AFTER applying the font and style: glyph fallback, native
    // style, DPI and TS3 skins can all change a control's actual minimum size.
    titleBar_->setFixedHeight(std::max(qRound(30 * scale), titleLayout->minimumSize().height()));
    const auto margins = layout_->contentsMargins();
    setMinimumWidth(std::max(qRound(220 * scale), titleLayout->minimumSize().width() + margins.left() + margins.right()));
    setMinimumHeight(chromeHeight());
    setMaximumHeight(std::max(minimumHeight(), settings_.maxHeight));
    canvas_->settings = settings_;
    canvas_->refresh();
    resize(qRound(sizeBefore.width() * resizeRatio), qRound(sizeBefore.height() * resizeRatio));
    move(position);
    adjustListHeight();
    if (visible) showPassive();
}
void SpeakerWindow::setSpeakers(const std::vector<Speaker>& speakers, std::size_t total) {
    canvas_->speakers = speakers;
    canvas_->refresh();
    title_->setToolTip(QString("%1 active speaker(s); %2 displayed").arg(static_cast<qulonglong>(total)).arg(speakers.size()));
    adjustListHeight();
}
int SpeakerWindow::chromeHeight() const {
    const auto margins = layout_->contentsMargins();
    return margins.top() + titleBar_->height() + grip_->height() + margins.bottom();
}
void SpeakerWindow::adjustListHeight() {
    scroll_->setVisible(!canvas_->speakers.empty());
    layout_->activate();
    if (!settings_.autoHeight) return;
    int cap = maximumHeight();
    if (auto* screen = QGuiApplication::screenAt(frameGeometry().center()))
        cap = std::min(cap, screen->availableGeometry().height() - 60);
    const int desired = chromeHeight() + canvas_->height();
    resize(width(), std::clamp(desired, minimumHeight(), std::max(minimumHeight(), cap)));
}
void SpeakerWindow::showPassive() { ensureOnScreen(); show(); }
void SpeakerWindow::ensureOnScreen() {
    for (auto* screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().intersects(QRect(frameGeometry().topLeft(), QSize(width(), 30)))) return;
    }
    if (auto* screen = QGuiApplication::primaryScreen())
        move(screen->availableGeometry().topLeft() + QPoint(40, 60));
}
void SpeakerWindow::closeEvent(QCloseEvent* event) {
    event->ignore();
    hide();
    if (onHidden) onHidden();
}
bool SpeakerWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == titleBar_ || watched == title_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                dragging_ = true; dragOffset_ = mouse->globalPos() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && dragging_) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->buttons() & Qt::LeftButton) { move(mouse->globalPos() - dragOffset_); return true; }
            dragging_ = false;
        } else if (event->type() == QEvent::MouseButtonRelease) { dragging_ = false; }
    }
    return QWidget::eventFilter(watched, event);
}
}
