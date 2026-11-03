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

#include "singleapplication.h"

#include <QDebug>

SingleApplication::SingleApplication(int &argc, char **argv)
    : QApplication(argc, argv)
{
    const DBusManager::RegisterResult dbusResult = m_dbusManager.registerAll();

    switch (dbusResult) {
    case DBusManager::RegisterResult::Registered:
        m_initResult = InitResult::Success;
        break;
    case DBusManager::RegisterResult::AlreadyRunning:
        m_initResult = InitResult::AlreadyRunning;
        return;
    case DBusManager::RegisterResult::Failed:
        m_initResult = InitResult::DBusFailed;
        return;
    }

    if (!m_pythonProcessManager.start()) {
        m_initResult = InitResult::PythonFailed;
    }
}

SingleApplication::InitResult SingleApplication::getInitResult() const
{
    return m_initResult;
}
