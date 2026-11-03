#include "windowcontroller.h"
#include "../x11windowhelper/x11windowhelper.h"

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QCursor>
#include <QLocale>
class QApplicationPrivate;

#define WINDOW_MARGIN 8
#define SIDEBAR_WIDTH 402
#define WINMODE_WIDTH 1096
#define WINMODE_HEIGHT 688


#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<QApplication *>(QCoreApplication::instance()))

WindowController::WindowController(QObject *parent)
{
    m_winMode = static_cast<WindowMode>(ConfigManager::getInstance()->getConfigValue("win-mode", 3).toInt());
    m_isTopping = ConfigManager::getInstance()->getConfigValue("is-window-mode-topping").toBool();
    
    // 初始化 X11WindowHelper
    m_x11WindowHelper = new X11WindowHelper(this);

    connect(this, &WindowController::winModeChanged, this, &WindowController::initWindowData);
    initDbusWathcer();
}

void WindowController::setFilePathWithStart(const QString filePath)
{
    m_filePathWithStart = filePath;

    return;
}

bool WindowController::ensureWindow()
{
    if (m_mainWin)
        return true;
    if (!g_qmlEngine)
        return false;
    g_qmlEngine->load(QUrl("qrc:/qml/qml/main.qml"));
    return m_mainWin != nullptr;
}

void WindowController::onAppRestarted(const QString &message)
{
    KyInfo() << "====onAppRestarted====message" << message;
    if (!ensureWindow()) {
        KyInfo() << "Error: Window ID is nullptr!" << message;
        return;
    }

    if (message == "running") {
        // 点击图标时的统一处理：如果窗口在顶层显示则隐藏，否则显示到最前面
        onTrayIconClicked();
    } else {//当做文件路径处理
        if (!m_mainWin->isActive()) {
            showMainWindow();
        }

        QUrl url(message);
        Q_EMIT sendFile(url.path());
    }

    return;
}

void WindowController::clearWindowStyle(QObject *win)
{
    QWindow * curWin = qobject_cast<QWindow*>(win);

    if (curWin) {
        // Let UKUI hide only the title bar so the native resize borders remain available.
        curWin->setProperty("ukui_surface_no_titlebar", true);
#ifdef UKUI_WINDOWHELPER_SUPPORT
        UkuiWindowHelper *winHelper = new UkuiWindowHelper(curWin);
        winHelper->removeTitleBar();
        winHelper->setSkipTaskBar(true);
#else
        // Linux X11 环境: 使用原生 X11 API 移除标题栏和跳过任务栏
        X11WindowHelper x11Helper;
        if (x11Helper.isX11Environment()) {
            x11Helper.removeTitleBar(curWin);
//            x11Helper.setSkipTaskbar(curWin, true);
            KyInfo() << "WindowController: Title bar removed, skip taskbar using X11 native API";
        }
#endif
    }

    return;
}

