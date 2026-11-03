#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>

class QVBoxLayout;
class QScrollArea;
class QLabel;
class QPushButton;
class QEvent;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    void addUserBubble();
    void addAIBubble();
    void appendToUserBubble(const QString &text);
    void appendToAIBubble(const QString &text);
    void setLatestAIBubbleText(const QString &text);
    void setLatestAIBubbleErrorText(const QString &text);
    void setUserBubbleText(const QString &text);
    void showLatestAIBubbleActions();
    void clearBubbles();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateBubbleMaximumWidths();
    QLabel *latestUserBubble() const;
    QLabel *latestAIBubble() const;
    bool isScrolledToBottom() const;
    int verticalScrollValue() const;
    void restoreScrollValue(int value);
    void updateBubbleScroll(bool shouldScroll, int previousValue);
    int applyScrollToBottom();
    int continueScrollToBottom(int expectedValue);
    void scrollToBottom();

    QVBoxLayout *m_mainLayout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_scrollContent = nullptr;
    QVBoxLayout *m_bubbleLayout = nullptr;
    QList<QLabel *> m_userBubbles;
    QList<QLabel *> m_aiBubbles;
    QList<QWidget *> m_aiActionBars;
    QList<QPushButton *> m_aiCopyButtons;
    bool m_followBottom = true;
};

#endif // CHATWIDGET_H
