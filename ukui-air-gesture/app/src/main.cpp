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

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include "singleapplication.h"

namespace {
void fileMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    const QString logDirectory = QDir::homePath()
        + QStringLiteral("/.local/state/ukui-air-gesture");
    QDir().mkpath(logDirectory);

    QFile file(logDirectory + QStringLiteral("/service.log"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    const char *level = "INFO";
    if (type == QtWarningMsg) {
        level = "WARN";
    } else if (type == QtCriticalMsg || type == QtFatalMsg) {
        level = "ERROR";
    } else if (type == QtDebugMsg) {
        level = "DEBUG";
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << ' ' << level << ' ' << message << '\n';
}
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(fileMessageHandler);
    SingleApplication a(argc, argv);

    const SingleApplication::InitResult result = a.getInitResult();
    switch (result) {
    case SingleApplication::InitResult::Success:
        break;
    case SingleApplication::InitResult::AlreadyRunning:
        return 0;
    case SingleApplication::InitResult::DBusFailed:
        qWarning() << "Application exiting: D-Bus registration failed.";
        return 1;
    case SingleApplication::InitResult::PythonFailed:
        qWarning() << "Application exiting: Python subprocess failed to start.";
        return 1;
    }

    return a.exec();
}