///
/// \brief WindowController::initScreenWathcer 监听屏幕分辨率变化
///
void WindowController::initScreenWathcer()
{
    KyInfo() << "initScreenWathcer start";
    m_currentScreen = getCurrentScreen();
    if (m_currentScreen == nullptr || m_currentScreen->availableGeometry().isEmpty()) {
        m_currentScreen = QGuiApplication::primaryScreen();
    }

    if (m_currentScreen == nullptr) {
        KyInfo() << "11111====m_currentScreen is nullptr====";
        return;
    }

    // 屏幕的分辨率或可用显示区域变化
    m_availableGeometryChangedConnection = connect(m_currentScreen, &QScreen::availableGeometryChanged, this, [this](const QRect &geometry) {
        KyInfo() << "22222====availableGeometryChanged====" << geometry;
        adjustForScreenChanged();
    });

    // 主屏变化
    connect(qApp, &QApplication::primaryScreenChanged, this, [this](QScreen *newPrimaryScreen) {
        if (newPrimaryScreen == nullptr || newPrimaryScreen->availableGeometry().isEmpty()) {
            KyInfo() << "33333====primaryScreen is null, ignore====";
            return;
        }

        KyInfo() << "33333====primaryScreenChanged====" << newPrimaryScreen->name() << newPrimaryScreen->availableGeometry();
        // 检查当前屏幕是否还存在于屏幕列表中
        QList<QScreen *> allScreens = QGuiApplication::screens();
        bool currentScreenExists = allScreens.contains(m_currentScreen);

        if (!currentScreenExists) {
            KyInfo() << "33333====The current screen has been removed====";
            m_currentScreen = getCurrentScreen();
            if (m_currentScreen == nullptr || m_currentScreen->availableGeometry().isEmpty()) {
                m_currentScreen = newPrimaryScreen;
            }
            KyInfo() << "33333====updated to the new current screen====" << m_currentScreen->name();
        } else {
            KyInfo() << "33333====The current screen still exists====" << m_currentScreen->name();
        }

        disconnect(m_availableGeometryChangedConnection);
        adjustForScreenChanged();

        m_availableGeometryChangedConnection = connect(m_currentScreen, &QScreen::availableGeometryChanged, this, [this](const QRect &geometry) {
            KyInfo() << "44444====availableGeometryChanged====" << geometry;
            adjustForScreenChanged();
        });
    });

    // 屏幕被添加
    connect(qApp, &QGuiApplication::screenAdded, this, [=](QScreen *screen){
        KyInfo() << "55555====screenAdded====ignore";
    });
    // 屏幕被移除
    connect(qApp, &QGuiApplication::screenRemoved, this, [=](QScreen *screen){
        KyInfo() << "66666====screenRemoved====";
        if (screen == nullptr || screen->availableGeometry().isEmpty()) {
            KyInfo() << "66666====screen is null, ignore=====";
            return;
        }

        // 检查当前屏幕是否还存在于屏幕列表中
        QList<QScreen *> allScreens = QGuiApplication::screens();
        bool currentScreenExists = allScreens.contains(m_currentScreen);

        if (currentScreenExists == false) {
           KyInfo() << "66666====The current screen has been removed====";
           m_currentScreen = QGuiApplication::primaryScreen();
           if (m_currentScreen == nullptr || m_currentScreen->availableGeometry().isEmpty()) {
               KyInfo() << "66666====get current screen failed, return====";
               return;
           }

           KyInfo() << "66666====updated to the new current screen====" << m_currentScreen->name();
        } else {
           KyInfo() << "66666====The current screen still exists====" << m_currentScreen->name();
        }

        disconnect(m_availableGeometryChangedConnection);
        adjustForScreenChanged();

        m_availableGeometryChangedConnection = connect(m_currentScreen, &QScreen::availableGeometryChanged, this, [this](const QRect &geometry) {
            KyInfo() << "77777====availableGeometryChanged====" << geometry;
            adjustForScreenChanged();
        });
    });

    // 窗口所在屏幕变化
    connect(this, &WindowController::currentScreenChanged, this, [this]() {
        if (m_currentScreen == nullptr || m_currentScreen->availableGeometry().isEmpty()) {
            KyInfo() << "88888====currentScreenChanged====new screen is null, ignore====";
            return;
        }

        KyInfo() << "88888====currentScreenChanged====" << m_currentScreen->name() << m_currentScreen->availableGeometry();
        disconnect(m_availableGeometryChangedConnection);
        m_availableGeometryChangedConnection = connect(m_currentScreen, &QScreen::availableGeometryChanged, this, [this](const QRect &geometry) {
            KyInfo() << "99999====availableGeometryChanged====" << geometry;
            adjustForScreenChanged();
        });
    });

    initWindowData(m_winMode);
    KyInfo() << "initScreenWathcer end";
}

QScreen *WindowController::getCurrentScreen()
{
    QScreen *screen = nullptr;
#ifdef UKUI_WINDOWHELPER_SUPPORT
    const bool isWaylandPlatform = QGuiApplication::platformName().startsWith(
            QStringLiteral("wayland"), Qt::CaseInsensitive);
    if (isWaylandPlatform) {
        if (m_winHelper == nullptr) {
            KyInfo() << "===ukui window helper is null=====";
        } else {
            screen = m_winHelper->getCurrentScreenBlocking();
        }
    }
#endif

    if (screen == nullptr || screen->availableGeometry().isEmpty()) {
        screen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr || screen->availableGeometry().isEmpty()) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen;
}

