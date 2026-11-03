#include "chatwidget.h"

#include <kballontip.h>

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

enum BubbleRole { RoleUser, RoleAI };

namespace {

QPainterPath roundedRectPath(const QRectF &rect, qreal topLeft, qreal topRight,
                             qreal bottomRight, qreal bottomLeft)
{
    QPainterPath path;
    path.moveTo(rect.left() + topLeft, rect.top());
    path.lineTo(rect.right() - topRight, rect.top());
    if (topRight > 0.0) {
        path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + topRight);
    } else {
        path.lineTo(rect.right(), rect.top());
    }

    path.lineTo(rect.right(), rect.bottom() - bottomRight);
    if (bottomRight > 0.0) {
        path.quadTo(rect.right(), rect.bottom(), rect.right() - bottomRight, rect.bottom());
    } else {
        path.lineTo(rect.right(), rect.bottom());
    }

    path.lineTo(rect.left() + bottomLeft, rect.bottom());
    if (bottomLeft > 0.0) {
        path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - bottomLeft);
    } else {
        path.lineTo(rect.left(), rect.bottom());
    }

    path.lineTo(rect.left(), rect.top() + topLeft);
    if (topLeft > 0.0) {
        path.quadTo(rect.left(), rect.top(), rect.left() + topLeft, rect.top());
    } else {
        path.lineTo(rect.left(), rect.top());
    }

    path.closeSubpath();
    return path;
}

class BubbleLabel : public QLabel
{
public:
    explicit BubbleLabel(BubbleRole role, QWidget *parent = nullptr)
        : QLabel(parent)
        , m_role(role)
    {
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setTextInteractionFlags(Qt::TextSelectableByMouse);
        setContextMenuPolicy(Qt::NoContextMenu);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        setContentsMargins(12, 8, 12, 8);
        QPalette bubblePalette = palette();
        bubblePalette.setColor(QPalette::WindowText, QColor(m_role == RoleUser ? "#000000" : "#000000"));
        setPalette(bubblePalette);
    }

    void setMaximumContentWidth(int width)
    {
        const int boundedWidth = qMax(1, width);
        if (m_maximumContentWidth == boundedWidth) {
            return;
        }
        m_maximumContentWidth = boundedWidth;
        updateGeometry();
        update();
    }

    void setActionsVisible(bool visible)
    {
        if (m_actionsVisible == visible) {
            return;
        }
        m_actionsVisible = visible;
        update();
    }

    void setErrorState(bool error)
    {
        if (m_error == error) {
            return;
        }
        m_error = error;
        QPalette bubblePalette = palette();
        bubblePalette.setColor(QPalette::WindowText, QColor(m_error ? "#D92D20" : "#000000"));
        setPalette(bubblePalette);
        setStyleSheet(m_error ? QStringLiteral("color: #D92D20;") : QString());
        update();
    }

    QSize sizeHint() const override
    {
        return bubbleSizeHint();
    }

    QSize minimumSizeHint() const override
    {
        return bubbleSizeHint();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(backgroundColor());

        const QRectF bubbleRect = rect();
        const qreal bottomRadius = (m_role == RoleAI && m_actionsVisible && !m_error) ? 0.0 : 10.0;
        painter.drawPath(roundedRectPath(bubbleRect, 10.0, 10.0, bottomRadius, bottomRadius));

        QLabel::paintEvent(event);
    }

private:
    QSize bubbleSizeHint() const
    {
        const QMargins margins = contentsMargins();
        const int maxTextWidth = qMax(1, m_maximumContentWidth - margins.left() - margins.right());

        QFontMetrics metrics(font());
        QRect textRect;
        if (text().isEmpty()) {
            textRect = QRect(0, 0, 0, metrics.height());
        } else {
            textRect = metrics.boundingRect(QRect(0, 0, maxTextWidth, 100000),
                                            Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                            text());
        }

        const int width = qMin(m_maximumContentWidth,
                               textRect.width() + margins.left() + margins.right());
        const int height = textRect.height() + margins.top() + margins.bottom();
        return QSize(qMax(width, margins.left() + margins.right()),
                     qMax(height, margins.top() + margins.bottom()));
    }

