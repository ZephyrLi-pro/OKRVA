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

#ifndef AIRGESTUREUI_H
#define AIRGESTUREUI_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QMovie>
#include <QList>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGSettings>

#include "kysdk/applications/kswitchbutton.h"
#include "kysdk/applications/klineframe.h"

#include "clickablelabel.h"
#include "airgesturecommon.h"
#include "trywidget.h"

class AirGestureUI : public QWidget
{
    Q_OBJECT
public:
    explicit AirGestureUI(QWidget *parent = nullptr);

private:
    void initUI();
    QFrame *createBoxFrame();
    QWidget *createIntroWidget();
    QWidget *createSettingsWidget(const AirGestureInfo &gestureInfo);
    void updateIntroWidget(const AirGestureInfo &gestureInfo);
    void initGestureSettings();
    void setupGestureSwitch(kdk::KSwitchButton *switchButton, AirGestureType gestureType);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLabel *m_pMovieLabel = nullptr;
    QLabel *m_pContentLabel = nullptr;
    QLabel *m_pDescriptionLabel = nullptr;

    kdk::KSwitchButton *m_pVerticalSwitchButton = nullptr;
    kdk::KSwitchButton *m_pHorizontalSwitchButton = nullptr;
    kdk::KSwitchButton *m_pFistSwitchButton = nullptr;
    QList<AirGestureInfo> m_gestureInfos;
    QGSettings *m_pGestureSettings = nullptr;
};

#endif // AIRGESTUREUI_H
