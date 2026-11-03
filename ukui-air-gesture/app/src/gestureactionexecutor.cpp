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

#include "gestureactionexecutor.h"

#include <QDebug>

GestureActionExecutor::GestureActionExecutor() {
    if (!initInputDevice()) {
        qWarning() << "Failed to initialize virtual input device at startup.";
    }
}

GestureActionExecutor::~GestureActionExecutor()
{
    cleanup();
}

bool GestureActionExecutor::shouldSkipAction(const QString &action)
{
    if (!m_timer.isValid()) {
        m_timer.start();
    }

    const qint64 nowMs = m_timer.elapsed();
    const qint64 lastMs = m_lastTriggerMs.value(action, -ACTION_COOLDOWN_MS);
    if (nowMs - lastMs < ACTION_COOLDOWN_MS) {
        return true;
    }

    m_lastTriggerMs.insert(action, nowMs);
    return false;
}

bool GestureActionExecutor::scrollVertical(int delta)
{
    if (delta == 0) {
        return true;
    }

    if (m_uinput == nullptr) {
        return false;
    }

    int rc = libevdev_uinput_write_event(m_uinput, EV_REL, REL_WHEEL, delta);
    if (rc < 0) {
        qWarning() << "Failed to send REL_WHEEL event:" << std::strerror(-rc);
        return false;
    }

    rc = libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
    if (rc < 0) {
        qWarning() << "Failed to send SYN_REPORT event:" << std::strerror(-rc);
        return false;
    }

    return true;
}

bool GestureActionExecutor::pressKey(int keyCode, const char *keyName)
{
    if (m_uinput == nullptr) {
        return false;
    }

    int rc = libevdev_uinput_write_event(m_uinput, EV_KEY, keyCode, 1);
    if (rc < 0) {
        qWarning() << "Failed to press key" << keyName << ":" << std::strerror(-rc);
        return false;
    }

    rc = libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
    if (rc < 0) {
        qWarning() << "Failed to sync key down for" << keyName << ":" << std::strerror(-rc);
        return false;
    }

    rc = libevdev_uinput_write_event(m_uinput, EV_KEY, keyCode, 0);
    if (rc < 0) {
        qWarning() << "Failed to release key" << keyName << ":" << std::strerror(-rc);
        return false;
    }

    rc = libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
    if (rc < 0) {
        qWarning() << "Failed to sync key up for" << keyName << ":" << std::strerror(-rc);
        return false;
    }

    return true;
}

bool GestureActionExecutor::initInputDevice()
{
    if (m_uinput != nullptr) {
        return true;
    }

    m_device = libevdev_new();
    if (m_device == nullptr) {
        qWarning() << "Failed to allocate libevdev device.";
        return false;
    }

    libevdev_set_name(m_device, "ukui-air-gesture-virtual-device");

    int rc = libevdev_enable_event_type(m_device, EV_KEY);
    if (rc < 0) {
        qWarning() << "Failed to enable EV_KEY:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_enable_event_code(m_device, EV_KEY, KEY_LEFT, nullptr);
    if (rc < 0) {
        qWarning() << "Failed to enable KEY_LEFT:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_enable_event_code(m_device, EV_KEY, KEY_RIGHT, nullptr);
    if (rc < 0) {
        qWarning() << "Failed to enable KEY_RIGHT:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_enable_event_code(m_device, EV_KEY, KEY_SYSRQ, nullptr);
    if (rc < 0) {
        qWarning() << "Failed to enable KEY_SYSRQ:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_enable_event_type(m_device, EV_REL);
    if (rc < 0) {
        qWarning() << "Failed to enable EV_REL:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_enable_event_code(m_device, EV_REL, REL_WHEEL, nullptr);
    if (rc < 0) {
        qWarning() << "Failed to enable REL_WHEEL:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    rc = libevdev_uinput_create_from_device(m_device, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc < 0) {
        qWarning() << "Failed to create uinput device:" << std::strerror(-rc);
        cleanup();
        return false;
    }

    return true;
}

void GestureActionExecutor::cleanup()
{
    if (m_uinput != nullptr) {
        libevdev_uinput_destroy(m_uinput);
        m_uinput = nullptr;
    }

    if (m_device != nullptr) {
        libevdev_free(m_device);
        m_device = nullptr;
    }
}

void GestureActionExecutor::executeWheel(const QString &direction)
{
    if (shouldSkipAction(QStringLiteral("wheel:") + direction)) {
        return;
    }

    const int wheelDelta = (direction == QStringLiteral("up")) ? 1 : -1;
    if (!scrollVertical(wheelDelta)) {
        qWarning() << "Failed to inject wheel event via libevdev.";
    }
}

void GestureActionExecutor::executeHorizontal(const QString &direction)
{
    if (shouldSkipAction(QStringLiteral("horizontal:") + direction)) {
        return;
    }

    // Mirror the movement so that the desktop follows what the user sees.
    const int keyCode = (direction == QStringLiteral("left")) ? KEY_RIGHT : KEY_LEFT;
    const char *keyName = (direction == QStringLiteral("left")) ? "KEY_RIGHT" : "KEY_LEFT";

    if (!pressKey(keyCode, keyName)) {
        qWarning() << "Failed to inject" << keyName << "event via libevdev.";
    }
}

bool GestureActionExecutor::executePrintScreen()
{
    if (shouldSkipAction(QStringLiteral("key:ptrsc"))) {
        return false;
    }

    if (!pressKey(KEY_SYSRQ, "KEY_SYSRQ")) {
        qWarning() << "Failed to inject KEY_SYSRQ event via libevdev.";
        return false;
    }

    return true;
}
