#include "selectcontrol.h"
#include <QObject>
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QAction>
#include <QKeySequence>
#include <KGlobalAccel>
#include "global/kaglobal.h" // 必须包含头文件
#include <x11windowhelper/x11windowhelper.h>
#include "configmanager.h"
#include "selectionpopupguard.h"

// 初始化划词控制器，监听剪切板变化，达到触发条件发送信号进行划词条显示，划词上内容点击后拼接内容发送到msgpane文本条进行处理
SelectControl* SelectControl::m_instance = nullptr;
extern QQmlApplicationEngine *g_qmlEngine; // 显式声明使用全局变量


SelectControl *SelectControl::getInstance(QObject *parent)
{
    if (!m_instance) {
          m_instance = new SelectControl(parent);
      }
    return m_instance;
}

void SelectControl::setSelectWindowVisible(bool visible)
{

    return;
}

SelectControl::SelectControl(QObject *parent)
    : QObject(parent)
{
    initConnect();
}

SelectControl::~SelectControl()
{

}

void SelectControl::onStartSendSelectMsg()
{
    return;
}

void SelectControl::initSelectControl()
{
    // 划词剪切板
    m_selectclip = new Clipboard;
    m_selectclip->setObjectName("kylin-aiassistant_Clipboard_m_selectclip");
    // 划词发送文字延迟定时器
    selectTimer  = new QTimer;
    selectTimer->setInterval(30);
    connect(selectTimer,&QTimer::timeout,this,&SelectControl::onStartSendSelectMsg,Qt::UniqueConnection);

    // 划词自动显示逻辑，2025.12.25新增逻辑
    m_clipboardTimer = new QTimer(this);
    m_clipboardTimer->setInterval(1000);
    m_clipboardTimer->setSingleShot(true);
    m_initClipText = QApplication::clipboard()->text(QClipboard::Selection);
    // 划词自动显示定时器，1s内未划词则自动显示划词按钮
    connect(m_clipboardTimer,&QTimer::timeout,this, &SelectControl::autoShowSelectWindow,Qt::UniqueConnection);
    // 基于划词内容变化显示划词条按钮
    connect(QApplication::clipboard(),&QClipboard::selectionChanged, this, &SelectControl::handleSelectionTextChanged,Qt::UniqueConnection);


    // ✅ 独立加载划词弹窗QML，创建顶级窗口（核心代码，保留）
    if(g_qmlEngine && !m_selectWindow)
    {
        QQmlComponent component(g_qmlEngine, QUrl("qrc:/qml/qml/SelectPopupIconWindow.qml"));
        // 修复：等待组件加载完成（解决异步加载失败）
        if(component.status() == QQmlComponent::Loading)
        {
            QEventLoop loop;
            connect(&component, &QQmlComponent::statusChanged, &loop, &QEventLoop::quit);
            loop.exec();
        }
        if(component.isReady())
        {
            QObject* obj = component.create();
            m_selectWindow = qobject_cast<QWindow*>(obj);
            if(m_selectWindow)
            {
                m_selectWindow->setObjectName("SelectScribeWindow");
                m_selectWindow->setVisible(false);
                // ✅ 关键：提前设置窗口flags，杜绝主窗口联动
                m_selectWindow->setFlags(SelectionPopupGuard::popupFlags(m_selectWindow->flags(), false));
                connect(m_selectWindow, &QWindow::activeChanged, this, &SelectControl::onSelectWindowActiveChanged,Qt::UniqueConnection);
                KyInfo() << "✅ 独立加载划词弹窗成功 → 窗口地址：" << m_selectWindow;
            }
        }
        else
        {
            KyInfo() << "❌ 加载划词弹窗失败：" << component.errorString();
        }
    }

    refreshGlobalScribeShortcut();

    return;
}

void SelectControl::initSelectSlots()
{
    return;
}