void WindowController::setSideModePos()
{
    KyInfo() << "====setSideModePos====";
    if (m_currentScreen == nullptr) {
        KyInfo() << "m_currentScreen is nullptr, cannot set side mode pos";
        return;
    }

    QRect screenRect = m_currentScreen->availableGeometry();
    KyInfo() << "====currentScreen====name" << m_currentScreen->name();
    KyInfo() << "====availableGeometry====x:" << screenRect.x() << "y:" << screenRect.y()
             << "width:" << screenRect.width() << "height:" << screenRect.height()
             << "right:" << screenRect.right() << "top:" << screenRect.top();

    m_width = SIDEBAR_WIDTH;
    m_height = screenRect.height() - 2 * WINDOW_MARGIN;

    m_y = screenRect.top() + WINDOW_MARGIN;
    QString systemLang = QLocale::system().name();
    if (systemLang == "ug_CN" || systemLang == "ky_KG" || systemLang == "kk_KZ") {
        m_x = screenRect.x() + WINDOW_MARGIN;
    } else {
        m_x = screenRect.right() - SIDEBAR_WIDTH - WINDOW_MARGIN;
    }
    KyInfo() << "====setSideModePos====x:" << m_x << "y:" << m_y << "width:" << m_width << "height:" << m_height;
    Q_EMIT screenChanged();
}

void WindowController::setWindowModePos()
{
    //窗口在当前屏幕居中
    KyInfo() << "====setWindowModePos====";
    if (m_currentScreen == nullptr) {
        KyInfo() << "currentScreen is nullptr, cannot set window mode pos";
        return;
    }

    QRect screenRect = m_currentScreen->availableGeometry();
    KyInfo() << "====currentScreen====name" << m_currentScreen->name();
    KyInfo() << "====availableGeometry====x:" << screenRect.x() << "y:" << screenRect.y()
             << "width:" << screenRect.width() << "height:" << screenRect.height();

    m_width = WINMODE_WIDTH;
    m_height = WINMODE_HEIGHT;
    m_x = screenRect.x() + (screenRect.width() - WINMODE_WIDTH) / 2;
    m_y = screenRect.y() + (screenRect.height() - WINMODE_HEIGHT) / 2;
    KyInfo() << "====setWindowModePos====x:" << m_x << "y:" << m_y << "width:" << m_width << "height:" << m_height;
    Q_EMIT screenChanged();
}

void WindowController::adjustForScreenChanged()
{
    if (m_winMode == WINDOW_MODE_SIDEMODE) {
        setSideModePos();
    }
}

void WindowController::initDbusWathcer()
{
    m_dbusOperator = new KADBusOperator(this);
    connect(m_dbusOperator, &KADBusOperator::sigShowOrHideView, this, &WindowController::showOrHideWindow);
    connect(m_dbusOperator, &KADBusOperator::sigNotifyHistoryChanged, this, &WindowController::historyChanged);
    connect(m_dbusOperator, &KADBusOperator::sigShowChatAndSendText, this, [this](const QString &text) {
        showMainWindow();
        Q_EMIT sendVoiceTextToChat(text);
    });

    m_aiBallManager = new AIBallManager(this);
    connect(m_dbusOperator, &KADBusOperator::sigShowAIBall, m_aiBallManager, &AIBallManager::setAIBallVisible);
    connect(m_aiBallManager, &AIBallManager::textSubmitted, this, [this](const QString &text) {
        showMainWindow();
        Q_EMIT sendVoiceTextToChat(text);
    });

    return;
}

void WindowController::initWindowData(const WindowMode winMode)
{
    KyInfo() << "===initWindowData=====" << winMode;
    QScreen *currentScreen = getCurrentScreen();
    if (currentScreen == nullptr || currentScreen->availableGeometry().isEmpty()) {
        KyInfo() << "current screen is null, return";
        return;
    }

    if (m_currentScreen != currentScreen) {
        m_currentScreen = currentScreen;
        emit currentScreenChanged();
    }

    if (winMode == WINDOW_MODE_SIDEMODE) {
        setSideModePos();
    } else if (winMode == WINDOW_MODE_WINDOWMODE) {
        setWindowModePos();
    }

    return;
}

