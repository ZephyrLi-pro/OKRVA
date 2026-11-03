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

#ifndef AIRGESTURECOMMON_H
#define AIRGESTURECOMMON_H

#include <QString>

enum class AirGestureType {
    Wave_Vertical,
    Wave_Horizontal,
    Fist,
    Unknown
};

inline QString gestureTypeToString(AirGestureType type)
{
    switch (type) {
    case AirGestureType::Wave_Vertical:
        return QStringLiteral("wave_vertical");
    case AirGestureType::Wave_Horizontal:
        return QStringLiteral("wave_horizontal");
    case AirGestureType::Fist:
        return QStringLiteral("fist");
    default:
        return QStringLiteral("unknown");
    }
}

inline AirGestureType gestureTypeFromString(const QString &type)
{
    if (type == QStringLiteral("wave_vertical")) {
        return AirGestureType::Wave_Vertical;
    }
    if (type == QStringLiteral("wave_horizontal")) {
        return AirGestureType::Wave_Horizontal;
    }
    if (type == QStringLiteral("fist")) {
        return AirGestureType::Fist;
    }
    return AirGestureType::Unknown;
}

namespace AirGestureSettings {
static const char kVerticalScrollEnabledKey[] = "vertical-scroll-enabled";
static const char kHorizontalScrollEnabledKey[] = "horizontal-scroll-enabled";
static const char kFistScreenshotEnabledKey[] = "fist-screenshot-enabled";

inline const char *enabledKeyFor(AirGestureType type)
{
    switch (type) {
    case AirGestureType::Wave_Vertical:
        return kVerticalScrollEnabledKey;
    case AirGestureType::Wave_Horizontal:
        return kHorizontalScrollEnabledKey;
    case AirGestureType::Fist:
        return kFistScreenshotEnabledKey;
    default:
        return nullptr;
    }
}
}

namespace AirGestureDBus {
static const char serviceName[] = "org.ukui.AirGesture";
static const char objectPath[] = "/org/ukui/AirGesture";
static const char interfaceName[] = "org.ukui.AirGesture";
}

#endif // AIRGESTURECOMMON_H
