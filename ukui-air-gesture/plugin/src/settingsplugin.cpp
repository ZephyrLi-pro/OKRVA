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

#include "settingsplugin.h"

#include <QFile>
#include <QTranslator>

#include <cerrno>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

QWidget *SettingsPlugin::pluginUi()
{
    installTranslation();

    return new AirGestureUI();
}

const QString SettingsPlugin::name() const
{
    return QStringLiteral("ukui-air-gesture");
}

bool SettingsPlugin::hasVideoCaptureCapability(int fd) const
{
    v4l2_capability cap {};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
        return false;
    }

    const __u32 caps = cap.device_caps ? cap.device_caps : cap.capabilities;
    return (caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
}

bool SettingsPlugin::isCameraAvailable() const
{
    for (int index = 0; index < 8; ++index) {
        const QString path = QStringLiteral("/dev/video%1").arg(index);
        if (!QFile::exists(path)) {
            continue;
        }

        const QByteArray devicePath = path.toLocal8Bit();
        const int fd = ::open(devicePath.constData(), O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            // 设备被占用说明硬件存在，仍视为可用
            if (errno == EBUSY) {
                return true;
            }
            continue;
        }

        const bool available = hasVideoCaptureCapability(fd);
        ::close(fd);
        if (available) {
            return true;
        }
    }

    return false;
}

bool SettingsPlugin::isEnable() const
{
    return isCameraAvailable();
}

QString SettingsPlugin::translationPath() const
{
    return QStringLiteral("/usr/share/accessibility-plugin/i18n/ukui-air-gesture/%1.ts");
}

void SettingsPlugin::installTranslation()
{
    QTranslator *translator = new QTranslator(this);
    if (translator->load(QLocale::system().name(),
                        QStringLiteral("/usr/share/accessibility-plugin/i18n/ukui-air-gesture"),
                        QString(),
                        QStringLiteral(".qm"))) {
        QCoreApplication::installTranslator(translator);
    }
}