void SelectControl::onShowScribeWindow()
{
    //如果禁用划词则直接返回
    bool isSelectDisable = ConfigManager::getInstance()->getConfigValue("is-stroke-word").toBool();
    //    KyInfo() << "当前禁用划词显示的标志位为"<< isSelectDisable;
    if(!isSelectDisable){
        return;
    }

    //KyInfo() << "=======m_selectwid->isVisible()======" << m_selectWindow;
    if (!m_selectWindow) {
           KyInfo() << "❌ 划词窗口未初始化，无法显示";
           return;
    }

    const bool shortcutInvokeMode =
            !ConfigManager::getInstance()->getConfigValue("scribe-auto-invoke", true).toBool();
    bool invokeOk = false;
    bool resetOpacity = false;
    if (shortcutInvokeMode) {
        invokeOk = QMetaObject::invokeMethod(m_selectWindow, "showExpandedForShortcutInvoke",
                                            Qt::QueuedConnection);
        if (invokeOk) {
            KyInfo() << "快捷键唤起划词悬浮条，直接展开";
        }
    } else {
        if (m_selectWindow->property("globalReadState").toInt() != 0) {
            return; // An automatic selection change must not interrupt reading.
        }
        QMetaObject::invokeMethod(m_selectWindow, "clearShortcutInvokePinned", Qt::QueuedConnection);
        invokeOk = QMetaObject::invokeMethod(m_selectWindow, "prepareAutomaticSelection", Qt::DirectConnection);
        resetOpacity = QMetaObject::invokeMethod(m_selectWindow, "resetOpacity", Qt::QueuedConnection);
        if (invokeOk && resetOpacity) {
            KyInfo() << "成功调用QML窗口的onUpdateRectSize()函数";
        }
    }
    if (!invokeOk) {
        KyInfo() << "调用QML函数失败：函数名不存在/参数不匹配,应用外划词显示失败";
    }

     QPoint cursorPos = QCursor::pos();
    // 基于ukuiwindowhelper更新显示位置
#ifdef UKUI_WINDOWHELPER_SUPPORT
    if (m_ukuiWindowHelper) {
        m_ukuiWindowHelper->deleteLater();
        m_ukuiWindowHelper = nullptr;
    }
    m_ukuiWindowHelper = new UkuiWindowHelper(m_selectWindow);
    if (m_ukuiWindowHelper)
    {
        // 设置窗口角色+隐藏任务栏（项目标准配置）
        m_ukuiWindowHelper->removeTitleBar();
        m_ukuiWindowHelper->setSkipTaskBar(true);
        m_ukuiWindowHelper->setSkipSwitcher(true);

        // ✅ 关键3：先调用openUnderCursor（配置位置）→ 再设置窗口可见
        cursorPos.setX(10);
        cursorPos.setY(10);
        m_ukuiWindowHelper->openUnderCursor(cursorPos);
        KyInfo() << "✅ Wayland窗口位置配置完成 → 光标坐标：" << cursorPos;
    }
#else
     X11WindowHelper x11Helper;
     if (x11Helper.isX11Environment()) {
         x11Helper.removeTitleBar(m_selectWindow);
         // 设置跳过任务栏
         x11Helper.setSkipTaskbar(m_selectWindow, true);
         KyInfo() << "SelectControl: Title bar removed, skip taskbar using X11 native API";
     }
        QRect availableGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        // 调整划词弹出窗口位置，确保屏幕范围内可见，让0 <= x <= availableGeometry.right() - this->width(), 0 <= y <= availableGeometry.bottom() - this->height(),
        int x = qBound(0, cursorPos.x(), availableGeometry.right() - m_selectWindow->width());
        int y = qBound(0, cursorPos.y(), availableGeometry.bottom() - m_selectWindow->height());
        m_selectWindow->setPosition(x, y);
#endif

        // 显示窗口（出现动画与消失由 QML 控制：出现 200ms → 持续 2s → 消失 200ms，仅在鼠标移出后触发消失）
        if (!m_selectWindow->isVisible()) {
            m_selectWindow->show();
        }
        return;
}