void WindowController::setWindow(QObject *win)
{
    KyInfo() << "setWindow start";
    m_mainWin = qobject_cast<QWindow*>(win);

    if (m_mainWin) {
        clearWindowStyle(win);
#ifdef UKUI_WINDOWHELPER_SUPPORT
        m_winHelper = new UkuiWindowHelper(m_mainWin);
        // 最大化按钮分屏功能
        m_maxBtnHoverDelayTimer = new QTimer(this);
        m_maxBtnHoverDelayTimer->setInterval(500);
        m_maxBtnHoverDelayTimer->setSingleShot(true);
        connect(m_maxBtnHoverDelayTimer, &QTimer::timeout, this, [this](){
            m_winHelper->showTileFlyout(m_winRect);
        });
#endif

        // 追踪窗口最大化状态，用于最小化后恢复时保持最大化
        connect(m_mainWin, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
            if (visibility == QWindow::Maximized) {
                m_wasMaximized = true;
            } else if (visibility == QWindow::Windowed) {
                m_wasMaximized = false;
            }
            // Minimized / Hidden 时不改变 m_wasMaximized，保留之前的状态
        });

        // 设置窗口支持拖拽
        UKUIWindowManager::instance()->registerWindow(m_mainWin);
    }

    bool isMoveable = (m_winMode == WindowMode::WINDOW_MODE_WINDOWMODE) ? true : false;
    setWindowProperty(isMoveable, m_isTopping);

    KyInfo() << "setWindow end";
    return;
}

void WindowController::setWinMode(int newWinMode)
{
    WindowMode mode = static_cast<WindowMode>(newWinMode);
    if (m_winMode == mode) {
        return;
    }

    m_winMode = mode;
    ConfigManager::getInstance()->updateConfig("win-mode", m_winMode);
    KyInfo() << "setWinMode====m_winMode" << m_winMode;
    Q_EMIT winModeChanged(m_winMode);

    bool isMoveable = (mode == WindowMode::WINDOW_MODE_WINDOWMODE) ? true : false;
    setWindowProperty(isMoveable, m_isTopping);

    return;
}

void WindowController::showMainWindow()
{
    KyInfo() << "show main window";
    if (!ensureWindow()) {
        KyInfo() << "ERROR: 无法获取窗口ID";
        return;
    }

    // 确保窗口可见（静默模式下首次加载 QML 时，窗口为 hidden 状态）
    m_mainWin->setVisible(true);

    QScreen *currentScreen = getCurrentScreen();
    if (currentScreen == nullptr || currentScreen->availableGeometry().isEmpty()) {
        KyInfo() << "current screen is unavailable, showing window without repositioning";
    } else {
        if (m_currentScreen != currentScreen) {
            m_currentScreen = currentScreen;
            emit currentScreenChanged();
        }

        if (m_winMode == WINDOW_MODE_SIDEMODE) {
            setSideModePos();
        } else if (m_winMode == WINDOW_MODE_WINDOWMODE && m_isFirstShow) {
            setWindowModePos();
            m_isFirstShow = false;
        }
    }

    // 如果窗口之前处于最大化状态，恢复最大化；否则正常显示
    if (m_wasMaximized && m_mainWin->visibility() != QWindow::Maximized) {
        m_mainWin->setVisibility(QWindow::Maximized);
    } else {
        m_mainWin->show();
    }

    m_mainWin->raise();
    m_mainWin->requestActivate();
    KyInfo() << "show main window end";

    // 在 Linux X11 环境下，使用原生 X11 API 确保窗口置顶显示
#ifdef Q_OS_LINUX
    if (m_x11WindowHelper && m_x11WindowHelper->isX11Environment()) {
        m_x11WindowHelper->raiseAndActivateWindow(m_mainWin);
    }
#endif

    return;
}

///
/// \brief WindowController::showOrHideWindow 全局快捷键响应槽函数，用于实现按下快捷键时，
/// 唤出或隐藏窗口。
///
void WindowController::showOrHideWindow()
{
    if (!ensureWindow()) {
        KyInfo() << "ERROR: 无法获取窗口ID";
        return;
    }

    KyInfo() << "----showorhidewindow---- 窗口是否激活？ isActive()" << m_mainWin->isActive();
    KyInfo() << "----showorhidewindow---- 窗口是否可见？  isVisible()" << m_mainWin->isVisible();
    KyInfo() << "----showorhidewindow---- 窗口Visiability  visibility()" << m_mainWin->visibility();

    if (m_mainWin->isVisible()) {
        // 主窗口最小化  或  主窗口不是当前活动窗口（失去焦点）时
        if (m_mainWin->visibility() == QWindow::Visibility::Minimized || !m_mainWin->isActive()) {
            showMainWindow();
        } else {
            // 检查是否有活动的模态窗口（QWidget或QML Window），有则不允许隐藏
            if (QApplication::activeModalWidget()) {
                KyInfo() << "====modal dialog is active, preventing hide====";
                return;
            }

            for (QWindow *w : QGuiApplication::topLevelWindows()) {
                if (w != m_mainWin && w->modality() != Qt::NonModal && w->isVisible()) {
                    KyInfo() << "----modal window is active, preventing hide----";
                    return;
                }
            }

            KyInfo() << "----hide----";
            m_mainWin->hide();
        }
    } else {
        showMainWindow();
    }
}

