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
#include "custommessagebox.h"

#include <QResizeEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QGuiApplication>
#include <QScreen>

#include "windowhelper.h"

CustomMessageBox::CustomMessageBox(QWidget *parent)
    : QDialog(parent), m_clickedButton(0)
{
    initLayout();
    initConnect();
    checkTheme();
}

void CustomMessageBox::initLayout()
{
    this->setWindowTitle(QString(" "));
    setFixedWidth(376);
    setWindowFlags(Qt::Dialog);

#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *ukuiWindowHelper = new UkuiWindowHelper(this);
    ukuiWindowHelper->removeTitleBar();
#else
    kdk::UkuiStyleHelper::self()->removeHeader(this);
#endif

    // 焦点不在窗口上时鼠标悬浮可显示tooltips
    this->setAttribute(Qt::WA_AlwaysShowToolTips);

    closeButton = new QPushButton;
    closeButton->setAccessibleName("kylin-aiassistant_QPushButton_closeButton");
    closeButton->setFlat(true);
    closeButton->setFixedSize(30, 30);
    closeButton->setProperty("isWindowButton",  0x2);
    closeButton->setProperty("useIconHighlightEffect", 0x8);
    closeButton->setIcon(QIcon::fromTheme("window-close-symbolic"));
    closeButton->setToolTip(tr("Close"));

    QWidget *titleWidget = new QWidget();
    titleWidget->setAccessibleName("kylin-aiassistant_QWidget_titleWidget");
    titleWidget->setFixedSize(376, 40);
    QHBoxLayout *titleHLayout = new QHBoxLayout(titleWidget);
    titleHLayout->setContentsMargins(8, 0, 4, 0);
    titleHLayout->addStretch();
    titleHLayout->addWidget(closeButton);

    // 提示
    iconLabel = new QLabel(this);
    iconLabel->setAccessibleName("kylin-aiassistant_QLabel_iconLabel");
    iconLabel->setFixedSize(24, 24);
    iconLabel->setScaledContents(true);

    QPixmap tipsPixmap(":/icon/icon/tips.svg");
    if (!tipsPixmap.isNull()) {
        iconLabel->setPixmap(tipsPixmap);
    }

    messageLabel = new QLabel(this);
    messageLabel->setAccessibleName("kylin-aiassistant_QLabel_messageLabel");
    messageLabel->setFixedWidth(304);
    messageLabel->setWordWrap(true);

    QVBoxLayout *iconLayout = new QVBoxLayout();
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setSpacing(0);
    iconLayout->addWidget(iconLabel);
    iconLayout->addStretch();

    QVBoxLayout *tipLayout = new QVBoxLayout();
    tipLayout->setContentsMargins(0, 1, 0, 1);
    tipLayout->setSpacing(0);
    tipLayout->addWidget(messageLabel);
    tipLayout->addStretch();

    QHBoxLayout *tipsLayout = new QHBoxLayout();
    tipsLayout->setContentsMargins(24, 8, 24, 0);
    tipsLayout->addLayout(iconLayout);
    tipsLayout->addSpacing(2);
    tipsLayout->addLayout(tipLayout);
    tipsLayout->addStretch();

    // 次要文字
    informativeLabel = new QLabel(this);
    informativeLabel->setAccessibleName("kylin-aiassistant_QLabel_informativeLabel");
    informativeLabel->setFixedWidth(304);
    informativeLabel->setWordWrap(true);
    informativeLabel->setVisible(false);
//    QPalette palette = informativeLabel->palette();
//    palette.setColor(QPalette::WindowText, Qt::gray);  // 使用Qt预定义灰色
//    informativeLabel->setPalette(palette);
    informativeLabel->setStyleSheet("color: rgba(0,0,0,0.6);");

    QHBoxLayout *infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(50, 0, 24, 0);
    infoLayout->addWidget(informativeLabel);
    infoLayout->addStretch();

    // 按钮,待做QCheckBox
    cancelButton = new CustomPushButton(this);
    cancelButton->setAccessibleName("kylin-aiassistant_QQPushButton_cancelButton");
    cancelButton->setFixedSize(88, 36);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setVisible(false);

    okButton = new CustomPushButton(this);
    okButton->setAccessibleName("kylin-aiassistant_QPushButton_okButton");
    okButton->setFixedSize(88, 36);
    okButton->setText(tr("Delete"));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(24, 0, 24, 24);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(titleWidget);
    mainLayout->addLayout(tipsLayout);
    mainLayout->addLayout(infoLayout);
    mainLayout->addSpacing(32);
    mainLayout->addLayout(buttonLayout);
    this->setLayout(mainLayout);
}

void CustomMessageBox::initConnect()
{
    connect(closeButton, &QPushButton::clicked, this, &CustomMessageBox::onCancelClicked);
    connect(cancelButton, &QPushButton::clicked, this, &CustomMessageBox::onCancelClicked);
    connect(okButton, &QPushButton::clicked, this, &CustomMessageBox::onOkClicked);
}

void CustomMessageBox::setMessage(const QString &message) {
    messageLabel->setText(message);

    return;
}

void CustomMessageBox::setButtons(int buttons) {
    okButton->setVisible(buttons & OkButton);
    cancelButton->setVisible(buttons & CancelButton);
}

