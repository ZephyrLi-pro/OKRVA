/*
 * Copyright (C) 2026, KylinSoft Co., Ltd.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GESTUREACTIONEXECUTOR_H
#define GESTUREACTIONEXECUTOR_H

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <cerrno>
#include <cstring>
#include <linux/input-event-codes.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

class GestureActionExecutor
{
public:
    GestureActionExecutor();
    ~GestureActionExecutor();

    void executeWheel(const QString &direction);
    void executeHorizontal(const QString &direction);
    bool executePrintScreen();

private:
    bool shouldSkipAction(const QString &action);
    bool scrollVertical(int delta);
    bool pressKey(int keyCode, const char *keyName);
    bool initInputDevice();
    void cleanup();

    static constexpr qint64 ACTION_COOLDOWN_MS = 400;
    QElapsedTimer m_timer;
    QHash<QString, qint64> m_lastTriggerMs;
    libevdev *m_device = nullptr;
    libevdev_uinput *m_uinput = nullptr;
};

#endif // GESTUREACTIONEXECUTOR_H
