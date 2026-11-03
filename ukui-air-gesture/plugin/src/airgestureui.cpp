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

#include "airgestureui.h"

#include <QDebug>
#include <algorithm>

AirGestureUI::AirGestureUI(QWidget *parent)
    : QWidget(parent)
    , m_gestureInfos{
        { AirGestureType::Wave_Vertical, ":/data/gif/上下滑动.gif", tr("Air Vertical Scroll"), tr("Keep your palm facing the screen, 20 to 60 cm away. After the gesture icon appears, move your hand up or down to scroll the content.") },
        { AirGestureType::Wave_Horizontal, ":/data/gif/左右滑动.gif", tr("Air Horizontal Scroll"), tr("Keep your palm facing the screen, 20 to 60 cm away. After the gesture icon appears, move your hand left or right to switch content.") },
        { AirGestureType::Fist, ":/data/gif/握拳截图.gif", tr("Air Fist Screenshot"), tr("Keep your palm facing the screen, 20 to 60 cm away. After the gesture icon appears, make a fist facing the screen to capture the full screen.") }
    }
{
    initGestureSettings();
    initUI();
}

void AirGestureUI::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setMargin(0);

    QWidget *titleWidget = new QWidget(this);
    titleWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setText(tr("Air Gesture"));
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(16, 0, 0, 0);
    titleLayout->addWidget(titleLabel);
    mainLayout->addWidget(titleWidget);

    mainLayout->addSpacing(8);

    QFrame *frame = createBoxFrame();
    mainLayout->addWidget(frame);
    mainLayout->addStretch();
}

QFrame *AirGestureUI::createBoxFrame()
{
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::Box);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    QWidget *introWidget = createIntroWidget();
    frameLayout->addWidget(introWidget);

    for (const AirGestureInfo &gestureInfo : m_gestureInfos) {
        frameLayout->addWidget(new kdk::KHLineFrame(this));
        QWidget *settingsWidget = createSettingsWidget(gestureInfo);
        frameLayout->addWidget(settingsWidget);
    }

    return frame;
}

QWidget *AirGestureUI::createIntroWidget()
{
    QWidget *introWidget = new QWidget(this);
    introWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout *introLayout = new QVBoxLayout(introWidget);
    introLayout->setContentsMargins(16, 16, 16, 16);
    introLayout->setSpacing(0);

    m_pMovieLabel = new QLabel(introWidget);
    m_pMovieLabel->setFixedSize(320, 180);
    QMovie *movie = new QMovie(m_gestureInfos[0].gifPath, QByteArray(), introWidget);
    m_pMovieLabel->setScaledContents(true);
    movie->setScaledSize(m_pMovieLabel->size());
    m_pMovieLabel->setMovie(movie);
    movie->start();
    introLayout->addWidget(m_pMovieLabel, 0, Qt::AlignHCenter);

    introLayout->addItem(new QSpacerItem(0, 16, QSizePolicy::Minimum, QSizePolicy::Fixed));

    m_pContentLabel = new QLabel(introWidget);
    m_pContentLabel->setText(m_gestureInfos[0].name);
    introLayout->addWidget(m_pContentLabel, 0, Qt::AlignHCenter);

    introLayout->addItem(new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed));
    
    m_pDescriptionLabel = new QLabel(introWidget);
    m_pDescriptionLabel->setDisabled(true);
    m_pDescriptionLabel->setText(m_gestureInfos[0].description);
    introLayout->addWidget(m_pDescriptionLabel, 0, Qt::AlignHCenter);

    return introWidget;
}

