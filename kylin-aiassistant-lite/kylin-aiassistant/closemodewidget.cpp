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


#include "closemodewidget.h"
#include <QRadioButton>
#include <QCheckBox>
#include <QDebug>
#include <QVariant>
#include <QApplication>

#include <configmanager.h>
#include <global/kaglobal.h>

#include "windowhelper.h"

CloseModeWidget::CloseModeWidget(QWidget *parent)
    : QDialog{parent}
{
    initLayout();
}

void CloseModeWidget::initLayout()
{
    this->setWindowTitle(QString(" "));
    // 设置无边框窗口
    this->setFixedSize(370,192);

#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *ukuiWindowHelper = new UkuiWindowHelper(this);
    ukuiWindowHelper->removeTitleBar();
#else
    kdk::UkuiStyleHelper::self()->removeHeader(this);
#endif

    // 焦点不在窗口上时鼠标悬浮可显示tooltips
    this->setAttribute(Qt::WA_AlwaysShowToolTips);

    // 创建垂直布局作为主布局
    mainLayout = new QVBoxLayout();

    closeButton = new QPushButton;
    closeButton->setAccessibleName("kylin-aiassistant_QPushButton_closeButton");
    closeButton->setFlat(true);
    closeButton->setFixedSize(32, 32);
    closeButton->setProperty("isWindowButton", 0x2);
    closeButton->setProperty("useIconHighlightEffect", 0x8);
    closeButton->setIcon(QIcon::fromTheme("window-close-symbolic"));
    closeButton->setToolTip(tr("Close"));
    connect(closeButton,&QPushButton::clicked,this,[=](){
        ConfigManager::getInstance()->updateConfig("is-hide-tray", true);
        ConfigManager::getInstance()->updateConfig("is-need-tips", true);
        emit sigCurrentCloseMode(true);
        this->close();
    });

    titleWidget = new QWidget(this);
    titleWidget->setAccessibleName("kylin-aiassistant_QWidget_titleWidget");
    titleWidget->setFixedSize(370, 40);

    titleHLayout = new QHBoxLayout(titleWidget);
    titleHLayout->setContentsMargins(0, 0, 5, 0);
    titleHLayout->addStretch();
    titleHLayout->addWidget(closeButton);

    mainLayout->addLayout(titleHLayout);


    // 创建标题标签
    QHBoxLayout *tipsLayout = new QHBoxLayout;

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setAccessibleName("kylin-aiassistant_QLabel_iconLabel");
    iconLabel->setFixedSize(24, 24);
    QPixmap pixmap(":/icon/icon/tips.svg");
    pixmap.scaled(iconLabel->size(), Qt::KeepAspectRatio,Qt::SmoothTransformation);
    iconLabel->setScaledContents(true);
    iconLabel->setPixmap(pixmap);//设置图标

    QLabel *tipsLabel = new QLabel(tr("Please select the status after closing: "));
    tipsLabel->setStyleSheet("font-size:14px;");
    tipsLayout->setContentsMargins(24,38,24,0);
    tipsLayout->setSpacing(8);
    tipsLayout->addWidget(iconLabel);
    tipsLayout->addWidget(tipsLabel);
    mainLayout->addLayout(tipsLayout);

    // 创建水平布局用于放置单选按钮
    QHBoxLayout *radioLayout = new QHBoxLayout();
    hideWindowsRadio = new QRadioButton(tr("Hide windows"));
    hideWindowsRadio->setStyleSheet("font-size:14px;");
    connect(hideWindowsRadio,&QRadioButton::clicked,this,[&](){
        ConfigManager::getInstance()->updateConfig("is-hide-tray",true);
        emit sigCurrentCloseMode(true);
    });
    exitRadio = new QRadioButton(tr("Exit directly"));
    exitRadio->setStyleSheet("font-size:14px;");
    connect(exitRadio,&QRadioButton::clicked,this,[&](){
        ConfigManager::getInstance()->updateConfig("is-hide-tray",false);
        emit sigCurrentCloseMode(false);
    });
    //get current message
    bool isHideTray = ConfigManager::getInstance()->getConfigValue("is-hide-tray", true).toBool();
    if(isHideTray){
        hideWindowsRadio->setChecked(true);
    }
    else {
        exitRadio->setChecked(true);
    }
    radioLayout->setContentsMargins(60,0,24,0);
    radioLayout->setSpacing(8);
    radioLayout->addWidget(hideWindowsRadio);
    radioLayout->addWidget(exitRadio);

    mainLayout->addLayout(radioLayout);
    mainLayout->addStretch();
    // 创建水平布局用于放置复选框和按钮
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    noRemindCheckBox = new QCheckBox(tr("No remind"));
    /* 自定义checkbox的选择框样式 */
    noRemindCheckBox->setStyleSheet("font-size:14px;");
    connect(noRemindCheckBox,&QCheckBox::clicked,this,[&](bool isChecked){
        if(isChecked) {
            ConfigManager::getInstance()->updateConfig("is-need-tips", false);
        }
        else {
            ConfigManager::getInstance()->updateConfig("is-need-tips", true);
        }
    });
    cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setFixedSize(94,36);
    connect(cancelButton,&QPushButton::clicked,this,[&](){
        reject();
    });
    okButton = new QPushButton(tr("Confirm"));
    okButton->setAccessibleName("kylin-aiassistant_QPushButton_okButton");
    okButton->setFixedSize(94,36);
    okButton->setProperty("isImportant", true);
    okButton->setDefault(true);
    connect(okButton,&QPushButton::clicked,this,[&](){
        this->accept();
        bool isHideTray = ConfigManager::getInstance()->getConfigValue("is-hide-tray", true).toBool();
        if(isHideTray)
        {
            emit sigHideCurrentApp();
        }
        else
        {
            qWarning() << "[SHUTDOWN] CloseModeWidget confirm exit -> qApp->quit()";
            qApp->quit();
        }
    });
    bottomLayout->setContentsMargins(24,8,24,0);
    bottomLayout->setSpacing(8);
    bottomLayout->addWidget(noRemindCheckBox);
    bottomLayout->addWidget(cancelButton);
    bottomLayout->addWidget(okButton);
    mainLayout->addLayout(bottomLayout);
    mainLayout->setContentsMargins(0,10,0,20);
    this->setStyleSheet("border:solder 1px rgba(38,38,38,0.15),background:white;");
    this->setLayout(mainLayout);
    connect(this,&CloseModeWidget::rejected,this,[&](){
        ConfigManager::getInstance()->updateConfig("is-hide-tray", true);
        ConfigManager::getInstance()->updateConfig("is-need-tips", true);
        emit sigCurrentCloseMode(true);
    });
}

void CloseModeWidget::updateMsg()
{
    bool isHideTray = ConfigManager::getInstance()->getConfigValue("is-hide-tray", true).toBool();
    if(isHideTray){
        hideWindowsRadio->setChecked(true);
    }
    else {
        exitRadio->setChecked(true);
    }
    bool isNeedTips = ConfigManager::getInstance()->getConfigValue("is-need-tips").toBool();
    noRemindCheckBox->setChecked(!isNeedTips);
}
