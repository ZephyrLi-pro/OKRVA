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

#include "clickablelabel.h"
#include <QGuiApplication>

ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent)
{
    QPalette pal = palette();
    QColor accent = pal.highlight().color();
    pal.setColor(QPalette::Text, accent);
    setPalette(pal);
    setCursor(Qt::PointingHandCursor);
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && onClicked) {
        onClicked();
    }
    QLabel::mouseReleaseEvent(event);
}
