#ifndef CMAINWND_H
#define CMAINWND_H

#include <QObject>
#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QJsonParseError>
#include <QJsonObject>
#include <QDesktopWidget>
#include <QApplication>
#include <QMenuBar>
#include <QToolButton>
#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QStackedWidget>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QScreen>
#include <QList>
#include <QClipboard>
#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QSpacerItem>
#include <QDir>
#include <QShowEvent>
#include <QTimer>
#include <QSessionManager>
#include <QProcess>
#include <QStandardPaths>
#include <QFileDialog>
#include <KGlobalAccel>
#include <KWindowSystem>
#include <unistd.h>
#include <QShortcut>
#include <QKeySequence>
#include <QDesktopServices>
#include <QUrl>
#include <QMetaObject>

#include <kysdk/applications/kwidget.h>
#include <ukui-log4qt.h>
#include <kysdk/diagnosetest/libkydatacollect.h>
#include <ukuistylehelper/ukuistylehelper.h>
#include <windowmanager/windowmanager.h>

#include "msgpane.h"

#include "dbus/kadbusoperator.h"
#include "textselect/clipboard.h"
#include "textselect/selection.h"
#include "setting/scribewordpage.h"
#include "global/kaglobal.h"
#include "textselect/selection.h"
#include "commandtemplatelist.h"
#include <custommessagebox.h>
#include "global/kaglobal.h"
#include "kaboutdialog.h"
#include "closemodewidget.h"
#include "windowhelper.h"
#include "x11windowhelper/x11windowhelper.h"

#define UKUI_FONT_SIZE "systemFontSize"
#define WINDOW_MARGIN 8
#define SIDEBAR_WIDTH 402
#define WINDOW_WIDTH 902
#define WINDOW_HEIGHT 568
#define WINDOW_MIN_WIDTH 768
#define WINDOW_MIN_HEIGHT 568

using namespace kdk;

enum MainWndViewType
{
    TABTITLE_MESSAGE = 0,       // 消息会话
    TABTITLE_VOICE = 1,      // 通讯录
};

class CMainWnd : public KWidget
{
    Q_OBJECT
public:

    explicit CMainWnd(QWidget *parent = nullptr);
    ~CMainWnd();

    static CMainWnd *getInstance();

    QGSettings *themeData = nullptr; //监测主题

    void checkTheme();
    void keepWindowAbove(bool needTokeepAbove);
    void setMovable(bool isMovable);
    // 设置窗口置顶状态（从外部调用）
    void setWindowAlwaysOnTop(bool checked);

public slots:
    void onFocusChanged(QWidget *oldWidget, QWidget *newWidget);

    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onPageChanged(const int pageNumber);
    void handleEnterMeetingRecordPage(bool entered);
    void autoShowSelectWidget(); //自动显示划词控件
    void handleSelectionTextChanged(); //响应划词内容变更信号
    void onCreateDialog();
    void onShowScribeWord();
    void onShowDeleteMessageBox(int msgIndex);
    void showOrHideWindow();
    void onShowHelp();
    void onShowAbout();
    void onActivateMainWindow();
    void onSaveAs(const QString &base64Image);  //响应qml发来的下载图片文件选择框

    void onNewConversation();
    Q_INVOKABLE QString getAiAssistantVersion();
    Q_INVOKABLE bool isAboutWindowOpen() const;
    Q_INVOKABLE void setAboutWindowOpen(bool open);
    Q_INVOKABLE void activateAboutWindow();

signals:
    void sigThemeChanged();
    void sigWindowAlwaysOnTopChanged(bool isTop); // 窗口置顶状态改变信号
    void sigShowAbout();
    void sigActivateAboutWindow();

private:
    void createAllChildWnd();
    void initSolts();
    void initLayout();
    void showView();
    void initScribeWords();
    void initPosition();
    void changeToOccupiedMode();
    void initScribeWordsSetting();
    void changeToNormalMode();
    void initShortcut();
    void initTopping();
    void closeOrHidden();

    void switchToSideMode(); // 切换到侧边栏模式
    void setSideModePos(); // 保持在当前屏幕
    void switchToWindowMode(); // 切换到窗口模式
    void setWindowModePos(); // 设置在当前屏幕居中
    void adjustForScreenChanged(); // 响应屏幕变化

private:
    bool m_isTop = false;
    bool isMsgBoxOpen = false;
    bool isCloseBoxOpen = false;
    bool m_aboutWindowOpen = false;

    CMsgPane *m_msgPane = nullptr;
    X11WindowHelper *m_x11WindowHelper = nullptr;

#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *m_ukuiWindowHelper = nullptr;
#else
    quint64 m_window_id = 0;
#endif
};

#endif // CMAINWND_H
