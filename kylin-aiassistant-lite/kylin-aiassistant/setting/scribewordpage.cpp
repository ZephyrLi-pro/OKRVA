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
#include "scribewordpage.h"
#include "global/kaglobal.h"
#include "commandtemplatelist.h"
#include "clickfixlabel.h"

#include <QLabel>
#include <QSharedPointer>
#include <QQuickWidget>
#include <QQmlContext>
#include <QLineEdit>
#include <QFrame>
#include <QVBoxLayout>

#include "kswitchbutton.h"

using namespace kdk;

class ScribeWordPage::Private
{
public:
    QLabel *m_titleLabel = nullptr;
    QFrame *m_pageGroup = nullptr;
    QVBoxLayout *m_pageGroupLayout = nullptr;

    QFrame *m_enableFrame = nullptr;
    QLabel *m_enableLabel = nullptr;
    KSwitchButton *m_enableBtn =nullptr;  // 允许启用悬浮条

    QFrame *m_shortcutFrame = nullptr;
    QLabel *m_shortcutLabel = nullptr;
    QLineEdit *m_shortcutFixLabel =nullptr;

    bool isScribeEnable = false;
};

ScribeWordPage::ScribeWordPage(QWidget *parent)
    : QWidget{parent},
      d_ptr(make_shared<Private>())
{
    initLayout();
    //初始化信号槽
    initConnection();

}

ScribeWordPage::~ScribeWordPage()
{

}

void ScribeWordPage::initLayout()
{
    QVBoxLayout *base_vlayout = new QVBoxLayout(this);
    base_vlayout->setContentsMargins(16, 0, 16, 0);
    d_ptr->m_pageGroup = new QFrame(this);
    d_ptr->m_pageGroup->setAccessibleName("kylin-aiassistant_QFrame_d_ptr->m_pageGroup");
    d_ptr->m_pageGroupLayout = new QVBoxLayout(d_ptr->m_pageGroup);
    d_ptr->m_pageGroupLayout->setContentsMargins(0, 0, 0, 0);
    d_ptr->m_pageGroupLayout->setSpacing(1);

    // 标题
    d_ptr->m_titleLabel = new QLabel;
    d_ptr->m_titleLabel->setAccessibleName("kylin-aiassistant_QLabel_d_ptr->m_titleLabel");
    d_ptr->m_titleLabel->setText(tr("Word Selection Floating Bar"));
    d_ptr->m_titleLabel->setContentsMargins(16, 0, 0, 0);
    // QFont font("Noto Sans CJK SC", 11);
    // d_ptr->m_titleLabel->setFont(font);

    // 启用划词悬浮条
    d_ptr->m_enableFrame = new QFrame;

    d_ptr->m_enableFrame->setAccessibleName("kylin-aiassistant_QFrame_d_ptr->m_enableFrame");
    d_ptr->m_enableFrame->setMinimumSize(QSize(496, 60));
    d_ptr->m_enableFrame->setMaximumSize(QSize(16777215, 60));
    d_ptr->m_enableFrame->setFrameShape(QFrame::Box);
    d_ptr->m_enableFrame->setObjectName(QString("enableFrame"));


    QHBoxLayout *enableHLayout = new QHBoxLayout;
    enableHLayout->setContentsMargins(16, 10, 16, 10);
    d_ptr->m_enableFrame->setLayout(enableHLayout);

    d_ptr->m_enableLabel = new QLabel(tr("Enable Floating Bar for Word Selection"), this);
    d_ptr->m_enableLabel->setAccessibleName("kylin-aiassistant_QLabel_d_ptr->m_enableLabel");
    d_ptr->m_enableLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    d_ptr->m_enableBtn = new KSwitchButton;
    d_ptr->m_enableBtn->setAccessibleName("kylin-aiassistant_KSwitchButton_d_ptr->m_enableBtn");
    d_ptr->m_enableBtn->setObjectName("enable");

    enableHLayout->addWidget(d_ptr->m_enableLabel);
    enableHLayout->addStretch();
    enableHLayout->addWidget(d_ptr->m_enableBtn);

    //添加gsetting判断
    d_ptr->isScribeEnable = ConfigManager::getInstance()->getConfigValue("is-stroke-word").toBool();
    d_ptr->m_enableBtn->setChecked(d_ptr->isScribeEnable);

    // 划词悬浮条快捷键
    d_ptr->m_shortcutFrame = new QFrame;
    d_ptr->m_shortcutFrame->setAccessibleName("kylin-aiassistant_QFrame_d_ptr->m_shortcutFrame");
    d_ptr->m_shortcutFrame->setMinimumSize(QSize(496, 60));
    d_ptr->m_shortcutFrame->setMaximumSize(QSize(16777215, 60));
    d_ptr->m_shortcutFrame->setFrameShape(QFrame::Box);
    d_ptr->m_shortcutFrame->setObjectName(QString("shortcutFrame"));


    QHBoxLayout *shortcutHLayout = new QHBoxLayout;
    shortcutHLayout->setContentsMargins(16, 10, 16, 10);
    d_ptr->m_shortcutFrame->setLayout(shortcutHLayout);

    d_ptr->m_shortcutLabel = new QLabel(tr("Shortcut Keys for Word Selection Floating Bar"), d_ptr->m_shortcutFrame);
    d_ptr->m_shortcutLabel->setAccessibleName("kylin-aiassistant_QLabel_d_ptr->m_shortcutLabel");
    d_ptr->m_shortcutLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d_ptr->m_shortcutLabel->setFixedWidth(230);

    d_ptr->m_shortcutFixLabel = new QLineEdit();
    d_ptr->m_shortcutFixLabel->setAccessibleName("kylin-aiassistant_ClickFixLabel_d_ptr->m_shortcutFrame");
    d_ptr->m_shortcutFixLabel->setFixedWidth(228);
    d_ptr->m_shortcutFixLabel->setText(tr("Super  Alt  C"));
    d_ptr->m_shortcutFixLabel->setReadOnly(true);

    shortcutHLayout->addWidget(d_ptr->m_shortcutLabel);
    shortcutHLayout->addStretch();
    shortcutHLayout->addWidget(d_ptr->m_shortcutFixLabel);

    // 总布局
    d_ptr->m_pageGroupLayout->addWidget(d_ptr->m_enableFrame);
    d_ptr->m_pageGroupLayout->addWidget(d_ptr->m_shortcutFrame);

    base_vlayout->addWidget(d_ptr->m_titleLabel);
    base_vlayout->setSpacing(8);
    base_vlayout->addWidget(d_ptr->m_pageGroup);
    base_vlayout->addStretch();

}

void ScribeWordPage::initConnection()
{
    connect(d_ptr->m_enableBtn, &KSwitchButton::clicked, this, &ScribeWordPage::toggleScribeSetting);
}

void ScribeWordPage::disableScribeWords()
{
    if(d_ptr->m_enableBtn && d_ptr->isScribeEnable)
    {
        d_ptr->m_enableBtn->setChecked(false);
        d_ptr->isScribeEnable = false;
    }
}

void ScribeWordPage::toggleScribeSetting()
{

    d_ptr->isScribeEnable = !d_ptr->isScribeEnable;
    Globals::getinstance()->setScribeEnabled(d_ptr->isScribeEnable);
    d_ptr->m_enableBtn->setChecked(d_ptr->isScribeEnable);
    if(d_ptr->isScribeEnable)
    {
        Q_EMIT sigEnableScribeWordsSwitch();
    }else{
        Q_EMIT sigDisableScribeWordsSwitch();
    }
}
