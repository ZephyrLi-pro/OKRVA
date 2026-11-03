/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
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
#include "clickfixlabel.h"
#include <QDebug>

ClickFixLabel::ClickFixLabel(QWidget *parent):
    QLabel(parent)
{
    this->setContentsMargins(8,0,0,0);
}

ClickFixLabel::~ClickFixLabel()
{

}

void ClickFixLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT doubleClicked();
    return;
}

void ClickFixLabel::paintEvent(QPaintEvent *event)
{
    QFontMetrics fontMetrics(this->font());
    int fontSize = fontMetrics.width(mStr);
    int m_width = this->width() - 8;
    if (fontSize > m_width) {
        this->setText(fontMetrics.elidedText(mStr, Qt::ElideRight, m_width), false);
        this->setToolTip(mStr);
    } else {
        this->setText(mStr, false);
        this->setToolTip("");
    }
    QLabel::paintEvent(event);
}

void ClickFixLabel::setText(const QString & text, bool saveTextFlag)
{
    if (saveTextFlag)
        mStr = text;
    QLabel::setText(text);
}
