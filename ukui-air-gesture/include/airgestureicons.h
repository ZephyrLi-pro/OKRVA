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

#ifndef AIRGESTUREICONS_H
#define AIRGESTUREICONS_H

#include <QIcon>
#include <QPixmap>
#include <QString>

inline QString airGestureIconResourcePath(const QString &iconName)
{
    return QStringLiteral(":/data/%1.svg").arg(iconName);
}

inline QIcon airGestureIcon(const QString &iconName)
{
    if (!iconName.isEmpty() && QIcon::hasThemeIcon(iconName)) {
        return QIcon::fromTheme(iconName);
    }

    return QIcon(airGestureIconResourcePath(iconName));
}

inline QPixmap airGestureIconPixmap(const QString &iconName, int size)
{
    return airGestureIcon(iconName).pixmap(size, size);
}

#endif // AIRGESTUREICONS_H