    QColor backgroundColor() const
    {
        return m_role == RoleUser ? QColor("#C4E1FF") : QColor("#F7F7F7");
    }

    BubbleRole m_role;
    int m_maximumContentWidth = 240;
    bool m_actionsVisible = false;
    bool m_error = false;
};

class ActionBarWidget : public QWidget
{
public:
    explicit ActionBarWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(32);
        setVisible(false);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#F7F7F7"));
        painter.drawPath(roundedRectPath(rect(), 0.0, 0.0, 10.0, 10.0));
    }
};

class CopyButton : public QPushButton
{
public:
    explicit CopyButton(QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_icon(QStringLiteral(":/image/icon/copy.svg"))
    {
        setFixedSize(28, 28);
        setCursor(Qt::PointingHandCursor);
        setToolTip(ChatWidget::tr("Copy"));
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void enterEvent(QEvent *event) override
    {
        m_hovered = true;
        update();
        QPushButton::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QPushButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QColor background(Qt::transparent);
        if (isDown()) {
            background = QColor(0, 0, 0, 31);
        } else if (m_hovered) {
            background = QColor(0, 0, 0, 15);
        }

        if (background.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(background);
            painter.drawRoundedRect(rect(), 4, 4);
        }

        const QSize iconDrawSize(16, 16);
        const QRect iconRect((width() - iconDrawSize.width()) / 2,
                             (height() - iconDrawSize.height()) / 2,
                             iconDrawSize.width(), iconDrawSize.height());
        m_icon.paint(&painter, iconRect);
    }

private:
    QIcon m_icon;
    bool m_hovered = false;
};

static BubbleLabel *asBubbleLabel(QLabel *label)
{
    return dynamic_cast<BubbleLabel *>(label);
}

static void setBubbleActionsVisible(QLabel *label, bool visible)
{
    BubbleLabel *bubbleLabel = asBubbleLabel(label);
    if (bubbleLabel) {
        bubbleLabel->setActionsVisible(visible);
    }
}

static void setBubbleErrorState(QLabel *label, bool error)
{
    BubbleLabel *bubbleLabel = asBubbleLabel(label);
    if (bubbleLabel) {
        bubbleLabel->setErrorState(error);
    }
}

static void showCenteredCopyTip(QWidget *anchor)
{
    QWidget *host = anchor ? anchor->window() : nullptr;
    if (!host) {
        host = anchor;
    }
    if (!host) {
        return;
    }

    auto *tip = new kdk::KBallonTip(QObject::tr("复制成功"), kdk::Normal);
    tip->setWindowFlag(Qt::FramelessWindowHint);
    tip->setAttribute(Qt::WA_DeleteOnClose);
    tip->setAttribute(Qt::WA_TranslucentBackground);
    // KBallonTip 默认内容区较高，16px 上下边距时总高度约 70px；改为 7px 后总高度为 52px（对齐 CommonTips）
    tip->setContentsMargins(16, 7, 16, 7);
    tip->setTipTime(3000);
    tip->adjustSize();
    if (!tip->sizeHint().isEmpty()) {
        tip->setFixedSize(tip->sizeHint());
    }

    const QPoint hostCenter = host->mapToGlobal(host->rect().center());
    tip->move(hostCenter.x() - tip->width() / 2,
              hostCenter.y() - tip->height() / 2);
    tip->showInfo();
}

static QLabel *createBubble(BubbleRole role)
{
    return new BubbleLabel(role);
}

static CopyButton *createAICopyButton()
{
    return new CopyButton;
}

static QWidget *createAIActionBar(QPushButton **copyButton)
{
    ActionBarWidget *bar = new ActionBarWidget;

    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 0, 8, 4);
    layout->setSpacing(0);

    *copyButton = createAICopyButton();
    layout->addWidget(*copyButton);
    layout->addStretch();

    return bar;
}

static QWidget *addBubbleToLayout(QVBoxLayout *layout, QLabel *label, BubbleRole role, QPushButton **copyButton = nullptr)
{
    QHBoxLayout *row = new QHBoxLayout;
    row->setContentsMargins(8, 2, 8, 2);
    row->setSpacing(0);

    QWidget *actionBar = nullptr;
    QWidget *bubbleContainer = new QWidget;
    bubbleContainer->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleContainer);
    bubbleLayout->setContentsMargins(0, 0, 0, 0);
    bubbleLayout->setSpacing(0);
    bubbleLayout->addWidget(label);

