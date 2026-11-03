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

#ifndef DBUSADAPTOR_H
#define DBUSADAPTOR_H

#include "airgesturecommon.h"
#include "gestureactionexecutor.h"
#include "mainwindow.h"
#include <QDBusAbstractAdaptor>
#include <QObject>

class DBusAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ukui.AirGesture")
public:
    explicit DBusAdaptor(QObject *parent = nullptr);

private:
    MainWindow m_mainWindow;
    GestureActionExecutor m_actionExecutor;
    bool m_trialActive = false;
    AirGestureType m_trialGestureType = AirGestureType::Unknown;

    bool isGestureEnabled(AirGestureType type) const;
    bool isGestureAllowed(AirGestureType type) const;
    bool isShowIconAllowed(const QString &palmDirection) const;

public slots:
    void showIcon(const QString &palmDirection);
    void hideIcon();
    void wheel(const QString &direction);
    void horizontal(const QString &direction);
    void ptrsc();
    void startTrial(const QString &gestureType);
    void stopTrial();

signals:
    void palmDetected(const QString &palmDirection);
    void palmLost();
    void trialGesturePerformed(const QString &gesture);
};

#endif // DBUSADAPTOR_H
