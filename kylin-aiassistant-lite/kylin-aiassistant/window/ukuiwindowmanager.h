/*
 * Qt5-UKUI's Library
 *
 * Copyright (C) 2023, KylinSoft Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Yue Lan <lanyue@kylinos.cn>
 *
 */

#ifndef UKUIWindowManager_H
#define UKUIWindowManager_H

#include <QObject>
#include <QTimer>
#include <QPoint>
#include <QWindow>

#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/pointer.h>

class QMouseEvent;

class AppEventFilter;

/*!
 * \brief The UKUIWindowManager class
 * \details
 * This class is use to help window management.
 * There are some qt's windows, such as frameless window, could not
 * move and move resize by it self. Through registering widget to window
 * manager, we can let them movable and can be maximum/half-maximum like
 * other normal window.
 */
class UKUIWindowManager : public QObject
{
    friend class AppEventFilter;
    Q_OBJECT
    // 1. 单例核心：Q_GADGET/Q_OBJECT 下声明私有构造，配合友元保证唯一实例
    explicit UKUIWindowManager(QObject *parent = nullptr);
    ~UKUIWindowManager() override;

    // 2. 禁止拷贝和赋值（C++11及以上），保证单例唯一性
    UKUIWindowManager(const UKUIWindowManager&) = delete;
    UKUIWindowManager& operator=(const UKUIWindowManager&) = delete;
public:
    // 3. 单例全局访问点：获取唯一实例（饿汉式，程序启动时初始化，线程安全）
    static UKUIWindowManager* instance();

    void registerWindow(QWindow *w);
    void unregisterWindow(QWindow *w);

    bool eventFilter(QObject *obj, QEvent *e);

    void buttonPresseEvent(QObject *obj, QMouseEvent *e);
    void mouseMoveEvent(QObject *obj, QMouseEvent *e);
    void mouseReleaseEvent(QObject *obj, QMouseEvent *e);

protected:
    void endDrag();

private:
    Qt::Edges resizeEdgesAt(QWindow *window, const QPoint &pos) const;
    void updateResizeCursor(QWindow *window, const QPoint &pos);
    bool startResize(QWindow *window, Qt::Edges edges, const QPoint &globalPos);

    QObject *m_current_obj = nullptr;
    bool m_is_dragging = false;
    bool m_resize_cursor_active = false;
    QTimer m_timer;
    QPoint m_start_point;

    bool m_prepared_to_drag = false;
    bool isDragable(QWindow *window);
    bool dragable = true;

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Pointer *m_pointer = nullptr;
    int m_serial = 0;
};

class AppEventFilter : public QObject
{
    friend class UKUIWindowManager;
    Q_OBJECT
private:
    explicit AppEventFilter(UKUIWindowManager *parent);
    ~AppEventFilter() {}

    bool eventFilter(QObject *obj, QEvent *e);

    UKUIWindowManager *m_wm = nullptr;
};
#endif // UKUIWindowManager_H