void SelectControl::initConnect()
{

}

void SelectControl::autoShowSelectWindow()
{
    QString currentText = QApplication::clipboard()->text(QClipboard::Selection);
    if (!ConfigManager::getInstance()->getConfigValue("scribe-auto-invoke", true).toBool()
            || !SelectionPopupGuard::shouldShow(currentText, m_initClipText, m_lastValidText,
                                               m_isFirstValidSelect, isCursorInSelfWindow())) {
        return;
    }

    // 更新缓存，执行显示逻辑
    m_lastValidText = currentText;
    // 显示划词条
    onShowScribeWindow();

    return;
}

void SelectControl::handleSelectionTextChanged()
{
    if (!ConfigManager::getInstance()->getConfigValue("scribe-auto-invoke", true).toBool()) {
        return;
    }

    QString currentText = QApplication::clipboard()->text(QClipboard::Selection);

    if (isCursorInSelfWindow()) {
        m_clipboardTimer->stop();
        m_isFirstValidSelect = false;
        m_lastValidText = currentText;
        if (m_selectWindow && m_selectWindow->property("globalReadState").toInt() == 0
                && !m_selectWindow->property("shortcutInvokePinned").toBool()) {
            m_selectWindow->hide();
        }
        return;
    }

    if(currentText == m_initClipText) {
        m_clipboardTimer->stop();
        m_isFirstValidSelect = false;
        return;
    }

    if (currentText.isEmpty()) {
        m_clipboardTimer->stop();
        m_isFirstValidSelect = false;
        m_lastValidText.clear();
        return;
    }

    if (currentText == m_lastValidText) {
        return;
    }
    m_isFirstValidSelect = true;

    m_clipboardTimer->stop();
    m_clipboardTimer->start();

    return;
}

QString SelectControl::getCurSelectMsg()
{
    return m_selectclip->getClipText();
}

void SelectControl::showMainView()
{

    return;
}

void SelectControl::disableScribe()
{
    //关闭划词属性
    Globals::getinstance()->setScribeEnabled(false);
}

void SelectControl::invokeGlobalScribeFloatingBar()
{
    onShowScribeWindow();
}

void SelectControl::refreshGlobalScribeShortcut()
{
    if (!m_scribeShortcutInvokeAction) {
        m_scribeShortcutInvokeAction = new QAction(this);
        m_scribeShortcutInvokeAction->setObjectName(QStringLiteral("scribe-shortcut-invoke"));
        m_scribeShortcutInvokeAction->setText(tr("Show word selection floating bar (shortcut invoke mode)"));
        connect(m_scribeShortcutInvokeAction, &QAction::triggered,
                this, &SelectControl::invokeGlobalScribeFloatingBar);
    }

    const bool scribeEnabled = ConfigManager::getInstance()->getConfigValue("is-stroke-word", false).toBool();
    const bool autoInvoke = ConfigManager::getInstance()->getConfigValue("scribe-auto-invoke", true).toBool();

    KGlobalAccel *ga = KGlobalAccel::self();
    ga->removeAllShortcuts(m_scribeShortcutInvokeAction);

    if (!scribeEnabled || autoInvoke)
        return;

    const QList<QKeySequence> shortcuts = QList<QKeySequence>()
            << QKeySequence(QStringLiteral("Meta+Alt+C"));
    const bool ok = ga->setShortcut(m_scribeShortcutInvokeAction, shortcuts, KGlobalAccel::NoAutoloading);

    if (ok)
        KyInfo() << "[SelectControl] Registered global shortcut Meta+Alt+C for shortcut-invoke scribewords";
    else
        KyWarning() << "[SelectControl] Failed to register Meta+Alt+C for scribewords (KGlobalAccel)";
}

void SelectControl::showSettingPage()
{
    return;
}

