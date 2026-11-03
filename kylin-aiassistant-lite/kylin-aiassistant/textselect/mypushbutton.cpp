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
#include "mypushbutton.h"
#include <QApplication>
#include <QMenu>
#include <QClipboard>
#include <QDebug>
#include "clipboard.h"

MyPushButton::MyPushButton(QWidget *parent) :
    QPushButton(parent)
{
    settingRightClickMenu= new QMenu(this);
    settingRightClickMenu->setAccessibleName("kylin-aiassistant_QMenu_settingRightClickMenu");
    homePageAction = settingRightClickMenu->addAction(tr("Qi Ling HomePage"));
    disableAction = settingRightClickMenu->addAction(tr("Disable Floating Bar For Scribe Words"));
    settingAction = settingRightClickMenu->addAction(tr("Setting"));
    closeAction = settingRightClickMenu->addAction(tr("Close"));

}


void MyPushButton::mousePressEvent(QMouseEvent *event){

        if (event->button() == Qt::RightButton) {
            Q_EMIT rightButtonClicked();
            // 右键点击事件
            if(event->type() == QEvent::ContextMenu){
               QMenu menu(this);
               menu.show();
           }
        } else if (event->button() == Qt::LeftButton) {
            // 左键点击事件
            Q_EMIT leftButtonClicked();
        }
        QPushButton::mousePressEvent(event); // 调用基类的mousePressEvent
}

void MyPushButton::contextMenuEvent(QContextMenuEvent *event){

    selectedAction = settingRightClickMenu->exec(event->globalPos());

    if (selectedAction == homePageAction) {
        // 处理 "AI助手首页show出" 的点击事件
        Q_EMIT sigHomePageShow();
    } else if (selectedAction == disableAction) {
        // 处理 "禁用划词悬浮条" 的点击事件
        Q_EMIT sigDisableScribe();
    }else if (selectedAction == settingAction) {
        // 处理 "设置页面弹出" 的点击事件
        Q_EMIT sigSettingShow();
    }else if (selectedAction == closeAction) {
        // 处理 "划词界面关闭" 的点击事件
        Q_EMIT sigSelectWidgetClose();
    }
}
