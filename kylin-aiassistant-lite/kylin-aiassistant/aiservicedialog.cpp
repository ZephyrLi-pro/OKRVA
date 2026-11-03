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
#include "aiservicedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QApplication>
#include <QProcess>
#include <ukui-log4qt.h>

#include "windowhelper.h"

AiServiceDialog::AiServiceDialog(QWidget *parent) : QDialog(parent)
{
#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *ukuiWindowHelper = new UkuiWindowHelper(this);
    ukuiWindowHelper->removeTitleBar();
#else
    kdk::UkuiStyleHelper::self()->removeHeader(this);
#endif

    initLayout();
    initConnect();

}

AiServiceDialog::~AiServiceDialog()
{

}

void AiServiceDialog::setLowVersionText()
{
    titleLabel->setText(tr("The current AI subsystem version is relatively low"));
    detailLabel->setText(tr("The AI subsystem version is outdated, causing abnormal functioning of the Qi Ling. Please go to System Settings > AI Module Management, update the AI subsystem, and try again."));
    return;
}

void AiServiceDialog::initLayout()
{
    this->setWindowTitle(QString(" "));
    this->setFixedSize(452, 212);
    this->setStyleSheet("QDialog { background-color: white; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 24);
    mainLayout->setSpacing(8);

    QHBoxLayout *tipsLayout = new QHBoxLayout(this);
    QHBoxLayout *contentLayout = new QHBoxLayout(this);
    contentLayout->setContentsMargins(20, 0, 0, 0);


    closeButton = new QPushButton;
    closeButton->setAccessibleName("kylin-aiassistant_QPushButton_closeButton");
    closeButton->setFlat(true);
    closeButton->setFixedSize(32, 32);
    closeButton->setProperty("isWindowButton",  0x2);
    closeButton->setProperty("useIconHighlightEffect", 0x8);
    closeButton->setIcon(QIcon::fromTheme("window-close-symbolic"));
    closeButton->setToolTip(tr("Close"));

    QWidget *titleWidget = new QWidget();
    titleWidget->setAccessibleName("kylin-aiassistant_QWidget_titleWidget");
    titleWidget->setFixedSize(452, 40);
    QHBoxLayout *titleHLayout = new QHBoxLayout(titleWidget);
    titleHLayout->setContentsMargins(8, 0, 4, 0);
    titleHLayout->addStretch();
    titleHLayout->addWidget(closeButton);
    mainLayout->addWidget(titleWidget);

    iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(":/icon/icon/hint.svg").scaled(16, 16));
    iconLabel->setContentsMargins(28, 2, 0, 144);
    titleLabel = new QLabel(tr("AI service unavailable"));
    titleLabel->setFixedSize(380, 20);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    tipsLayout->setContentsMargins(24, 8, 24, 0);
    tipsLayout->addWidget(iconLabel);
    tipsLayout->addWidget(titleLabel);

    mainLayout->addLayout(tipsLayout);

    detailLabel = new QLabel(tr("The AI subsystem is not detected. The AI service of the application cannot be used. Please go to System Settings > AI Module Management page to install it."));
    detailLabel->setWordWrap(true);
    detailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    contentLayout->setContentsMargins(48, 0, 24, 0);
    contentLayout->addWidget(detailLabel);
    mainLayout->addLayout(contentLayout);

    QHBoxLayout *buttonLayout = new QHBoxLayout(this);
    cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setAccessibleName("kylin-aiassistant_QQPushButton_cancelButton");
    cancelButton->setFixedSize(94, 36);

    goButton = new QPushButton(tr("go now"));
    goButton->setAccessibleName("kylin-aiassistant_QPushButton_okButton");

    goButton->setProperty("isImportant", true);
    goButton->setDefault(true);

    buttonLayout->setContentsMargins(24, 0, 24, 0);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(goButton);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void AiServiceDialog::initConnect()
{
    connect(cancelButton, &QPushButton::clicked, this, &AiServiceDialog::reject);
    connect(goButton, &QPushButton::clicked, this, &AiServiceDialog::openUkuiControlCenter);
    connect(closeButton, &QPushButton::clicked, this, &AiServiceDialog::reject);
}

void AiServiceDialog::openUkuiControlCenter()
{
    QProcess process;
    QString command = "ukui-control-center";
    QStringList arguments;
    arguments << "-m" << "aisubsystem";
    process.startDetached(command, arguments);
    process.waitForFinished(-1);
    // 检查进程是否正常退出
    if (process.exitStatus() == QProcess::NormalExit) {
        KyInfo() << "ukui-control-center successfully!";
    } else {
        KyInfo() << "Failed  to open ukui-control-center";
    }
    this->close();
}
