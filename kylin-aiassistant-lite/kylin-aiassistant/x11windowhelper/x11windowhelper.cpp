#include "x11windowhelper.h"
#include <QGuiApplication>
#include <QWindow>
#include <QQuickWindow>

// X11 头文件只在 Linux 下包含
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

X11WindowHelper::X11WindowHelper(QObject *parent)
    : QObject(parent)
    , m_isX11(false)
{
#ifdef Q_OS_LINUX
    // Wayland sessions commonly expose DISPLAY for XWayland. Only the xcb
    // platform plugin produces an X11 window ID that is valid for Xlib calls.
    m_isX11 = QGuiApplication::platformName().compare(QStringLiteral("xcb"), Qt::CaseInsensitive) == 0;
    if (m_isX11) {
        qDebug() << "X11WindowHelper: X11 environment detected";
    } else {
        qDebug() << "X11WindowHelper: X11 operations disabled for Qt platform"
                 << QGuiApplication::platformName();
    }
#else
    qDebug() << "X11WindowHelper: Not running on Linux";
#endif
}

X11WindowHelper::~X11WindowHelper()
{
}

WId X11WindowHelper::getX11WindowId(QWindow *window)
{
    if (!window) {
        qWarning() << "X11WindowHelper: Invalid window pointer";
        return 0;
    }

    // 对于 QQuickWindow，需要获取其 underlying QWindow
    QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window);
    if (quickWindow) {
        return quickWindow->winId();
    }

    return window->winId();
}

bool X11WindowHelper::removeTitleBar(QObject *window)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(window);
    qWarning() << "X11WindowHelper::removeTitleBar: Not supported on this platform";
    emit operationFailed("Not supported on this platform");
    return false;
#else
    if (!m_isX11) {
        qWarning() << "X11WindowHelper::removeTitleBar: Not running in X11 environment";
        emit operationFailed("Not running in X11 environment");
        return false;
    }

    if (!window) {
        qWarning() << "X11WindowHelper::removeTitleBar: Invalid window object";
        emit operationFailed("Invalid window object");
        return false;
    }

    QWindow *qwindow = qobject_cast<QWindow *>(window);
    if (!qwindow) {
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (quickWindow) {
            qwindow = quickWindow;
        } else {
            qWarning() << "X11WindowHelper::removeTitleBar: Object is not a QWindow";
            emit operationFailed("Object is not a QWindow");
            return false;
        }
    }

    WId x11WindowId = getX11WindowId(qwindow);
    if (x11WindowId == 0) {
        qWarning() << "X11WindowHelper::removeTitleBar: Failed to get X11 window ID";
        emit operationFailed("Failed to get X11 window ID");
        return false;
    }

    qDebug() << "X11WindowHelper::removeTitleBar: Removing title bar for window ID:" << x11WindowId;

    bool success = removeX11WindowDecoration(x11WindowId);
    if (success) {
        qDebug() << "X11WindowHelper::removeTitleBar: Title bar removed successfully";
        emit titleBarRemoved();
    } else {
        qWarning() << "X11WindowHelper::removeTitleBar: Failed to remove title bar";
        emit operationFailed("Failed to remove title bar");
    }

    return success;
#endif
}

bool X11WindowHelper::isX11Environment()
{
    return m_isX11;
}

bool X11WindowHelper::setWindowAlwaysOnTop(QObject *window, bool above)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(window);
    Q_UNUSED(above);
    return false;
#else
    if (!m_isX11) {
        qWarning() << "X11WindowHelper::setWindowAlwaysOnTop: Not in X11 environment";
        return false;
    }

    if (!window) {
        return false;
    }

    QWindow *qwindow = qobject_cast<QWindow *>(window);
    if (!qwindow) {
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (quickWindow) {
            qwindow = quickWindow;
        } else {
            return false;
        }
    }

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "X11WindowHelper: Cannot open display";
        return false;
    }

    WId winId = getX11WindowId(qwindow);
    if (winId == 0) {
        qWarning() << "X11WindowHelper: Invalid window ID";
        XCloseDisplay(display);
        return false;
    }

    Atom netWmState = XInternAtom(display, "_NET_WM_STATE", True);
    Atom atomAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", True);

    if (netWmState == None || atomAbove == None) {
        qWarning() << "X11WindowHelper: Failed to get atoms";
        XCloseDisplay(display);
        return false;
    }

    qDebug() << "X11WindowHelper: Window ID:" << winId << "above:" << above;

    long action = above ? 1 : 0;  // 1 = ADD, 0 = REMOVE

    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = winId;
    xev.xclient.message_type = netWmState;
    xev.xclient.display = display;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = action;
    xev.xclient.data.l[1] = atomAbove;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(display);

    qDebug() << "X11WindowHelper: Window always on top set to:" << above;

    XCloseDisplay(display);
    return true;
#endif
}

bool X11WindowHelper::raiseAndActivateWindow(QObject *window)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(window);
    return false;
