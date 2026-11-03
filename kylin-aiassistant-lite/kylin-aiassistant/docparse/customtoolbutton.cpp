#include "customtoolbutton.h"

#include <QEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QIcon>
#include <QSize>
#include <QPalette>
#include <QColor>
#include <QDebug>  // 添加调试输出

CustomToolButton::CustomToolButton(QWidget *parent)
    : QToolButton(parent)
{
    // 设置默认值
    m_normalIconPath = ":/icon/icon/add-file-black.svg";
    m_hoverIconPath = ":/icon/icon/add-file-hover.svg";
    m_clickIconPath = ":/icon/icon/add-file-click.svg";

    // 设置图标大小
    setIconSize(QSize(16, 16));

    // 设置按钮样式
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 设置初始状态
    setNormalState();

    debugState("构造函数");
}

void CustomToolButton::setupButton(const QString &normalIconPath,
                                   const QString &hoverIconPath,
                                   const QString &clickIconPath)
{
    m_normalIconPath = normalIconPath;
    m_hoverIconPath = hoverIconPath;
    m_clickIconPath = clickIconPath;

    setNormalState();
}

bool CustomToolButton::event(QEvent *event)
{
    // 调试：输出所有事件
    if (m_debugMode) {
        qDebug() << "CustomToolButton 事件:" << event->type();
    }

    return QToolButton::event(event);
}

void CustomToolButton::mousePressEvent(QMouseEvent *event)
{
    if (m_debugMode) {
        qDebug() << "CustomToolButton 鼠标按下";
    }

    setClickState();
    QToolButton::mousePressEvent(event);
}

void CustomToolButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_debugMode) {
        qDebug() << "CustomToolButton 鼠标释放";
    }

    // 如果鼠标仍在按钮上，设置为悬停状态，否则为正常状态
    if (rect().contains(event->pos())) {
        setHoverState();
    } else {
        setNormalState();
    }

    QToolButton::mouseReleaseEvent(event);
}

void CustomToolButton::enterEvent(QEvent *event)
{
    if (m_debugMode) {
        qDebug() << "CustomToolButton 鼠标进入";
    }

    setHoverState();
    QToolButton::enterEvent(event);
}

void CustomToolButton::leaveEvent(QEvent *event)
{
    if (m_debugMode) {
        qDebug() << "CustomToolButton 鼠标离开";
    }

    setNormalState();
    QToolButton::leaveEvent(event);
}

void CustomToolButton::setNormalState()
{
    debugState("正常状态");

    // 设置图标
    QIcon icon(m_normalIconPath);
    if (icon.isNull()) {
        qWarning() << "无法加载正常状态图标:" << m_normalIconPath;
    } else {
        setIcon(icon);
    }
}

void CustomToolButton::setHoverState()
{
    debugState("悬停状态");

    // 设置图标
    QIcon icon(m_hoverIconPath);
    if (icon.isNull()) {
        qWarning() << "无法加载悬停状态图标:" << m_hoverIconPath;
    } else {
        setIcon(icon);
    }
}

void CustomToolButton::setClickState()
{
    debugState("点击状态");

    // 设置图标
    QIcon icon(m_clickIconPath);
    if (icon.isNull()) {
        qWarning() << "无法加载点击状态图标:" << m_clickIconPath;
    } else {
        setIcon(icon);
    }
}

void CustomToolButton::debugState(const QString &state)
{
    if (m_debugMode) {
        qDebug() << "CustomToolButton 状态变化:" << state
                 << "对象名称:" << objectName()
                 << "文本:" << text();
    }
}
