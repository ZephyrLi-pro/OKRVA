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
#include "ctextedit.h"
#include <QTextEdit>
#include <QFocusEvent>
#include <QPalette>

CTextEdit::CTextEdit(QWidget *parent) : QTextEdit(parent)
{
    setStyleSheet("QTextEdit { border: 1px solid rgba(38,38,38,0.15); border-radius: 6px; padding:1px 1px}");
}

void CTextEdit::focusInEvent(QFocusEvent *event)
{
    setStyleSheet("QTextEdit { border: 2px solid palette(highlight); border-radius: 6px; padding:1px 1px}");
    QTextEdit::focusInEvent(event);
}

void CTextEdit::focusOutEvent(QFocusEvent *event)
{
    setStyleSheet("QTextEdit { border: 1px solid rgba(38,38,38,0.15); border-radius: 6px; padding:1px 1px}");
    QTextEdit::focusOutEvent(event);
}
