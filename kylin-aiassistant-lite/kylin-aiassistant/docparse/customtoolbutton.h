#ifndef CUSTOMTOOLBUTTON_H
#define CUSTOMTOOLBUTTON_H

#include <QToolButton>
#include <QEvent>

class CustomToolButton : public QToolButton
{
    Q_OBJECT

public:
    explicit CustomToolButton(QWidget *parent = nullptr);

    // 可选：添加一个设置函数，便于调试
    void setupButton(const QString &normalIconPath = "qrc:/icon/icon/add-file-black.svg",
                    const QString &hoverIconPath = "qrc:/icon/icon/add-file-hover.svg",
                    const QString &clickIconPath = "qrc:/icon/icon/add-file-click.svg");

protected:
    // 重写事件处理函数
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setNormalState();
    void setHoverState();
    void setClickState();

    // 添加一个调试模式
    void debugState(const QString &state);

private:
    QString m_normalIconPath;
    QString m_hoverIconPath;
    QString m_clickIconPath;

    bool m_debugMode = true;  // 设置为 true 以启用调试输出
};

#endif // CUSTOMTOOLBUTTON_H