    if (role == RoleAI) {
        actionBar = createAIActionBar(copyButton);
        bubbleLayout->addWidget(actionBar);
    }

    if (role == RoleUser) {
        row->addStretch();
        row->addWidget(bubbleContainer);
    } else {
        row->addWidget(bubbleContainer);
        row->addStretch();
    }
    layout->addLayout(row);
    return actionBar;
}

} // namespace

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_scrollArea(new QScrollArea(this))
    , m_scrollContent(new QWidget)
    , m_bubbleLayout(new QVBoxLayout(m_scrollContent))
{
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, Qt::white);
    setPalette(widgetPalette);
    setContextMenuPolicy(Qt::NoContextMenu);
    m_mainLayout->setContentsMargins(0, 0, 0, 8);
    m_mainLayout->setSpacing(0);

    m_scrollContent->setAutoFillBackground(true);
    m_scrollContent->setBackgroundRole(QPalette::Window);
    m_scrollContent->setContextMenuPolicy(Qt::NoContextMenu);
    m_scrollArea->setAutoFillBackground(true);
    m_scrollArea->setBackgroundRole(QPalette::Window);
    m_scrollArea->setContextMenuPolicy(Qt::NoContextMenu);
    m_scrollArea->viewport()->setContextMenuPolicy(Qt::NoContextMenu);
    m_scrollArea->viewport()->installEventFilter(this);

    m_bubbleLayout->setContentsMargins(0, 0, 0, 10);
    m_bubbleLayout->setSpacing(0);
    m_bubbleLayout->setAlignment(Qt::AlignTop);

    m_scrollArea->setWidget(m_scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->horizontalScrollBar()->setVisible(false);

    QScrollBar *verticalBar = m_scrollArea->verticalScrollBar();
    connect(verticalBar, &QScrollBar::valueChanged, this, [this]() {
        m_followBottom = isScrolledToBottom();
    });
    connect(verticalBar, &QScrollBar::rangeChanged, this, [this](int, int) {
        if (m_followBottom) {
            scrollToBottom();
        }
    });

    m_mainLayout->addWidget(m_scrollArea);
}

ChatWidget::~ChatWidget()
{
}

