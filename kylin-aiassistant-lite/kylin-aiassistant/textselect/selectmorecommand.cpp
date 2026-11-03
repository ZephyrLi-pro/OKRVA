/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "selectmorecommand.h"
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnection>
#include <QDebug>
#include <QMenu>
#include <QTimer>
#include <QQmlContext>
#include "mypushbutton.h"
#include <kysdk/diagnosetest/libkydatacollect.h>
#include <kysdk/applications/kwidget.h>

#include "windowhelper.h"

SelectMoreCommandWidget::SelectMoreCommandWidget(QWidget *parent) :
    m_parentWidget(parent)
{
    this->setWindowFlags(Qt::WindowStaysOnTopHint);
    //设置属性不显示任务栏缩略图
#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *ukuiWindowHelper = new UkuiWindowHelper(this);
    ukuiWindowHelper->removeTitleBar();
    ukuiWindowHelper->setSkipTaskBar(true);
    ukuiWindowHelper->setSkipSwitcher(true);
#else
    kdk::UkuiStyleHelper::self()->removeHeader(this);
    kdk::WindowManager::setSkipTaskBar(this->windowHandle(), true);
    kdk::WindowManager::setSkipSwitcher(this->windowHandle(),true);
#endif

    this->setFixedSize(180,172);
    //初始化更多指令面板
    initSelectMoreCommandWidget();
    installEventFilter(this);
}

void SelectMoreCommandWidget::initSelectMoreCommandWidget()
{
    m_hLayoutMain = new QHBoxLayout();
    this->setContentsMargins(0,0,0,0);
    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setAccessibleName("kylin-aiassistant_QQuickWidget_m_quickWidget");
    m_quickWidget->setContentsMargins(0,0,0,0);
    //添加指令模
    m_quickWidget->rootContext()->setContextProperty("commandTemplateListModel", CommandTemplateList::getInstance());
    m_quickWidget->rootContext()->setContextProperty("selectMoreCommand", this);
    m_quickWidget->setSource(QUrl("qrc:/qml/qml/SelectMoreCommand.qml"));
}

bool SelectMoreCommandWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::ActivationChange) {
        if (QApplication::activeWindow() != this) {
            if(QApplication::activeWindow() == m_parentWidget) {
                emit sigDeactiveWidget(false);
            } else{
                emit sigDeactiveWidget(true);
            }
        }
    }

    return QWidget::eventFilter(o, e);
}

void SelectMoreCommandWidget::keyPressEvent(QKeyEvent *event)
{
    //按ESC键关闭窗口
    if(event->key() == Qt::Key_Escape)
    {
        close();
        emit sigCloseSelectWidget();
    }
    // 确保基类也能处理事件
    QWidget::keyPressEvent(event);
}

void SelectMoreCommandWidget::setWidgetWidth(const int width)
{
    qDebug() << "设置当前宽度为"<< width;
    this->setFixedWidth(width);
    QQuickItem *item=m_quickWidget->rootObject();
    if(item){
        QMetaObject::invokeMethod(item, "onSetWidget",Q_ARG(QVariant ,width));

         QMetaObject::invokeMethod(item, "onSetHeight");
    }
}

void SelectMoreCommandWidget::setWidgetHeight(const int height)
{
    KyInfo() << "设置当前高度为"<< height;
    this->setFixedHeight(height);
}

void SelectMoreCommandWidget::sendButtonText(const QString &currentButtonText, const int currentId)
{
    //填充当前按钮文本
    if(!currentButtonText.isEmpty() && currentId != -1 )
    {
        KyInfo() << "currentButtonText" << currentButtonText;
        emit sigCurrentCommandText(currentButtonText,currentId);
    }

}

