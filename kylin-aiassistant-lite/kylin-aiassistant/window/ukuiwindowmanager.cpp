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

#include "ukuiwindowmanager.h"

#include <QWidget>
#include <QMouseEvent>

#include <KWindowSystem>
#include <NETWM>
#include <QX11Info>
#include <xcb/xcb.h>

#include <QApplication>
#include <QCursor>
#include <QWindow>

#include <QDebug>

namespace {
constexpr int kResizeMargin = 6;

#if (QT_VERSION < QT_VERSION_CHECK(5,15,0))
NET::Direction netResizeDirection(Qt::Edges edges)
{
    if (edges == (Qt::TopEdge | Qt::LeftEdge))
        return NET::TopLeft;
    if (edges == Qt::TopEdge)
        return NET::Top;
    if (edges == (Qt::TopEdge | Qt::RightEdge))
        return NET::TopRight;
    if (edges == Qt::RightEdge)
        return NET::Right;
    if (edges == (Qt::BottomEdge | Qt::RightEdge))
        return NET::BottomRight;
    if (edges == Qt::BottomEdge)
        return NET::Bottom;
    if (edges == (Qt::BottomEdge | Qt::LeftEdge))
        return NET::BottomLeft;
    return NET::Left;
}
#endif
}

// 1. 初始化静态实例：饿汉式，程序启动时创建，线程安全
static UKUIWindowManager* global_instance = nullptr;

// 2. 实现单例访问接口
UKUIWindowManager* UKUIWindowManager::instance()
{
    if(!global_instance)
        global_instance = new UKUIWindowManager();
    return global_instance;
}

UKUIWindowManager::UKUIWindowManager(QObject *parent) : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(100);
    m_timer.setSingleShot(true);
    m_start_point = QPoint(0, 0);

    qApp->installEventFilter(new AppEventFilter(this));

    if (QX11Info::isPlatformX11())
        return;

#if (QT_VERSION < QT_VERSION_CHECK(5,15,0))
    using namespace KWayland::Client;
    m_connection = ConnectionThread::fromApplication(qApp);
    if (!m_connection)
        return;
    m_registry = new Registry(this);
    m_registry->create(m_connection);

    connect(m_registry, &KWayland::Client::Registry::seatAnnounced, this, [=](quint32 name, quint32 version) {
        m_seat = m_registry->createSeat(name, version, this);
        connect(m_seat, &Seat::hasPointerChanged, this, [=](bool pointerChanged) {
            if (pointerChanged) {
                if (!m_pointer) {
                    m_pointer = m_seat->createPointer(this);
                    connect(m_pointer,
                            &KWayland::Client::Pointer::buttonStateChanged,
                            this,
                            [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                                m_serial = serial;
                            });
                }
            } else {
                delete m_pointer;
                m_pointer = nullptr;
            }
        });
    });

    m_registry->setup();
    m_connection->roundtrip();
#endif
}

UKUIWindowManager::~UKUIWindowManager()
{
    if(m_registry){
        m_registry->deleteLater();
        m_registry = nullptr;
    }
}

void UKUIWindowManager::registerWindow(QWindow *w)
{
    if (dragable = isDragable(w)) {
        w->removeEventFilter(this);
        w->installEventFilter(this);
    }
}

void UKUIWindowManager::unregisterWindow(QWindow *w)
{
    if (dragable)
        w->removeEventFilter(this);
}

