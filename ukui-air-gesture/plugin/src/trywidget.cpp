/*
 * Copyright (C) 2026, KylinSoft Co., Ltd.
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

#include "trywidget.h"
#include "airgestureicons.h"

#include <QDBusReply>
#include <QDebug>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <ukuiwindowhelper.h>

TryWidget::TryWidget(QWidget *parent, AirGestureType gestureType)
    : QDialog(parent)
    , m_gestureType(gestureType)
{
    initGSettings();

    // 设置窗口属性
    UkuiWindowHelper *windowHelper = new UkuiWindowHelper(this);
    windowHelper->removeTitleBar();
    windowHelper->setWindowRole(UkuiWindowHelper::WindowRole::Dialog);
    UkuiWindowHelper::States state = windowHelper->defaultState();
    state.setFlag(UkuiWindowHelper::State::Movable, false);
    windowHelper->setWindowState(state);
    setModal(true);

    // 设置窗口尺寸
    setFixedSize(960, 620);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // 创建主内容区域
    QWidget *topWidget = new QWidget(this);
    topWidget->setFixedSize(960, 543);
    QVBoxLayout *topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    if (gestureType == AirGestureType::Wave_Vertical) {
        topLayout->addWidget(createVerticalWaveTryWidget());
    } else if (gestureType == AirGestureType::Wave_Horizontal) {
        topLayout->addWidget(createHorizontalWaveTryWidget());
    } else if (gestureType == AirGestureType::Fist) {
        topLayout->addWidget(createFistTryWidget());
    }
    layout->addWidget(topWidget);
    
    // 创建底部按钮区域
    QWidget *bottomWidget = new QWidget(this);
    bottomWidget->setFixedSize(960, 77);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(840, 16, 24, 24);
    bottomLayout->setSpacing(0);
    QPushButton *closeButton = new QPushButton(tr("Close"), bottomWidget);
    closeButton->setFixedSize(96, 36);
    closeButton->setProperty("isImportant", 0x1);
    connect(closeButton, &QPushButton::clicked, this, &TryWidget::close);
    bottomLayout->addWidget(closeButton);
    layout->addWidget(bottomWidget);
}

void TryWidget::initGSettings()
{
    if (!QGSettings::isSchemaInstalled(AirGestureSettings::kStyleSchema)) {
        return;
    }

    m_pStyleGSettings = new QGSettings(AirGestureSettings::kStyleSchema, "", this);
    m_currentStyle = m_pStyleGSettings->get(AirGestureSettings::kStyleKey).toString();

    connect(m_pStyleGSettings, &QGSettings::changed, this, [this](const QString &key) {
        if (key != AirGestureSettings::kStyleKey) {
            return;
        }
        m_currentStyle = m_pStyleGSettings->get(AirGestureSettings::kStyleKey).toString();
        if (m_pPromptIconLabel != nullptr) {
            QPixmap pixmap = m_pPromptIconLabel->pixmap(Qt::ReturnByValue);
            if (!pixmap.isNull()) {
                m_pPromptIconLabel->setPixmap(invertPixmapColors(pixmap));
            }
        }
         if (m_pTopIconLabel != nullptr) {
            QPixmap pixmap = m_pTopIconLabel->pixmap(Qt::ReturnByValue);
            if (!pixmap.isNull()) {
                m_pTopIconLabel->setPixmap(invertPixmapColors(pixmap));
            }
        }
    });
}

QWidget *TryWidget::createVerticalWaveTryWidget()
{
    QWidget *container = new QWidget(this);
    container->setFixedSize(960, 543);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(68, 40, 68, 0);

    m_pVerticalScrollArea = new QScrollArea(container);
    m_pVerticalScrollArea->setFixedSize(824, 503);
    QLabel *label = new QLabel(m_pVerticalScrollArea);
    label->setFixedSize(800, 1584);
    label->setPixmap(QPixmap(":/data/picture/滑动内容.svg"));
    m_pVerticalScrollArea->setWidget(label);
    layout->addWidget(m_pVerticalScrollArea);

    return container;
}

QWidget *TryWidget::createHorizontalWaveTryWidget()
{
    QWidget *container = new QWidget(this);
    container->setFixedSize(960, 543);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(40, 40, 40, 0);
    layout->setSpacing(0);

    m_pHorizontalPage = new SwitchPage(container);
    m_pHorizontalPage->setFixedSize(880, 487);

    QLabel *label1 = new QLabel(m_pHorizontalPage);
    label1->setFixedSize(880, 451);
    label1->setPixmap(QPixmap(":/data/picture/banner1.png"));
    m_pHorizontalPage->addWidget(label1);

    QLabel *label2 = new QLabel(m_pHorizontalPage);
    label2->setFixedSize(880, 451);
    label2->setPixmap(QPixmap(":/data/picture/banner2.png"));
    m_pHorizontalPage->addWidget(label2);

    QLabel *label3 = new QLabel(m_pHorizontalPage);
    label3->setFixedSize(880, 451);
    label3->setPixmap(QPixmap(":/data/picture/banner3.png"));
    m_pHorizontalPage->addWidget(label3);

    layout->addWidget(m_pHorizontalPage);
    layout->addSpacing(16);
    layout->addWidget(new kdk::KHLineFrame(this));

    return container;
}

QWidget *TryWidget::createFistTryWidget()
{
    QWidget *container = new QWidget(this);
    container->setFixedSize(960, 543);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 542, 0, 0);
    layout->setSpacing(0);

    QLinearGradient gradient(0, 0, 0, container->height());
    
    if (m_pStyleGSettings != nullptr) {
        QString styleName = m_pStyleGSettings->get(AirGestureSettings::kStyleKey).toString();
        if (styleName == "ukui-dark") {
            gradient.setColorAt(0, QColor(28, 30, 34));
            gradient.setColorAt(1, QColor(28, 30, 34));
        } else {
            gradient.setColorAt(0, QColor(238, 243, 255));
            gradient.setColorAt(1, QColor(255, 251, 239));
        }
    } else {
        gradient.setColorAt(0, QColor(238, 243, 255));
        gradient.setColorAt(1, QColor(255, 251, 239));
    }

    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(gradient));
    container->setAutoFillBackground(true);
    container->setPalette(palette);

    layout->addWidget(new kdk::KHLineFrame(this));
    
    return container;
}

void TryWidget::updateIconDialog(const QString &palmDirection)
{
    if (m_pIconDialog == nullptr) {
        m_pIconDialog = new QDialog(this);
        m_pIconDialog->setFixedSize(64, 64);
        UkuiWindowHelper *windowHelper = new UkuiWindowHelper(m_pIconDialog);
        windowHelper->removeTitleBar();
        windowHelper->setWindowRole(UkuiWindowHelper::WindowRole::Dialog);
        UkuiWindowHelper::States state = windowHelper->defaultState();
        state.setFlag(UkuiWindowHelper::State::Movable, false);
        windowHelper->setWindowState(state);

        m_pTopIconLabel = new QLabel(m_pIconDialog);
        m_pTopIconLabel->setFixedSize(48, 48);
        QVBoxLayout *layout = new QVBoxLayout(m_pIconDialog);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->addWidget(m_pTopIconLabel);

        const QPoint parentGlobalTopLeft = mapToGlobal(QPoint(0, 0));
        const int iconX = parentGlobalTopLeft.x() + (width() - m_pIconDialog->width()) / 2;
        const int iconY = parentGlobalTopLeft.y() + 24;
        m_pIconDialog->move(iconX, iconY);
    }

    QString iconName;
    if (palmDirection == "up") {
        iconName = "hand-palm-up-24-symbolic";
    } else if (palmDirection == "down") {
        iconName = "hand-back-down-24-symbolic";
    } else if (palmDirection == "left") {
        iconName = "hand-palm-right-24-symbolic";
    } else if (palmDirection == "right") {
        iconName = "hand-back-left-24-symbolic";
    }

    QPixmap pixmap = airGestureIconPixmap(iconName, 48);
    if (m_currentStyle == "ukui-dark") {
        pixmap = invertPixmapColors(pixmap);
    }
    m_pTopIconLabel->setPixmap(pixmap);
}

void TryWidget::updatePromptDialog(bool withIcon, const QString &action)
{
    if (m_pPromptDialog == nullptr) {
        m_pPromptDialog = new QDialog(this);
        UkuiWindowHelper *windowHelper = new UkuiWindowHelper(m_pPromptDialog);
        windowHelper->removeTitleBar();
        windowHelper->setWindowRole(UkuiWindowHelper::WindowRole::Dialog);
        UkuiWindowHelper::States state = windowHelper->defaultState();
        state.setFlag(UkuiWindowHelper::State::Movable, false);
        state.setFlag(UkuiWindowHelper::State::KeepAbove, true);
        windowHelper->setWindowState(state);

        m_pPromptLayout = new QVBoxLayout(m_pPromptDialog);
        m_pPromptLayout->setSpacing(16);

        m_pPromptIconLabel = new QLabel(m_pPromptDialog);
        m_pPromptIconLabel->setFixedSize(64, 64);
        m_pPromptLayout->addWidget(m_pPromptIconLabel, 0, Qt::AlignHCenter);
        
        m_pPromptLabel = new QLabel(m_pPromptDialog);
        m_pPromptLabel->setAlignment(Qt::AlignHCenter);
        m_pPromptLayout->addWidget(m_pPromptLabel);
    }

    if (withIcon) {
        m_pPromptLayout->setContentsMargins(24, 24, 24, 24);
        m_pPromptIconLabel->show();
        m_pPromptLabel->setWordWrap(true);
        m_pPromptLabel->setFixedWidth(372);

        QPixmap pixmap;
        if (action == "down") {
            pixmap = airGestureIconPixmap(QStringLiteral("hand-palm-up-24-symbolic"), 64);
            m_pPromptLabel->setText(tr("Hold your palm with your fingertips pointing up, 20 to 70 cm in front of the camera, until the hand icon appears."));
        } else if (action == "up") {
            pixmap = airGestureIconPixmap(QStringLiteral("hand-back-down-24-symbolic"), 64);
            m_pPromptLabel->setText(tr("Hold the back of your hand with your fingertips pointing down, 20 to 70 cm in front of the camera, until the hand icon appears."));
        } else if (action == "left") {
            pixmap = airGestureIconPixmap(QStringLiteral("hand-palm-right-24-symbolic"), 64);
            m_pPromptLabel->setText(tr("Hold your palm with your fingertips pointing right, 20 to 70 cm in front of the camera, until the hand icon appears."));
        } else if (action == "right") {
            pixmap = airGestureIconPixmap(QStringLiteral("hand-back-left-24-symbolic"), 64);
            m_pPromptLabel->setText(tr("Hold the back of your hand with your fingertips pointing left, 20 to 70 cm in front of the camera, until the hand icon appears."));
        } else {
            pixmap = airGestureIconPixmap(QStringLiteral("hand-palm-up-24-symbolic"), 64);
            m_pPromptLabel->setText(tr("Hold your palm facing the camera, 20 to 70 cm in front of the camera, until the hand icon appears."));
        }
        if (m_currentStyle == "ukui-dark") {
            pixmap = invertPixmapColors(pixmap);
        }
        m_pPromptIconLabel->setPixmap(pixmap);
    } else {
        m_pPromptLayout->setContentsMargins(16, 16, 16, 16);
        m_pPromptIconLabel->hide();
        m_pPromptLabel->setWordWrap(true);
        m_pPromptLabel->setFixedWidth(372);
        if (action == "down") {
            m_pPromptLabel->setText(tr("Wave your palm up and down to scroll the page content to the bottom."));
        } else if (action == "up") {
            m_pPromptLabel->setText(tr("Wave your palm up and down to scroll the page content to the top."));
        } else if (action == "left") {
            m_pPromptLabel->setText(tr("Wave your palm backward to switch to the next image."));
        } else if (action == "right") {
            m_pPromptLabel->setText(tr("Point your fingertips to the left and wave backward to switch to the previous image."));
        } else if (action == "ptrsc") {
            m_pPromptLabel->setText(tr("Make a fist facing the screen to capture the full screen."));
        }
    }

    m_pPromptDialog->adjustSize();
}

void TryWidget::updatePromptGeometry()
{
    const QPoint localPos((width() - m_pPromptDialog->width()) / 2,
                          (height() - m_pPromptDialog->height()) / 2);
    m_pPromptDialog->move(mapToGlobal(localPos));
}

void TryWidget::showPromptDialog()
{
    updatePromptGeometry();
    m_pPromptDialog->show();
    m_pPromptDialog->raise();
}

void TryWidget::showIconDialog(const QString &palmDirection)
{
    updateIconDialog(palmDirection);
    m_pIconDialog->show();
    m_pIconDialog->raise();
}

void TryWidget::beginGestureTransition(const QString &nextPromptAction, TrialStage nextStage)
{
    m_pPromptDialog->hide();
    m_trialStage = TrialStage::TransitionPause;

    QTimer::singleShot(kGestureTransitionDelayMs, this, [this, nextPromptAction, nextStage]() {
        m_pIconDialog->hide();
        updatePromptDialog(true, nextPromptAction);
        showPromptDialog();
        m_trialStage = nextStage;
    });
}

QPixmap TryWidget::invertPixmapColors(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        return pixmap;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    image.invertPixels(QImage::InvertRgb);

    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(pixmap.devicePixelRatio());

    return result;
}

void TryWidget::startTrialMode()
{
    QDBusInterface iface(AirGestureDBus::serviceName,
                         AirGestureDBus::objectPath,
                         AirGestureDBus::interfaceName,
                         QDBusConnection::sessionBus());

    QDBusReply<void> reply = iface.call(QStringLiteral("startTrial"), gestureTypeToString(m_gestureType));
    if (!reply.isValid()) {
        qWarning() << "Failed to start trial mode:" << reply.error().message();
        return;
    }

    QDBusConnection::sessionBus().connect(AirGestureDBus::serviceName,
                                          AirGestureDBus::objectPath,
                                          AirGestureDBus::interfaceName,
                                          QStringLiteral("palmDetected"),
                                          this,
                                          SLOT(onPalmDetected(QString)));

    QDBusConnection::sessionBus().connect(AirGestureDBus::serviceName,
                                          AirGestureDBus::objectPath,
                                          AirGestureDBus::interfaceName,
                                          QStringLiteral("palmLost"),
                                          this,
                                          SLOT(onPalmLost()));

    QDBusConnection::sessionBus().connect(AirGestureDBus::serviceName,
                                          AirGestureDBus::objectPath,
                                          AirGestureDBus::interfaceName,
                                          QStringLiteral("trialGesturePerformed"),
                                          this,
                                          SLOT(onTrialGesturePerformed(QString)));
}

void TryWidget::stopTrialMode()
{
    QDBusConnection::sessionBus().disconnect(AirGestureDBus::serviceName,
                                             AirGestureDBus::objectPath,
                                             AirGestureDBus::interfaceName,
                                             QStringLiteral("palmDetected"),
                                             this,
                                             SLOT(onPalmDetected(QString)));

    QDBusConnection::sessionBus().disconnect(AirGestureDBus::serviceName,
                                             AirGestureDBus::objectPath,
                                             AirGestureDBus::interfaceName,
                                             QStringLiteral("palmLost"),
                                             this,
                                             SLOT(onPalmLost()));

    QDBusConnection::sessionBus().disconnect(AirGestureDBus::serviceName,
                                             AirGestureDBus::objectPath,
                                             AirGestureDBus::interfaceName,
                                             QStringLiteral("trialGesturePerformed"),
                                             this,
                                             SLOT(onTrialGesturePerformed(QString)));

    QDBusInterface iface(AirGestureDBus::serviceName,
                         AirGestureDBus::objectPath,
                         AirGestureDBus::interfaceName,
                         QDBusConnection::sessionBus());
    QDBusReply<void> reply = iface.call(QStringLiteral("stopTrial"));
    if (!reply.isValid()) {
        qWarning() << "Failed to stop trial mode:" << reply.error().message();
    }
}

void TryWidget::performVerticalGesture(const QString &direction)
{
    if (m_pVerticalScrollArea == nullptr) {
        return;
    }

    QScrollBar *bar = m_pVerticalScrollArea->verticalScrollBar();
    const int delta = 300;
    if (direction == QStringLiteral("down")) {
        bar->setValue(qMin(bar->maximum(), bar->value() + delta));
    } else if (direction == QStringLiteral("up")) {
        bar->setValue(qMax(bar->minimum(), bar->value() - delta));
    }
}

void TryWidget::performHorizontalGesture(const QString &direction)
{
    if (m_pHorizontalPage == nullptr) {
        return;
    }

    const int delta = (direction == QStringLiteral("left")) ? 1 : -1;
    m_pHorizontalPage->setCurrentWidget(m_pHorizontalPage->currentWidget() + delta);
}

void TryWidget::onPalmDetected(const QString &palmDirection)
{
    if (m_trialStage == TrialStage::TransitionPause) {
        return;
    }

    if (m_gestureType == AirGestureType::Wave_Vertical) {
        if (m_trialStage == TrialStage::WaitingFirstPalm && palmDirection == QStringLiteral("up")) {
            updatePromptDialog(false, QStringLiteral("down"));
            showPromptDialog();
            showIconDialog(QStringLiteral("up"));
            m_trialStage = TrialStage::WaitingFirstGesture;
        } else if (m_trialStage == TrialStage::WaitingSecondPalm && palmDirection == QStringLiteral("down")) {
            updatePromptDialog(false, QStringLiteral("up"));
            showPromptDialog();
            showIconDialog(QStringLiteral("down"));
            m_trialStage = TrialStage::WaitingSecondGesture;
        }
    } else if (m_gestureType == AirGestureType::Wave_Horizontal) {
        if (m_trialStage == TrialStage::WaitingFirstPalm && palmDirection == QStringLiteral("left")) {
            updatePromptDialog(false, QStringLiteral("left"));
            showPromptDialog();
            showIconDialog(QStringLiteral("left"));
            m_trialStage = TrialStage::WaitingFirstGesture;
        } else if (m_trialStage == TrialStage::WaitingSecondPalm && palmDirection == QStringLiteral("right")) {
            updatePromptDialog(false, QStringLiteral("right"));
            showPromptDialog();
            showIconDialog(QStringLiteral("right"));
            m_trialStage = TrialStage::WaitingSecondGesture;
        }
    } else if (m_gestureType == AirGestureType::Fist) {
        if (m_trialStage == TrialStage::WaitingFirstPalm && palmDirection == QStringLiteral("up")) {
            updatePromptDialog(false, QStringLiteral("ptrsc"));
            showPromptDialog();
            showIconDialog(QStringLiteral("up"));
            m_trialStage = TrialStage::WaitingFirstGesture;
        }
    }
}

void TryWidget::onPalmLost()
{
    if (m_trialStage == TrialStage::TransitionPause) {
        return;
    }

    QString revertPromptAction;
    TrialStage revertStage = m_trialStage;

    if (m_gestureType == AirGestureType::Wave_Vertical) {
        if (m_trialStage == TrialStage::WaitingFirstGesture) {
            revertPromptAction = QStringLiteral("down");
            revertStage = TrialStage::WaitingFirstPalm;
        } else if (m_trialStage == TrialStage::WaitingSecondGesture) {
            revertPromptAction = QStringLiteral("up");
            revertStage = TrialStage::WaitingSecondPalm;
        } else {
            return;
        }
    } else if (m_gestureType == AirGestureType::Wave_Horizontal) {
        if (m_trialStage == TrialStage::WaitingFirstGesture) {
            revertPromptAction = QStringLiteral("left");
            revertStage = TrialStage::WaitingFirstPalm;
        } else if (m_trialStage == TrialStage::WaitingSecondGesture) {
            revertPromptAction = QStringLiteral("right");
            revertStage = TrialStage::WaitingSecondPalm;
        } else {
            return;
        }
    } else if (m_gestureType == AirGestureType::Fist) {
        if (m_trialStage == TrialStage::WaitingFirstGesture) {
            revertPromptAction = QStringLiteral("ptrsc");
            revertStage = TrialStage::WaitingFirstPalm;
        } else {
            return;
        }
    } else {
        return;
    }

    if (m_pIconDialog != nullptr) {
        m_pIconDialog->hide();
    }
    updatePromptDialog(true, revertPromptAction);
    showPromptDialog();
    m_trialStage = revertStage;
}

void TryWidget::onTrialGesturePerformed(const QString &gesture)
{
    if (m_trialStage == TrialStage::TransitionPause) {
        return;
    }

    if (m_gestureType == AirGestureType::Wave_Vertical) {
        if (m_trialStage == TrialStage::WaitingFirstGesture && gesture == QStringLiteral("down")) {
            performVerticalGesture(gesture);
            beginGestureTransition(QStringLiteral("up"), TrialStage::WaitingSecondPalm);
        } else if (m_trialStage == TrialStage::WaitingSecondGesture && gesture == QStringLiteral("up")) {
            performVerticalGesture(gesture);
            beginGestureTransition(QStringLiteral("down"), TrialStage::WaitingFirstPalm);
        }
    } else if (m_gestureType == AirGestureType::Wave_Horizontal) {
        if (m_trialStage == TrialStage::WaitingFirstGesture && gesture == QStringLiteral("left")) {
            performHorizontalGesture(gesture);
            beginGestureTransition(QStringLiteral("right"), TrialStage::WaitingSecondPalm);
        } else if (m_trialStage == TrialStage::WaitingSecondGesture && gesture == QStringLiteral("right")) {
            performHorizontalGesture(gesture);
            beginGestureTransition(QStringLiteral("left"), TrialStage::WaitingFirstPalm);
        }
    } else if (m_gestureType == AirGestureType::Fist) {
        if (m_trialStage == TrialStage::WaitingFirstGesture && gesture == QStringLiteral("ptrsc")) {
            beginGestureTransition(QStringLiteral("ptrsc"), TrialStage::WaitingFirstPalm);
        }
    }
}

void TryWidget::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    m_trialStage = TrialStage::WaitingFirstPalm;
    startTrialMode();

    // 根据手势类型设置初始提示
    if (m_gestureType == AirGestureType::Wave_Vertical) {
        updatePromptDialog(true, "down");
    } else if (m_gestureType == AirGestureType::Wave_Horizontal) {
        updatePromptDialog(true, "left");
    } else if (m_gestureType == AirGestureType::Fist) {
        updatePromptDialog(true, "ptrsc");
    }

    // 延迟显示提示对话框，避免被其他窗口遮挡
    QTimer::singleShot(0, this, [this]() {
        showPromptDialog();
    });
}

void TryWidget::closeEvent(QCloseEvent *event)
{
    stopTrialMode();
    if (m_pIconDialog != nullptr) {
        m_pIconDialog->hide();
    }
    if (m_pPromptDialog != nullptr) {
        m_pPromptDialog->hide();
    }
    QDialog::closeEvent(event);
}
