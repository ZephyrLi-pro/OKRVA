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

#ifndef DBUSMANAGER_H
#define DBUSMANAGER_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>

class DBusManager : public QObject
{
    Q_OBJECT
public:
    enum class RegisterResult {
        Registered,
        AlreadyRunning,
        Failed
    };

    explicit DBusManager(QObject *parent = nullptr);

    RegisterResult registerAll();
};

#endif // DBUSMANAGER_H