bool UKUIWindowManager::eventFilter(QObject *obj, QEvent *e)
{

    QWindow *w = qobject_cast<QWindow*>(obj);
    if (!w) {
        return false;
    }
    auto var = w->property("useStyleWindowManager");
    if (!var.isValid() || (!var.isNull() && !var.toBool())) {
        if (m_resize_cursor_active) {
            w->unsetCursor();
            m_resize_cursor_active = false;
        }
        return false;
    }

    switch (e->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);

        if (event->button() == Qt::LeftButton) {
            const Qt::Edges edges = resizeEdgesAt(w, event->pos());
            if (edges != Qt::Edges() && startResize(w, edges, event->globalPos())) {
                endDrag();
                return true;
            }

            // If the cursor is not normal arrow cursor,
            // we should consider there is another excepted operation
            // which current widget hope do. So we won't trigger
            // the window move event.
            QWindow *w = qobject_cast<QWindow *>(obj);
            if (w->cursor().shape() != Qt::CursorShape::ArrowCursor)
                return false;

            buttonPresseEvent(obj, event);
            return false;
        }
        return false;
    }
    case QEvent::MouseMove: {
        //if (QWidget::mouseGrabber()) return false;

        QMouseEvent *event = static_cast<QMouseEvent*>(e);

        if (event->buttons() == Qt::NoButton) {
            updateResizeCursor(w, event->pos());
            return false;
        }

        /*! \note In x11, accuracy of the MouseEvent translated from TouchEvent is not
         *  consistent with the accuracy of the native MouseEvent(TouchEvent is a decimal
         *  and MouseEvent is an integer).
         *  When the touch is pressed, if the mouse clicks the right button, Qt will think that
         *  the positions of the two clicks are inconsistent, and will send a MouseMoveEvent,
         *  the source of this MouseMoveEvent is not MouseEventSynthesizedByQt, so this MouseMoveEvent
         *  will not tirgger w->grabMouse(), window will be grabbed by kwin but no TouchEndEvent is received
         *  Therefore, the MouseMoveEvent is filtered out here if the moving distance is too small.
         *  \return return true because some borderless windows handle window dragging separately,
         *  if return false will cause these windows to behave abnormally
         *  \author wangweinan@kylinos.cn
        */
        if (qAbs(event->pos().x() - m_start_point.x()) < 2 &&
            qAbs(event->pos().y() - m_start_point.y()) < 2) {
            return true;
        }

        //move request
        mouseMoveEvent(obj, event);
        return false;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);
        mouseReleaseEvent(obj, event);
        return false;
    }
    case QEvent::Move: {
        if (m_current_obj && m_is_dragging) {
            //qDebug()<<"drag end";
            endDrag();
        } else {
            //qDebug()<<"move";
        }
        return false;
    }
    default:
        return false;
    }
}

Qt::Edges UKUIWindowManager::resizeEdgesAt(QWindow *window, const QPoint &pos) const
{
    if (!window || window->visibility() == QWindow::Maximized
            || window->visibility() == QWindow::FullScreen
            || (window->minimumWidth() == window->maximumWidth()
                && window->minimumHeight() == window->maximumHeight())) {
        return Qt::Edges();
    }

    Qt::Edges edges;
    if (pos.x() >= 0 && pos.x() < kResizeMargin)
        edges |= Qt::LeftEdge;
    else if (pos.x() < window->width() && pos.x() >= window->width() - kResizeMargin)
        edges |= Qt::RightEdge;

    if (pos.y() >= 0 && pos.y() < kResizeMargin)
        edges |= Qt::TopEdge;
    else if (pos.y() < window->height() && pos.y() >= window->height() - kResizeMargin)
        edges |= Qt::BottomEdge;

    return edges;
}

void UKUIWindowManager::updateResizeCursor(QWindow *window, const QPoint &pos)
{
    const Qt::Edges edges = resizeEdgesAt(window, pos);
    Qt::CursorShape cursorShape = Qt::ArrowCursor;

    if (edges == (Qt::TopEdge | Qt::LeftEdge)
            || edges == (Qt::BottomEdge | Qt::RightEdge)) {
        cursorShape = Qt::SizeFDiagCursor;
    } else if (edges == (Qt::TopEdge | Qt::RightEdge)
               || edges == (Qt::BottomEdge | Qt::LeftEdge)) {
        cursorShape = Qt::SizeBDiagCursor;
    } else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge)) {
        cursorShape = Qt::SizeHorCursor;
    } else if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge)) {
        cursorShape = Qt::SizeVerCursor;
    }

    if (edges != Qt::Edges()) {
        window->setCursor(QCursor(cursorShape));
        m_resize_cursor_active = true;
    } else if (m_resize_cursor_active) {
        window->unsetCursor();
        m_resize_cursor_active = false;
    }
}

bool UKUIWindowManager::startResize(QWindow *window, Qt::Edges edges, const QPoint &globalPos)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
    Q_UNUSED(globalPos)
    return window->startSystemResize(edges);
#else
    if (QX11Info::isPlatformX11()) {
        const qreal dpiRatio = qApp->devicePixelRatio();
        auto connection = QX11Info::connection();
        xcb_ungrab_pointer(connection, XCB_TIME_CURRENT_TIME);
        NETRootInfo(connection, NET::WMMoveResize).moveResizeRequest(
                    window->winId(), globalPos.x() * dpiRatio, globalPos.y() * dpiRatio,
                    netResizeDirection(edges));
        xcb_flush(connection);
        return true;
    }

    auto shellSurface = KWayland::Client::ShellSurface::fromWindow(window);
    if (!shellSurface || !m_seat)
        return false;
    shellSurface->requestResize(m_seat, m_serial, edges);
    return true;
#endif
}