void ChatWidget::addUserBubble()
{
    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    QLabel *label = createBubble(RoleUser);
    addBubbleToLayout(m_bubbleLayout, label, RoleUser);
    m_userBubbles.append(label);
    updateBubbleMaximumWidths();
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::addAIBubble()
{
    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    QLabel *label = createBubble(RoleAI);
    QPushButton *copyButton = nullptr;
    QWidget *actionBar = addBubbleToLayout(m_bubbleLayout, label, RoleAI, &copyButton);
    m_aiBubbles.append(label);
    m_aiActionBars.append(actionBar);
    m_aiCopyButtons.append(copyButton);

    connect(copyButton, &QPushButton::clicked, this, [this, label, copyButton]() {
        if (!label) return;
        const QString text = label->text();
        if (text.isEmpty()) return;
        QApplication::clipboard()->setText(text);
        showCenteredCopyTip(this);
    });

    updateBubbleMaximumWidths();
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::appendToUserBubble(const QString &text)
{
    QLabel *label = latestUserBubble();
    if (!label) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    QString current = label->text();
    label->setText(current + text);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::appendToAIBubble(const QString &text)
{
    QLabel *label = latestAIBubble();
    if (!label) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    setBubbleErrorState(label, false);
    QString current = label->text();
    label->setText(current + text);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::setLatestAIBubbleText(const QString &text)
{
    QLabel *label = latestAIBubble();
    if (!label) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    setBubbleErrorState(label, false);
    label->setText(text);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::setLatestAIBubbleErrorText(const QString &text)
{
    QLabel *label = latestAIBubble();
    if (!label) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    label->setText(text);
    setBubbleErrorState(label, true);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::setUserBubbleText(const QString &text)
{
    QLabel *label = latestUserBubble();
    if (!label) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    label->setText(text);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::showLatestAIBubbleActions()
{
    if (m_aiActionBars.isEmpty()) return;

    QLabel *label = latestAIBubble();
    if (!label || label->text().trimmed().isEmpty()) return;

    QWidget *actionBar = m_aiActionBars.last();
    if (!actionBar) return;

    const bool shouldAutoScroll = isScrolledToBottom();
    const int previousValue = verticalScrollValue();
    setBubbleActionsVisible(label, true);
    actionBar->setVisible(true);
    updateBubbleScroll(shouldAutoScroll, previousValue);
}

void ChatWidget::clearBubbles()
{
    m_userBubbles.clear();
    m_aiBubbles.clear();
    m_aiActionBars.clear();
    m_aiCopyButtons.clear();

    QLayoutItem *item;
    while ((item = m_bubbleLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        if (item->layout()) {
            QLayoutItem *inner;
            while ((inner = item->layout()->takeAt(0)) != nullptr) {
                if (inner->widget()) inner->widget()->deleteLater();
                delete inner;
            }
        }
        delete item;
    }
}

bool ChatWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        updateBubbleMaximumWidths();
    }
    return QWidget::eventFilter(watched, event);
}

void ChatWidget::updateBubbleMaximumWidths()
{
    const int maxBubbleWidth = qMax(1, m_scrollArea->viewport()->width() - 16);

    const auto updateLabels = [maxBubbleWidth](const QList<QLabel *> &labels) {
        for (QLabel *label : labels) {
            BubbleLabel *bubbleLabel = asBubbleLabel(label);
            if (bubbleLabel) {
                bubbleLabel->setMaximumContentWidth(maxBubbleWidth);
            }
        }
    };

    updateLabels(m_userBubbles);
    updateLabels(m_aiBubbles);
}

QLabel *ChatWidget::latestUserBubble() const
{
    if (m_userBubbles.isEmpty()) return nullptr;
    return m_userBubbles.last();
}

QLabel *ChatWidget::latestAIBubble() const
{
    if (m_aiBubbles.isEmpty()) return nullptr;
    return m_aiBubbles.last();
}

bool ChatWidget::isScrolledToBottom() const
{
    QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
    if (!bar) {
        return true;
    }

    return bar->maximum() - bar->value() <= 2;
}

int ChatWidget::verticalScrollValue() const
{
    QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
    if (!bar) {
        return 0;
    }

    return bar->value();
}

void ChatWidget::restoreScrollValue(int value)
{
    QTimer::singleShot(0, this, [this, value]() {
        QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
        if (!bar) {
            return;
        }
        bar->setValue(qMin(value, bar->maximum()));
    });
}

void ChatWidget::updateBubbleScroll(bool shouldScroll, int previousValue)
{
    if (shouldScroll || m_followBottom) {
        m_followBottom = true;
        scrollToBottom();
        return;
    }

    m_followBottom = false;
    restoreScrollValue(previousValue);
}

int ChatWidget::applyScrollToBottom()
{
    if (!m_scrollArea) {
        return 0;
    }

    if (m_scrollContent) {
        m_scrollContent->updateGeometry();
        if (m_scrollContent->layout()) {
            m_scrollContent->layout()->activate();
        }
    }

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        return 0;
    }

    bar->setValue(bar->maximum());
    m_followBottom = isScrolledToBottom();
    return bar->value();
}

int ChatWidget::continueScrollToBottom(int expectedValue)
{
    QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
    if (!bar) {
        return -1;
    }

    if (!m_followBottom) {
        return -1;
    }

    if (bar->value() < expectedValue - 2) {
        return -1;
    }

    return applyScrollToBottom();
}

void ChatWidget::scrollToBottom()
{
    m_followBottom = true;
    const int expectedValue = applyScrollToBottom();
    QTimer::singleShot(0, this, [this, expectedValue]() {
        const int nextExpectedValue = continueScrollToBottom(expectedValue);
        if (nextExpectedValue < 0) {
            return;
        }
        QTimer::singleShot(0, this, [this, nextExpectedValue]() {
            continueScrollToBottom(nextExpectedValue);
        });
    });
}
