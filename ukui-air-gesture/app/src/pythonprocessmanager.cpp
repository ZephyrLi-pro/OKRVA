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

#include "pythonprocessmanager.h"

#include <QDebug>
#include <QFileInfo>

PythonProcessManager::PythonProcessManager(QObject *parent)
    : QObject(parent)
{
}

PythonProcessManager::~PythonProcessManager()
{
    stop();
}

bool PythonProcessManager::start()
{
    if (isRunning()) {
        return true;
    }

    const QString venvPath = getVenvPath();
    const QString scriptPath = getScriptPath();

    if (venvPath.isEmpty()) {
        qWarning() << "No Python interpreter found.";
        return false;
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "Python entry script main.py was not found.";
        return false;
    }

    setupProcessLogging();

    m_process.setProgram(venvPath);
    m_process.setArguments(QStringList() << "-u" << scriptPath);
    m_process.start();

    if (!m_process.waitForStarted(5000)) {
        qWarning() << "Failed to start Python process:" << m_process.errorString();
        return false;
    }

    qInfo() << "Python subprocess started. pid=" << m_process.processId();

    return true;
}

void PythonProcessManager::setupProcessLogging()
{
    if (m_loggingConnected) {
        return;
    }

    QObject::connect(&m_process,
                     &QProcess::readyReadStandardOutput,
                     this,
                     &PythonProcessManager::onReadyReadStandardOutput);

    QObject::connect(&m_process,
                     &QProcess::readyReadStandardError,
                     this,
                     &PythonProcessManager::onReadyReadStandardError);

    m_loggingConnected = true;
}

void PythonProcessManager::onReadyReadStandardOutput()
{
    const QByteArray output = m_process.readAllStandardOutput();
    if (!output.isEmpty()) {
        qInfo().noquote() << "[python][stdout]" << QString::fromLocal8Bit(output).trimmed();
    }
}

void PythonProcessManager::onReadyReadStandardError()
{
    const QByteArray output = m_process.readAllStandardError();
    if (!output.isEmpty()) {
        qWarning().noquote() << "[python][stderr]" << QString::fromLocal8Bit(output).trimmed();
    }
}

void PythonProcessManager::stop(int timeoutMs)
{
    if (!isRunning()) {
        return;
    }

    m_process.terminate();
    if (!m_process.waitForFinished(timeoutMs)) {
        m_process.kill();
        m_process.waitForFinished();
    }
}

bool PythonProcessManager::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString PythonProcessManager::getVenvPath() const
{
#ifdef AIR_GESTURE_VENV_PYTHON
    const QString installedVenvPython = QStringLiteral(AIR_GESTURE_VENV_PYTHON);
    if (QFileInfo::exists(installedVenvPython)) {
        return installedVenvPython;
    }

    qWarning() << "Configured venv python not found:" << installedVenvPython;
    return QString();
#else
    qWarning() << "AIR_GESTURE_VENV_PYTHON is not defined.";
    return QString();
#endif
}

QString PythonProcessManager::getScriptPath() const
{
#ifdef AIR_GESTURE_PYTHON_MAIN
    const QString installedScript = QStringLiteral(AIR_GESTURE_PYTHON_MAIN);
    if (QFileInfo::exists(installedScript)) {
        return installedScript;
    }

    qWarning() << "Configured Python entry script not found:" << installedScript;
    return QString();
#else
    qWarning() << "AIR_GESTURE_PYTHON_MAIN is not defined.";
    return QString();
#endif
}