void UKUIWindowManager::buttonPresseEvent(QObject *obj, QMouseEvent *e)
{
    //qDebug()<<"mouse press event";
    endDrag();
    QWindow *w = qobject_cast<QWindow*>(obj);
    //NOTE: We have to skip the border for resize event.
    auto pos = w->mapFromGlobal(e->globalPos());

    //title active height set 48px
    QRect activeRect(0, 0, w->width(), 48);
    if (!activeRect.contains(pos)) {
        //qDebug()<<"skip move event";
        return;
    }
//    if (!w->rect().adjusted(10, 10, -10, -10).contains(pos)) {
//        //qDebug()<<"skip move event";
//        return;
//    }


    m_prepared_to_drag = true;
    m_is_dragging = false;
    m_current_obj = obj;
    m_start_point = e->pos();
    m_timer.start();
}

void UKUIWindowManager::mouseMoveEvent(QObject *obj, QMouseEvent *e)
{
    //qDebug()<<"mouse move event";

    if (!m_prepared_to_drag)
        return;

    if (m_is_dragging)
        return;

    QWindow *w = qobject_cast<QWindow*>(obj);
    const QPoint native = e->globalPos();
    qreal  dpiRatio = qApp->devicePixelRatio();
    if (QX11Info::isPlatformX11()) {
        if (m_is_dragging)
            return;

//        qDebug()<<"x11 move start";
        auto connection = QX11Info::connection();
        xcb_ungrab_pointer(connection, XCB_TIME_CURRENT_TIME);
        NETRootInfo(connection, NET::WMMoveResize).moveResizeRequest(w->winId(), native.x() * dpiRatio, native.y() * dpiRatio, NET::Move);
//        qDebug()<<"x11 move end";

        if (e->source() == Qt::MouseEventSynthesizedByQt) {
//            if (!w->mouseGrabber()) {
//                //! \note Under XI, grabMouse will grab touch devices at the same time.
//                //! When the touch device grabbing changes, xserver will send TouchEndEvent
//                //! to the client that monitors the touch sequence.
//                //! \see qtbase QXcbConnection::xi2SetMouseGrabEnabled
//                w->grabMouse();
//                w->releaseMouse();
//            }
        }

        xcb_button_release_event_t* event = new xcb_button_release_event_t;
        memset(event, 0x00, sizeof(xcb_button_release_event_t));
        event->response_type = XCB_BUTTON_RELEASE;
        event->event = w->winId();
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = native.x();
        event->root_y = native.y();
        event->event_x = 0;
        event->event_y = 0;
        event->child = 0;
        event->state = 0;
        event->detail = XCB_BUTTON_INDEX_1;

        xcb_send_event(connection, false, w->winId(), XCB_EVENT_MASK_BUTTON_RELEASE, (char *) event);
        delete event;

        xcb_flush(connection);

        m_is_dragging = true;

        m_timer.start();
    } else {
        auto widget = qobject_cast<QWindow *>(obj);
        auto topLevel = qobject_cast<QWindow *>(obj);

#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
        topLevel->startSystemMove();
#else
        auto shellSurface = KWayland::Client::ShellSurface::fromWindow(topLevel);
        if (!shellSurface)
            return;
        shellSurface->requestMove(m_seat, m_serial);
#endif
        m_is_dragging = true;
    }

    return;
}

void UKUIWindowManager::mouseReleaseEvent(QObject *obj, QMouseEvent *e)
{
    //qDebug()<<"mouse release event";
    endDrag();
}

void UKUIWindowManager::endDrag()
{
    m_is_dragging = false;
    m_current_obj = nullptr;
    m_start_point = QPoint(0, 0);
    m_timer.stop();
}

bool UKUIWindowManager::isDragable(QWindow *window)
{
    if (window ) {
        if ((window->flags() & Qt::Popup) == Qt::Popup) {
            if ((window->flags() & Qt::Tool) == Qt::Tool)
                return true;
            return false;
        }
        return true;
    } else {
        return false;
    }
}



// AppEventFilter
AppEventFilter::AppEventFilter(UKUIWindowManager *parent) : QObject(parent)
{
    m_wm = parent;
}

bool AppEventFilter::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonRelease) {
        m_wm->m_prepared_to_drag = false;
        m_wm->m_is_dragging = false;
        m_wm->m_current_obj = nullptr;
        m_wm->m_timer.stop();
        m_wm->m_start_point = QPoint();
    }

//    if (m_wm->m_is_dragging && m_wm->m_current_obj && (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseMove)) {
//        QMouseEvent mouseEvent(QEvent::MouseButtonRelease,
//                               m_wm->m_start_point,
//                               Qt::LeftButton,
//                               Qt::LeftButton,
//                               Qt::NoModifier);
//        qApp->sendEvent(m_wm->m_current_obj, &mouseEvent);
//    }
    return false;
}