bool SelectControl::isCursorInSelfWindow()
{
    // Global cursor coordinates/topLevelAt are not reliable on native Wayland.
    // focusWindow is process-local. Ignore our own editing/dialog selections;
    // explicit shortcut invocation has a separate path and remains available.
    QWindow *focused = QGuiApplication::focusWindow();
    return QGuiApplication::applicationState() == Qt::ApplicationActive
            && focused && focused != m_selectWindow && focused->isVisible();
}

void SelectControl::enableWindowFocus()
{
    if (!m_selectWindow) return;
    qDebug() << "设置划词焦点";

    if (!m_selectWindow) {
        qDebug() << "❌ 划词窗口未初始化，无法开启焦点";
        return;
    }
    qDebug() << "✅ 开启划词窗口焦点（UKUI+Qt兜底）";

    // 1. 原生Qt flags兜底：临时移除不接受焦点标志（核心，不能注释）
    m_selectWindow->setProperty("acceptsInputFocus", true);
    const Qt::WindowFlags winFlags = SelectionPopupGuard::popupFlags(m_selectWindow->flags(), true);
    if (winFlags != m_selectWindow->flags()) m_selectWindow->setFlags(winFlags);

#ifdef UKUI_WINDOWHELPER_SUPPORT
    // 2. UKUI窗口助手：全量设置状态（包含所有需要的属性，非增量）
    if (m_ukuiWindowHelper) {
        UkuiWindowHelper::States targetStates;
        targetStates |= UkuiWindowHelper::State::KeepAbove;
        targetStates |= UkuiWindowHelper::State::Focusable;
        targetStates |= UkuiWindowHelper::State::Activatable;
        m_ukuiWindowHelper->setWindowState(targetStates);
    }
#endif

    // 3. 强制激活窗口（确保焦点能被获取）
    m_selectWindow->requestActivate();
    return;
}

void SelectControl::restoreWindowNoFocus()
{
    if (!m_selectWindow) return;
    m_selectWindow->setProperty("acceptsInputFocus", false);
    const Qt::WindowFlags flags = SelectionPopupGuard::popupFlags(m_selectWindow->flags(), false);
    if (flags != m_selectWindow->flags()) m_selectWindow->setFlags(flags);
    qDebug() << "取消设置划词焦点";

#ifdef UKUI_WINDOWHELPER_SUPPORT
    if (m_ukuiWindowHelper) {
        UkuiWindowHelper::States targetStates;
        targetStates |= UkuiWindowHelper::State::KeepAbove;
        targetStates &= ~UkuiWindowHelper::State::Focusable;
        targetStates &= ~UkuiWindowHelper::State::Activatable;
        m_ukuiWindowHelper->setWindowState(targetStates);
    }
#endif

    return;
}

void SelectControl::onSelectWindowActiveChanged()
{
    if(!m_selectWindow) {
        return;
    }
    qDebug() << "检查到焦点变化"<< m_selectWindow->isActive();
    if(m_selectWindow->isActive() == false ) {
        restoreWindowNoFocus();
        Q_EMIT sigCloseReadingAndSelectWindow();
    }

    return;
}

void SelectControl::closeAllPopupWindows()
{
    qDebug() << "关闭所有活动的非模态弹窗";

    // 遍历所有顶级窗口
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        // 跳过主窗口和划词窗口本身
        if (!w || w == m_selectWindow) {
            continue;
        }

        // 跳过不可见的窗口
        if (!w->isVisible()) {
            continue;
        }

        // 跳过主窗口
        QObject *winObj = w;
        if (winObj && winObj->objectName() == "mainWindow") {
            continue;
        }

        // 检查窗口是否是弹窗类型（Dialog 或 ApplicationModal/WindowModal）
        if (w->flags().testFlag(Qt::Dialog)) {
            qDebug() << "关闭弹窗:" << w->title() << w;
            w->hide();
        }
    }
}

QPoint SelectControl::getGlobalCursorPosition()
{
    KyInfo() << "获取当前鼠标位置" << QCursor::pos();
    return QCursor::pos();
}