QWidget *AirGestureUI::createSettingsWidget(const AirGestureInfo &gestureInfo)
{
    QWidget *settingsWidget = new QWidget(this);
    settingsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsWidget->setFixedHeight(60);
    QHBoxLayout *settingsLayout = new QHBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(16, 0, 16, 0);
    settingsLayout->setSpacing(0);

    QLabel *contentLabel = new QLabel(settingsWidget);
    contentLabel->setText(gestureInfo.name);
    settingsLayout->addWidget(contentLabel);

    settingsLayout->addStretch();

    ClickableLabel *tryLabel = new ClickableLabel(settingsWidget);
    tryLabel->setText(tr("Try Now"));
    tryLabel->onClicked = [this, gestureInfo]() {
        TryWidget *w = new TryWidget(this, gestureInfo.type);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    };
    settingsLayout->addWidget(tryLabel);
    settingsLayout->addSpacing(24);
    
    settingsWidget->setProperty("gestureType", static_cast<int>(gestureInfo.type));
    settingsWidget->installEventFilter(this);

    switch (gestureInfo.type) {
    case AirGestureType::Wave_Vertical:
        m_pVerticalSwitchButton = new kdk::KSwitchButton(settingsWidget);
        settingsLayout->addWidget(m_pVerticalSwitchButton);
        setupGestureSwitch(m_pVerticalSwitchButton, AirGestureType::Wave_Vertical);
        break;
    case AirGestureType::Wave_Horizontal:
        m_pHorizontalSwitchButton = new kdk::KSwitchButton(settingsWidget);
        settingsLayout->addWidget(m_pHorizontalSwitchButton);
        setupGestureSwitch(m_pHorizontalSwitchButton, AirGestureType::Wave_Horizontal);
        break;
    case AirGestureType::Fist:
        m_pFistSwitchButton = new kdk::KSwitchButton(settingsWidget);
        settingsLayout->addWidget(m_pFistSwitchButton);
        setupGestureSwitch(m_pFistSwitchButton, AirGestureType::Fist);
        break;
    default:
        break;
    }

    return settingsWidget;
}

void AirGestureUI::updateIntroWidget(const AirGestureInfo &gestureInfo)
{
    m_pContentLabel->setText(gestureInfo.name);
    m_pDescriptionLabel->setText(gestureInfo.description);
    QMovie *old = m_pMovieLabel->movie();
    if (old) { 
        old->stop(); 
        old->deleteLater(); 
    }
    QMovie *m = new QMovie(gestureInfo.gifPath, QByteArray(), this);
    m->setScaledSize(m_pMovieLabel->size());
    m_pMovieLabel->setMovie(m);
    m->start();
}

void AirGestureUI::initGestureSettings()
{
    if (!QGSettings::isSchemaInstalled(AirGestureSettings::kGestureSchema)) {
        return;
    }

    m_pGestureSettings = new QGSettings(AirGestureSettings::kGestureSchema, "", this);

    connect(m_pGestureSettings, &QGSettings::changed, this, [this](const QString &key) {
        if (key == AirGestureSettings::kVerticalScrollEnabledKey && m_pVerticalSwitchButton != nullptr) {
            m_pVerticalSwitchButton->setChecked(m_pGestureSettings->get(key).toBool());
        } else if (key == AirGestureSettings::kHorizontalScrollEnabledKey && m_pHorizontalSwitchButton != nullptr) {
            m_pHorizontalSwitchButton->setChecked(m_pGestureSettings->get(key).toBool());
        } else if (key == AirGestureSettings::kFistScreenshotEnabledKey && m_pFistSwitchButton != nullptr) {
            m_pFistSwitchButton->setChecked(m_pGestureSettings->get(key).toBool());
        }
    });
}

void AirGestureUI::setupGestureSwitch(kdk::KSwitchButton *switchButton, AirGestureType gestureType)
{
    const char *settingsKey = AirGestureSettings::enabledKeyFor(gestureType);
    if (switchButton == nullptr || settingsKey == nullptr || m_pGestureSettings == nullptr) {
        return;
    }

    switchButton->setCheckable(true);
    switchButton->setChecked(m_pGestureSettings->get(settingsKey).toBool());

    connect(switchButton, &kdk::KSwitchButton::stateChanged, this, [this, settingsKey](bool checked) {
        m_pGestureSettings->set(settingsKey, checked);
    });
}

bool AirGestureUI::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::Enter) {
        return QWidget::eventFilter(watched, event);
    }

    const QVariant gestureTypeVariant = watched->property("gestureType");
    if (!gestureTypeVariant.isValid()) {
        return QWidget::eventFilter(watched, event);
    }

    const AirGestureType type = static_cast<AirGestureType>(gestureTypeVariant.toInt());
    auto it = std::find_if(m_gestureInfos.begin(), m_gestureInfos.end(), [type](const AirGestureInfo &g) {
        return g.type == type;
    });

    if (it != m_gestureInfos.end()) {
        updateIntroWidget(*it);
    }

    return QWidget::eventFilter(watched, event);
}