///
/// \brief WindowController::onTrayIconClicked
/// 点击托盘图标时的响应：如果窗口在顶层显示则隐藏，否则显示到最前面
///
void WindowController::onTrayIconClicked()
{
    if (!ensureWindow()) {
        KyInfo() << "ERROR: 无法获取窗口ID";
        return;
    }

    KyInfo() << "----toggleWindowOnIconClick---- isVisible()" << m_mainWin->isVisible()
             << "isActive()" << m_mainWin->isActive()
             << "visibility()" << m_mainWin->visibility();

    // 如果窗口不可见，直接显示
    if (!m_mainWin->isVisible()) {
        showMainWindow();
        return;
    }

    // 如果窗口可见且在顶层（活动状态），则隐藏
    if (m_mainWin->isActive()) {
        // 检查是否有活动的模态窗口，有则不允许隐藏
        if (QApplication::activeModalWidget()) {
            KyInfo() << "====toggleWindowOnIconClick: modal dialog is active, preventing hide====";
            return;
        }

        for (QWindow *w : QGuiApplication::topLevelWindows()) {
            if (w != m_mainWin && w->modality() != Qt::NonModal && w->isVisible()) {
                KyInfo() << "----toggleWindowOnIconClick: modal window is active, preventing hide----";
                return;
            }
        }

        KyInfo() << "----toggleWindowOnIconClick: hide----";
        m_mainWin->hide();
    } else {
        // 窗口可见但不在顶层（被其他窗口遮挡），将其提升到顶层
        showMainWindow();
    }
}

void WindowController::checkUnProcessFile()
{
    if (m_filePathWithStart == ""){
        return;
    }

    KyInfo() << "start with file, checkUnProcessFile file path:" << m_filePathWithStart;
    QUrl url(m_filePathWithStart);
    Q_EMIT sendFile(url.path());
}

void WindowController::showTileFlyout(const QRect &rect)
{
#ifdef UKUI_WINDOWHELPER_SUPPORT
    m_winRect = rect;
    m_maxBtnHoverDelayTimer->start();
#endif

    return;
}

void WindowController::hideTileFlyout()
{
#ifdef UKUI_WINDOWHELPER_SUPPORT
    m_maxBtnHoverDelayTimer->stop();
#endif

    return;
}

void WindowController::setKeepAbove(bool flag)
{
    // 使用 X11WindowHelper 设置窗口置顶 (Linux X11 环境)
    if (m_x11WindowHelper && m_x11WindowHelper->isX11Environment() && m_mainWin) {
        m_x11WindowHelper->setWindowAlwaysOnTop(m_mainWin, flag);
        KyInfo() << "X11WindowHelper: Set always on top to" << flag;
    } else {
        // 非 X11 环境或非 UKUI 环境使用原有的 setWindowProperty
        bool isMoveable = (m_winMode == WindowMode::WINDOW_MODE_WINDOWMODE) ? true : false;
        setWindowProperty(isMoveable, flag);
    }

    return;
}

void WindowController::setWindowProperty(bool isMoveable, bool isTopping)
{
#ifdef UKUI_WINDOWHELPER_SUPPORT
    if(!m_winHelper) {
        return;
    }

    UkuiWindowHelper::States states;
    states |= UkuiWindowHelper::State::Minimizable;
    states |= UkuiWindowHelper::State::Maximizable;
    states |= UkuiWindowHelper::State::Closeable;
    states |= UkuiWindowHelper::State::Fullscreenable;
    states |= UkuiWindowHelper::State::Resizable;
    states |= UkuiWindowHelper::State::Focusable;
    states |= UkuiWindowHelper::State::Activatable;

    if (isMoveable){
        states |= UkuiWindowHelper::State::Movable;
    }

    if (isTopping) {
        states |= UkuiWindowHelper::State::KeepAbove;
    }

    m_winHelper->setWindowState(states);
#endif

    // X11 环境使用原生 API 设置置顶
    if (m_x11WindowHelper && m_x11WindowHelper->isX11Environment() && m_mainWin) {
        m_x11WindowHelper->setWindowAlwaysOnTop(m_mainWin, isTopping);
    }

    return;
}