void CustomMessageBox::setInformativeText(const QString &text)
{
    informativeLabel->setText(text);
    informativeLabel->setVisible(true);

    return;
}

void CustomMessageBox::addButton(const QString &text, MessageBoxButton role)
{
    switch (role) {
    case OkButton:
        okButton->setVisible(true);
        okButton->setText(text);
        break;
    case CancelButton:
        cancelButton->setVisible(true);
        cancelButton->setText(text);
        break;
    default:
        break;
    }
    return;
}

void CustomMessageBox::setIcon(Icon role)
{
    switch (role) {
    case Information:
        iconLabel->setPixmap(QIcon::fromTheme("dialog-info").pixmap(16, 16));
        break;
    case Warning:
        iconLabel->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(16, 16));
        break;
    case Critical:
        iconLabel->setPixmap(QIcon::fromTheme("dialog-error").pixmap(16, 16));
        break;
    case Question:
        iconLabel->setPixmap(QIcon::fromTheme("dialog-question").pixmap(16, 16));
        break;
    default:
        break;
    }
    return;
}

void CustomMessageBox::setDefaulteButton(CustomMessageBox::MessageBoxButton role)
{
    switch (role) {
    case OkButton:
        okButton->setProperty("isImportant", true);
        okButton->setDefault(true);
        break;
    case CancelButton:
        cancelButton->setProperty("isImportant", true);
        cancelButton->setDefault(true);
        break;
    default:
        break;
    }
    return;
}

int CustomMessageBox::clickedButton() const {
    return m_clickedButton;
}

int CustomMessageBox::exec()
{
    return QDialog::exec();
}

void CustomMessageBox::onOkClicked() {
    m_clickedButton = OkButton;
    accept();
}

void CustomMessageBox::onCancelClicked() {
    m_clickedButton = CancelButton;
    reject();
}

void CustomMessageBox::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    // Center the messageLabel within the dialog
    //messageLabel->setGeometry(0, 0, width(), messageLabel->height());
}

void CustomMessageBox::showEvent(QShowEvent *event)
{
    centerToParent(); // 确保每次显示时都居中
    QDialog::showEvent(event);
}

void CustomMessageBox::changeTheme()
{
    QPalette p = this->palette();
    QColor color = p.color(QPalette::Base);
    p.setColor(QPalette::Window, color);
    this->setPalette(p);
}

void CustomMessageBox::checkTheme()
{
    changeTheme();

    if(QGSettings::isSchemaInstalled("org.ukui.style")) {
        themeData = new QGSettings("org.ukui.style");
        connect(themeData, &QGSettings::changed, this, [=](const QString &key) {
            if(key == "styleName") {
                changeTheme();
            }
            else if (key == "systemFontSize") {
                centerToParent();
            }
        });
    }

    return;
}

int CustomMessageBox::getLableHeight(QLabel* label)
{
    int lineCount = 1;
    QRect textRect = label->contentsRect();
    QFontMetrics fm(label->font());

    QString text = label->text();
    QRect neededRect = fm.boundingRect(textRect,
                                      Qt::TextWordWrap | label->alignment(),
                                      text);

    lineCount = (neededRect.height() + 1) / fm.height();
    return  lineCount * fm.height();
}

void CustomMessageBox::updateFixedSize()
{
    int realHeight = 150;
    int tipHeight = getLableHeight(messageLabel);
    int infoHeight = 0;
    // 默认标准提示词时显示的，只有infomative文本是可选
    if (informativeLabel->isVisible()) {
        infoHeight = getLableHeight(informativeLabel);
    }
    realHeight += tipHeight + infoHeight;
    setFixedSize(376, realHeight);

    return;
}

void CustomMessageBox::centerToParent()
{
    updateFixedSize();
    if (parentWidget()) {
        // 相对于父窗口居中
        move(parentWidget()->frameGeometry().center() - rect().center());
    } else {
        // 如果没有父窗口，则居中到屏幕
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            move(screen->geometry().center() - rect().center());
        }
    }

    return;
}

CustomPushButton::CustomPushButton(const QString &text, QWidget *parent)
{
    updateElidedText();
    checkTheme();
}

CustomPushButton::CustomPushButton(QWidget *parent)
{
    checkTheme();
}

void CustomPushButton::setText(const QString &text)
{
    m_fullText = text;
    updateElidedText();

    return;
}

void CustomPushButton::checkTheme()
{
    if(QGSettings::isSchemaInstalled("org.ukui.style")) {
        QGSettings *themeData = new QGSettings("org.ukui.style");
        connect(themeData, &QGSettings::changed, this, [=](const QString &key) {
            if (key == "systemFontSize") {
                updateElidedText();
            }
        });
    }

    return;
}

void CustomPushButton::updateElidedText()
{
    // 只有当有文本时才进行计算
    if (!m_fullText.isEmpty()) {
        // 计算省略后的文本
        QString elidedText = fontMetrics().elidedText(
            m_fullText,
            Qt::ElideRight,
            width() - 12
        );
        // 调用父类的setText来显示处理后的文本
        QPushButton::setText(elidedText);
        if (elidedText != m_fullText) {
            setToolTip(m_fullText);
        }
    } else {
        QPushButton::setText("");
    }

    return;
}