#else
    if (!m_isX11) {
        qWarning() << "X11WindowHelper::raiseAndActivateWindow: Not in X11 environment";
        return false;
    }

    if (!window) {
        qWarning() << "X11WindowHelper::raiseAndActivateWindow: Invalid window object";
        return false;
    }

    QWindow *qwindow = qobject_cast<QWindow *>(window);
    if (!qwindow) {
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (quickWindow) {
            qwindow = quickWindow;
        } else {
            qWarning() << "X11WindowHelper::raiseAndActivateWindow: Object is not a QWindow";
            return false;
        }
    }

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "X11WindowHelper::raiseAndActivateWindow: Cannot open display";
        return false;
    }

    WId winId = getX11WindowId(qwindow);
    if (winId == 0) {
        qWarning() << "X11WindowHelper::raiseAndActivateWindow: Invalid window ID";
        XCloseDisplay(display);
        return false;
    }

    // 1. XRaiseWindow - 将窗口提升到堆叠顺序的最上层
    XRaiseWindow(display, winId);

    // 2. XSetInputFocus - 设置输入焦点到该窗口
    // 使用 RevertToParent 表示如果窗口不可见，焦点会回到父窗口
    XSetInputFocus(display, winId, RevertToParent, CurrentTime);

    // 3. 发送 _NET_ACTIVE_WINDOW 消息，通知窗口管理器激活窗口
    Atom netWmState = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    if (netWmState != None) {
        XEvent xev;
        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.xclient.window = winId;
        xev.xclient.message_type = netWmState;
        xev.xclient.display = display;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 2;  // 1 = 从应用程序请求激活
        xev.xclient.data.l[1] = CurrentTime;
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = 0;

        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }

    XFlush(display);
    qDebug() << "X11WindowHelper::raiseAndActivateWindow: Window raised and activated, ID:" << winId;

    XCloseDisplay(display);
    return true;
#endif
}

bool X11WindowHelper::setSkipTaskbar(QObject *window, bool skipTaskbar)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(window);
    Q_UNUSED(skipTaskbar);
    return false;
#else
    if (!m_isX11) {
        qWarning() << "X11WindowHelper::setSkipTaskbar: Not in X11 environment";
        return false;
    }

    if (!window) {
        qWarning() << "X11WindowHelper::setSkipTaskbar: Invalid window object";
        return false;
    }

    QWindow *qwindow = qobject_cast<QWindow *>(window);
    if (!qwindow) {
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (quickWindow) {
            qwindow = quickWindow;
        } else {
            qWarning() << "X11WindowHelper::setSkipTaskbar: Object is not a QWindow";
            return false;
        }
    }

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "X11WindowHelper::setSkipTaskbar: Cannot open display";
        return false;
    }

    WId winId = getX11WindowId(qwindow);
    if (winId == 0) {
        qWarning() << "X11WindowHelper::setSkipTaskbar: Invalid window ID";
        XCloseDisplay(display);
        return false;
    }

    // 设置 _NET_WM_STATE 跳过属性
    Atom netWmState = XInternAtom(display, "_NET_WM_STATE", True);
    Atom netWmStateSkipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", True);

    QList<Atom> atoms;
    if (skipTaskbar && netWmStateSkipTaskbar != None) {
        atoms.append(netWmStateSkipTaskbar);
    }

    if (!atoms.isEmpty() && netWmState != None) {
        // 删除现有属性
        XDeleteProperty(display, winId, netWmState);

        // 设置新属性
        Atom *atomArray = new Atom[atoms.size()];
        for (int i = 0; i < atoms.size(); ++i) {
            atomArray[i] = atoms[i];
        }
        XChangeProperty(display, winId, netWmState, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)atomArray, atoms.size());
        delete[] atomArray;

        // 发送 _NET_WM_STATE 消息通知窗口管理器
        for (int i = 0; i < atoms.size(); ++i) {
            XEvent xev;
            memset(&xev, 0, sizeof(xev));
            xev.type = ClientMessage;
            xev.xclient.window = winId;
            xev.xclient.message_type = netWmState;
            xev.xclient.display = display;
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
            xev.xclient.data.l[1] = atoms[i];
            xev.xclient.data.l[2] = 0;
            xev.xclient.data.l[3] = 1;  // source: application
            xev.xclient.data.l[4] = 0;

            XSendEvent(display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        }
    }

    XFlush(display);
    qDebug() << "X11WindowHelper::setSkipTaskbar: skipTaskbar =" << skipTaskbar << "winId =" << winId;

    XCloseDisplay(display);
    return true;
#endif
}

bool X11WindowHelper::removeX11WindowDecoration(WId x11Window)
{
#ifdef Q_OS_LINUX
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "X11WindowHelper: Failed to open X11 display";
        return false;
    }

    struct MotifHints {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    };

    MotifHints hints;
    hints.flags = (1L << 0) | (1L << 1); // MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS
    hints.functions = (1L << 0);         // MWM_FUNC_ALL
    hints.decorations = (1L << 1);       // MWM_DECOR_BORDER
    hints.input_mode = 0;
    hints.status = 0;

    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    if (motif_wm_hints != None) {
        XChangeProperty(display, x11Window, motif_wm_hints, motif_wm_hints, 32,
                       PropModeReplace, (unsigned char *)&hints, 5);
        qDebug() << "X11WindowHelper: Applied MOTIF_WM_HINTS to remove decorations";
    } else {
        qWarning() << "X11WindowHelper: _MOTIF_WM_HINTS atom not available";
    }

    Atom kde_frame_strut = XInternAtom(display, "_KDE_NET_WM_FRAME_STRUT", True);
    if (kde_frame_strut != None) {
        unsigned long frame_strut[4] = {0, 0, 0, 0};
        XChangeProperty(display, x11Window, kde_frame_strut, XA_CARDINAL, 32,
                       PropModeReplace, (unsigned char *)frame_strut, 4);
    }

    XFlush(display);
    XCloseDisplay(display);

    qDebug() << "X11WindowHelper: Window decorations removed via X11 API";
    return true;
#else
    Q_UNUSED(x11Window);
    return false;
#endif
}

