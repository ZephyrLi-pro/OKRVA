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

#include "mediacontroller.h"
#include <algorithm>
#include <unistd.h>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusVariant>
#include <QDBusReply>
#include <QDebug>
#include <QStringList>

MediaController::~MediaController() {
    if (m_waylandNotifier != nullptr) {
        m_waylandNotifier->setEnabled(false);
        delete m_waylandNotifier;
        m_waylandNotifier = nullptr;
    }

    if (m_display != nullptr) {
        delete m_display;
        m_display = nullptr;
    }
}

bool MediaController::init() {
    if (m_selfPid == 0) {
        m_selfPid = static_cast<uint32_t>(getpid());
    }

    if (m_display == nullptr) {
        m_display = new wayland::display_t();
    }

    // 从注册表中监听全局对象，找到ukui_window_management协议并绑定
    m_registry = m_display->get_registry();
    m_registry.on_global() = [this](uint32_t name, std::string interface, uint32_t version) {
        if (interface != wayland::ukui_window_management_t::interface_name) {
            return;
        }

        wayland::ukui_window_management_t iface;
        const uint32_t bindVersion = std::min<uint32_t>(version, 1);
        wayland::proxy_t proxy = m_registry.bind(name, iface, bindVersion);
        m_windowManager = wayland::ukui_window_management_t(proxy);
    };

    // 阻塞，让wayland客户端和服务端同步，确保全局对象事件被处理，窗口管理协议被绑定
    m_display->roundtrip();
    if (!m_windowManager.proxy_has_object()) {
        qWarning() << "MediaController init failed: ukui_window_management global not found."
                   << "Compositor did not advertise this protocol.";
        return false;
    }

    m_windowManager.on_window_created() = [this](const std::string& uuid) {
        const QString qUuid = QString::fromStdString(uuid);
        bindWindow(qUuid);
    };

    // 再次roundtrip，确保绑定窗口管理协议的回调被调用
    m_display->roundtrip();

    // 监听Wayland事件并分发
    if (m_waylandNotifier == nullptr) {
        m_waylandNotifier = new QSocketNotifier(m_display->get_fd(), QSocketNotifier::Read);
        QObject::connect(m_waylandNotifier, &QSocketNotifier::activated, [this](int) {
            if (m_display != nullptr) {
                m_display->dispatch();
            }
        });
    }

    return true;
}

void MediaController::bindWindow(const QString& uuid) {
    if (!m_windows.contains(uuid)) {
        wayland::ukui_window_t window = m_windowManager.create_window(uuid.toStdString());
        m_windows.insert(uuid, window);

        TopWindowInfo info;
        info.uuid = uuid;
        m_windowInfos.insert(uuid, info);

        m_windows[uuid].on_app_id_changed() = [this, uuid](const std::string& app_id) {
            if (!m_windowInfos.contains(uuid)) {
                return;
            }
            m_windowInfos[uuid].app_id = QString::fromStdString(app_id);
            if (m_topWindowInfo.uuid == uuid) {
                m_topWindowInfo.app_id = m_windowInfos[uuid].app_id;
            }
        };

        m_windows[uuid].on_pid_changed() = [this, uuid](uint32_t pid) {
            if (!m_windowInfos.contains(uuid)) {
                return;
            }
            m_windowInfos[uuid].pid = pid;
            if (m_topWindowInfo.uuid == uuid) {
                m_topWindowInfo.pid = pid;
            }
        };

        m_windows[uuid].on_state_changed() = [this, uuid](uint32_t state) {
            if (!m_windowInfos.contains(uuid)) {
                return;
            }
            m_windowInfos[uuid].state = state;
            // active位变化可作为stacking_order_changed缺失时的顶层窗口回退依据
            if (state & 0x1) {
                if (shouldIgnoreWindow(m_windowInfos[uuid])) {
                    return;
                }
                m_topWindowInfo = m_windowInfos[uuid];
            }
        };

        m_windows[uuid].on_unmapped() = [this, uuid]() {
            m_windows.remove(uuid);
            m_windowInfos.remove(uuid);
            if (m_topWindowInfo.uuid == uuid) {
                m_topWindowInfo = TopWindowInfo();
            }
        };
    }
}

bool MediaController::adjustVolumeForFullscreenMpris(const QString& direction) {
    const QString service = findMatchedFullscreenMprisService();
    if (service.isEmpty()) {
        return false;
    }
    return setMprisVolume(service, direction);
}

