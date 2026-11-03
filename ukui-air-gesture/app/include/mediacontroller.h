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

#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include <QString>
#include <QDBusConnection>
#include <QHash>
#include <QSocketNotifier>
#include <wayland-client-protocol.hpp>
#include "ukui-window-management.h"

struct TopWindowInfo {
    QString uuid;
    QString app_id;
    uint32_t pid = 0;
    uint32_t state = 0;
};

class MediaController {
public:
    MediaController() = default;
    ~MediaController();
    bool init();
    bool adjustVolumeForFullscreenMpris(const QString& direction);
    bool isTopWindowMpris() const;
private:
    QString findTopWindowMprisService() const;
    QString findMatchedFullscreenMprisService() const;
    bool isMprisServiceFullscreen(const QString& service) const;
    bool setMprisVolume(const QString& service, const QString& direction);
    bool shouldIgnoreWindow(const TopWindowInfo& info) const;
    void bindWindow(const QString& uuid);
    bool matchesMprisService(const QString& service, const QDBusConnection& bus) const;

    TopWindowInfo m_topWindowInfo;                      // 当前顶层窗口信息
    wayland::display_t* m_display = nullptr;
    wayland::registry_t m_registry;
    QSocketNotifier* m_waylandNotifier = nullptr;       // 用于监听Wayland事件的notifier
    uint32_t m_selfPid = 0;
    QHash<QString, wayland::ukui_window_t> m_windows;   // 按uuid缓存窗口对象
    QHash<QString, TopWindowInfo> m_windowInfos;        // 按uuid缓存窗口属性
    wayland::ukui_window_management_t m_windowManager;  // 窗管对象
};

#endif // MEDIACONTROLLER_H