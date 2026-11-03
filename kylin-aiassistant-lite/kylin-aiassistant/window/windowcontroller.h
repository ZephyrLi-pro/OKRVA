#ifndef WINDOWCONTROLLER_H
#define WINDOWCONTROLLER_H

#include <QObject>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QGuiApplication>
#include <QTimer>

#include "windowhelper.h"
#include "global/kaglobal.h"
#include "dbus/kadbusoperator.h"
#include "x11windowhelper/x11windowhelper.h"
#include "ukuiwindowmanager.h"
#include "voice-kyling/aiballmanager.h"

class WindowController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int width READ width FINAL)
    Q_PROPERTY(int height READ height FINAL)
    Q_PROPERTY(int x READ x FINAL)
    Q_PROPERTY(int y READ y FINAL)
    Q_PROPERTY(WindowMode winMode READ winMode WRITE setWinMode NOTIFY winModeChanged FINAL)
    Q_PROPERTY(bool isTopping READ isTopping WRITE setIsTopping NOTIFY isToppingChanged FINAL)


public:
    enum WindowMode
    {
        WINDOW_MODE_NORMAL = 0,
        WINDOW_MODE_OCCUPIED = 1,
        WINDOW_MODE_SIDEMODE = 2,
        WINDOW_MODE_WINDOWMODE= 3,
    };
    Q_ENUM(WindowMode)

    explicit WindowController(QObject *parent = nullptr);

    int height() const { return m_height; }
    int width() const { return m_width; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    bool isTopping() const {return m_isTopping;}
    WindowMode winMode() const {return m_winMode;}
    void setFilePathWithStart(const QString filePath);
    bool ensureWindow();

signals:
    void winModeChanged(WindowMode winMode);
    void isToppingChanged();
    void screenChanged();
    void sendFile(const QString &filePath);
    void sendVoiceTextToChat(const QString &text);
    void currentScreenChanged();
    void historyChanged();

public slots:
    void onAppRestarted(const QString &message);
    ///
    /// \brief clearWindowStyle 清除窗管样式
    /// \param win 传入QML ID作为窗口ID
    ///
    void clearWindowStyle(QObject *win);
    void initScreenWathcer();
    void initWindowData(const WindowMode winMode);
    void setWindow(QObject *win);
    void setWinMode(int newWinMode);
    void setIsTopping(int flag) {
        if (m_isTopping != flag) {
            m_isTopping = flag;
            ConfigManager::getInstance()->updateConfig("is-window-mode-topping", flag);
            setKeepAbove(flag);
            emit isToppingChanged();
        }
    }
    void showMainWindow();
    void showOrHideWindow();
    /// 点击托盘图标时的响应：如果窗口在顶层显示则隐藏，否则显示
    void onTrayIconClicked();
    void checkUnProcessFile();
    // 最大化按钮悬浮有双屏四屏功能
    void showTileFlyout(const QRect &rect);
    void hideTileFlyout();

private:
    void setKeepAbove(bool flag);
    void setWindowProperty(bool isMoveable, bool isTopping);
    void initDbusWathcer();
    QScreen *getCurrentScreen(); // 获取当前所在屏幕
    void setSideModePos(); // 保持在当前屏幕
    void setWindowModePos(); // 设置在当前屏幕居中
    void adjustForScreenChanged(); // 响应屏幕变化

    X11WindowHelper *m_x11WindowHelper = nullptr;


private:
    int m_height = 0;
    int m_width = 0;
    int m_x = 0;
    int m_y = 0;
    bool m_isTopping = false;
    QWindow *m_mainWin = nullptr;
    WindowMode m_winMode = WINDOW_MODE_WINDOWMODE;
#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *m_winHelper = nullptr;
    QTimer *m_maxBtnHoverDelayTimer = nullptr;
    QRect m_winRect;
#endif
    QMetaObject::Connection m_availableGeometryChangedConnection;
    KADBusOperator *m_dbusOperator = nullptr;
    AIBallManager *m_aiBallManager = nullptr;
    QString m_filePathWithStart = "";
    QScreen *m_currentScreen = nullptr;
    bool m_isFirstShow = true;
    bool m_wasMaximized = false; // 记录窗口是否处于最大化状态

};

#endif // WINDOWCONTROLLER_H