QString MediaController::findTopWindowMprisService() const
{
    if (shouldIgnoreWindow(m_topWindowInfo)) {
        return QString();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *connectionInterface = bus.interface();
    if (!connectionInterface) {
        return QString();
    }

    const QStringList services = connectionInterface->registeredServiceNames();
    for (const QString &service : services) {
        if (!service.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
            continue;
        }
        if (matchesMprisService(service, bus)) {
            return service;
        }
    }

    return QString();
}

QString MediaController::findMatchedFullscreenMprisService() const
{
    const QString service = findTopWindowMprisService();
    if (service.isEmpty() || !isMprisServiceFullscreen(service)) {
        return QString();
    }

    return service;
}

bool MediaController::isMprisServiceFullscreen(const QString &service) const
{
    constexpr uint32_t kStateFullscreen = 0x8;

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface mpris(service, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", bus);
    QDBusReply<QVariant> fsReply = mpris.call("Get", "org.mpris.MediaPlayer2", "Fullscreen");
    if (fsReply.isValid() && fsReply.value().toBool()) {
        return true;
    }

    // 某些播放器 Fullscreen 属性更新不及时，回退使用窗口状态位
    return (m_topWindowInfo.state & kStateFullscreen) != 0;
}

bool MediaController::setMprisVolume(const QString& service, const QString& direction) {
    if (service.isEmpty()) {
        return false;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface mpris(service, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", bus);
    if (!mpris.isValid()) {
        qWarning() << "MPRIS properties interface invalid for service:" << service;
        return false;
    }

    QDBusReply<QVariant> volumeReply = mpris.call("Get", "org.mpris.MediaPlayer2.Player", "Volume");
    if (!volumeReply.isValid()) {
        qWarning() << "Failed to get MPRIS volume. service=" << service
                   << "error=" << mpris.lastError().name() << mpris.lastError().message();
        return false;
    }

    double currentVolume = volumeReply.value().toDouble();
    double delta = 0.0;
    if (direction == "up") {
        delta = 0.05;
    } else if (direction == "down") {
        delta = -0.05;
    } else {
        return false;
    }
    double nextVolume = currentVolume + delta;
    if (nextVolume < 0.0) {
        nextVolume = 0.0;
    }
    if (nextVolume > 1.0) {
        nextVolume = 1.0;
    }

    QDBusReply<void> setReply = mpris.call(
        "Set",
        "org.mpris.MediaPlayer2.Player",
        "Volume",
        QVariant::fromValue(QDBusVariant(QVariant::fromValue(nextVolume)))
    );

    if (!setReply.isValid()) {
        qWarning() << "Failed to set MPRIS volume. service=" << service
                   << "current=" << currentVolume << "next=" << nextVolume
                   << "error=" << mpris.lastError().name() << mpris.lastError().message();
        return false;
    }

    return true;
}

// 判断MPRIS服务是否与当前顶层窗口匹配
bool MediaController::matchesMprisService(const QString& service, const QDBusConnection& bus) const {
    const QString appId = m_topWindowInfo.app_id.trimmed().toLower();
    const QString serviceSuffix = service.mid(QStringLiteral("org.mpris.MediaPlayer2.").size()).toLower();

    // 通过DesktopEntry属性匹配app_id
    QDBusInterface mpris(service, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", bus);
    QDBusReply<QVariant> desktopEntryReply = mpris.call("Get", "org.mpris.MediaPlayer2", "DesktopEntry");
    if (desktopEntryReply.isValid()) {
        const QString desktopEntry = desktopEntryReply.value().toString().trimmed().toLower();
        if (!desktopEntry.isEmpty() && !appId.isEmpty() && desktopEntry == appId) {
            return true;
        }
    }

    // 通过服务名后缀匹配app_id
    if (!appId.isEmpty() && !serviceSuffix.isEmpty()) {
        if (appId == serviceSuffix) {
            return true;
        }
    }

    QDBusConnectionInterface *connectionInterface = bus.interface();
    if (!connectionInterface) {
        return false;
    }

    // 最后通过pid匹配
    QDBusReply<uint> pidReply = connectionInterface->call("GetConnectionUnixProcessID", service);
    return pidReply.isValid() && pidReply.value() == m_topWindowInfo.pid;
}

// 检查当前顶层窗口是否对应任一 MPRIS 服务
bool MediaController::isTopWindowMpris() const {
    return !findTopWindowMprisService().isEmpty();
}

// 根据pid判断需要过滤掉的窗口
bool MediaController::shouldIgnoreWindow(const TopWindowInfo& info) const {
    return info.pid != 0 && info.pid == m_selfPid;
}
