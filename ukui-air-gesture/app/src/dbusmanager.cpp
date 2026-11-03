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

#include "dbusmanager.h"
#include "dbusadaptor.h"
#include "airgesturecommon.h"

#include <QDebug>

namespace {
const QString serviceName = QString::fromUtf8(AirGestureDBus::serviceName);
const QString objectPath = QString::fromUtf8(AirGestureDBus::objectPath);
}

DBusManager::DBusManager(QObject *parent)
    : QObject(parent)
{
}

DBusManager::RegisterResult DBusManager::registerAll()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "D-Bus session bus is not connected.";
        return RegisterResult::Failed;
    }

    QDBusConnectionInterface *interface = bus.interface();
    if (!interface) {
        qWarning() << "D-Bus connection interface is unavailable.";
        return RegisterResult::Failed;
    }

    QDBusReply<bool> serviceRegistered = interface->isServiceRegistered(serviceName);
    if (serviceRegistered.isValid() && serviceRegistered.value()) {
        qInfo() << "D-Bus service name is already owned by another instance.";
        return RegisterResult::AlreadyRunning;
    }

    new DBusAdaptor(this);

    if (!bus.registerObject(objectPath, this)) {
        qWarning() << "Failed to register D-Bus object:" << bus.lastError().message();
        qDeleteAll(findChildren<DBusAdaptor *>());
        return RegisterResult::Failed;
    }

    QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply = interface->registerService(serviceName);

    if (!reply.isValid()) {
        qWarning() << "Failed to register D-Bus service:" << reply.error().message();
        bus.unregisterObject(objectPath);
        qDeleteAll(findChildren<DBusAdaptor *>());
        return RegisterResult::Failed;
    }

    if (reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        qInfo() << "D-Bus service name is already owned by another instance.";
        bus.unregisterObject(objectPath);
        qDeleteAll(findChildren<DBusAdaptor *>());
        return RegisterResult::AlreadyRunning;
    }

    return RegisterResult::Registered;
}