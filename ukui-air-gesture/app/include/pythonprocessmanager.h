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

#ifndef PYTHONPROCESSMANAGER_H
#define PYTHONPROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class PythonProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit PythonProcessManager(QObject *parent = nullptr);
    ~PythonProcessManager();

    bool start();
    void stop(int timeoutMs = 3000);

    bool isRunning() const;

private:
    void setupProcessLogging();

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    QString getVenvPath() const;
    QString getScriptPath() const;

    QProcess m_process;
    bool m_loggingConnected = false;
};

#endif // PYTHONPROCESSMANAGER_H
