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
#include "selection.h"
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QPoint>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnection>
#include <QDebug>
#include <QMenu>
#include <QTimer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QScreen>
#include <QtGlobal>
#include "mypushbutton.h"
#include <windowmanager/windowmanager.h>
#include <kysdk/diagnosetest/libkydatacollect.h>

#include "windowhelper.h"

SelectWidget::SelectWidget(QWidget *parent)
{
    this->setAttribute(Qt::WA_TranslucentBackground); // 开启透明背景
    this->setStyleSheet("background: transparent;"); // 样式表强制透明
    this->setWindowFlags(Qt::WindowStaysOnTopHint);

    //初始化划词主界面
    initQmlSelectWidget();
    this->setContentsMargins(0,0,0,0);
    //初始化更多指令界面
    initSelectMoreCommandWidget();
    //初始化划词图标
    initSelectPopupIconWindow();
    //初始化划词动画
    initAnimation();
    //初始化划词菜单
    initMenu();
    //添加事件过滤器
    installEventFilter(this);

#ifdef UKUI_WINDOWHELPER_SUPPORT
    m_ukuiWindowHelper = new UkuiWindowHelper(this);
    m_ukuiWindowHelper->setWindowRole((UkuiWindowHelper::WindowRole::Tooltip));

#endif
}

void SelectWidget::initQmlSelectWidget()
{

    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setAccessibleName("kylin-aiassistant_QQuickWidget_m_quickWidget");
    m_quickWidget->setContentsMargins(0,0,0,0);
    m_quickWidget->setAttribute(Qt::WA_TranslucentBackground); // 开启透明
    m_quickWidget->setStyleSheet("background: transparent;"); // 样式表透明
    m_quickWidget->setClearColor(Qt::transparent); // QML 画布背景透明
    m_quickWidget->setWindowFlags(m_quickWidget->windowFlags() | Qt::FramelessWindowHint); // 无边框（可选，增强透明效果）
    //添加指令模型
    m_quickWidget->rootContext()->setContextProperty("commandTemplateListModel", CommandTemplateList::getInstance());
    m_quickWidget->setSource(QUrl("qrc:/qml/qml/Select.qml"));

    //设置属性不显示任务栏缩略图
#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *ukuiWindowHelper = new UkuiWindowHelper(this);
    ukuiWindowHelper->removeTitleBar();
    ukuiWindowHelper->setSkipTaskBar(true);
    ukuiWindowHelper->setSkipSwitcher(true);
#else
    kdk::UkuiStyleHelper::self()->removeHeader(this);
    kdk::WindowManager::setSkipTaskBar(this->windowHandle(), true);
    kdk::WindowManager::setSkipSwitcher(this->windowHandle(),true);
#endif

    //链接发送命令信号
    //在quickwidget中的qml发送一个带参数的信号槽到quickwidget所在的cpp中，然后建立信号槽的链接
    QQuickItem *item = m_quickWidget->rootObject();
    if(item){
        QObject::connect(item, SIGNAL(sigSendButtonText(QString, int)),this, SLOT(sendButtonText(QString, int)));
        QObject::connect(item, SIGNAL(sigResizeRect(int,int)),this,SLOT(slResizeRect(int,int)),Qt::QueuedConnection);
        QObject::connect(item,SIGNAL(sigsettingButtonLeftClicked()),this,SLOT(slShowContextMenu()));
        QObject::connect(item,SIGNAL(sigShowToolTip(bool)),this,SLOT(slShowToolTip(bool)));
        QObject::connect(this, &SelectWidget::sigDisableMorePage,item,[=]{
            //执行隐藏更多界面功能
            QMetaObject::invokeMethod(item, "onsigDisableMorePage");
        });
        QObject::connect(this,&SelectWidget::sigUpdateSelectEnableStatus,item,[=](bool isEnabled){
            //判断是否选中内容后更新划词悬浮条的按钮和菜单状态
            if(isEnabled){
                QMetaObject::invokeMethod(item, "onsigEnableSelectStatus");
            }
            else {
                QMetaObject::invokeMethod(item, "onsigDisableSelectStatus");

            }

        });
        // 隐藏划词条后处理逻辑
        QObject::connect(item, SIGNAL(sigHideWindowFinished()), this, SLOT(onHideAnimationFinished()));
        // 获取焦点
        QObject::connect(item, SIGNAL(sigGetFocus()), this, SLOT(slEnableSelectFocus()));
    }

    return;
}
void SelectWidget::sendButtonText(const QString &currentButtonText,const int currentId)
{
    m_clickFlag = true;
    //填充当前按钮文本
    if(!currentButtonText.isEmpty())
    {
        KyInfo() << "currentButtonText" << currentButtonText;
        buttonText = currentButtonText;
        m_selectMoreCommandWidget->setVisible(false);
        emit sigDisableMorePage();
    }
    if(currentId != -1)
    {
        KyInfo() << "currentId" << currentId;
        promptId = currentId;
    }
    emit sigbuttontext();
}

void SelectWidget::slResizeRect(const int width, const int height)
{
    m_quickWidget->setFixedSize(width,height);
    this->setFixedSize(width, height);
    m_clickFlag = true;

    if (m_selectMoreCommandWidget == nullptr) {
        KyInfo() << "selectMoreCommandWidget is nullptr";
        return;
    }

    return;
}

void SelectWidget::initSelectMoreCommandWidget()
{
    m_selectMoreCommandWidget = new SelectMoreCommandWidget(this);
    m_selectMoreCommandWidget->setAccessibleName("kylin-aiassistant_SelectMoreCommandWidget_m_selectMoreCommandWidget");
    m_selectMoreCommandWidget->setVisible(false);
    connect(m_selectMoreCommandWidget,&SelectMoreCommandWidget::sigCurrentCommandText,this,&SelectWidget::sendButtonText);
    connect(m_selectMoreCommandWidget,&SelectMoreCommandWidget::sigCloseSelectWidget,this,[=](){
        this->close();
    });
    connect(m_selectMoreCommandWidget, &SelectMoreCommandWidget::sigDeactiveWidget, this, [=](bool flag){
        if (m_clickFlag) {
            m_selectMoreCommandWidget->setVisible(false);
        } else {
            if (flag) {
                m_selectMoreCommandWidget->close();
                this->close();
            }
        }
    });
}

void SelectWidget::initSelectPopupIconWindow()
{
    m_qmlEngine = new QQmlEngine();
    QQmlComponent component(m_qmlEngine, QUrl("qrc:/qml/qml/SelectPopupIconWindow.qml"));

    if (component.isError()) {
        qDebug() << "QML 加载失败：";
        for (const auto& err : component.errors()) {
            qDebug() << err.toString();
        }
        return;
    }

    QObject* qmlObj = component.create();
    m_iconWindow = qobject_cast<QQuickWindow*>(qmlObj);
    if (!m_iconWindow) {
        qDebug() << "错误：QML 根元素不是 Window 类型！";
        delete qmlObj;
        return;
    }
    m_iconWindow->setPosition(QCursor::pos()); // 弹出到鼠标位置（推荐）

    m_iconWindow->setVisible(false); //默认先隐藏不显示
    //信号槽


}

void SelectWidget::slShowToolTip(bool isShow)
{
    if(isShow)
    {
        QPoint tooltipPos = mapToGlobal(QPoint(15, height() + 4));
        QToolTip::showText(tooltipPos,tr("Qi Ling options"),this);

    }
    else
    {
        QToolTip::hideText();

    }
}


void SelectWidget::slSettingButtonClicked()
{
    KTrackData *node = kdk_dia_data_init(KEVENTSOURCE_DESKTOP, KEVENT_CLICK);
    kdk_dia_upload_default(node, "send_scribe_words_content_button_click_event", "scribeWordsPage");
    kdk_dia_data_free(node);

    this->buttonText = "";
    emit signalSendClicpText();
}

void SelectWidget::slShowContextMenu()
{
    m_clickFlag = true;
    //点击图标后更新位置并显示此菜单
    settingRightClickMenu->move(cursor().pos());
    settingRightClickMenu->exec();
}

void SelectWidget::slUpdateSelectEnableStatus(bool isEnabled)
{
    if(isEnabled)
    {
        if(sendAction)
        {
            sendAction->setEnabled(true);
        }
        emit sigUpdateSelectEnableStatus(true);
    }
    else{
        if(sendAction)
        {
            sendAction->setEnabled(false);
        }
        emit sigUpdateSelectEnableStatus(false);
    }
}

QString SelectWidget::getButtonText()
{
    return buttonText;
}

int SelectWidget::getCurrentId()
{
    return promptId;
}

bool SelectWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::ActivationChange) {
        if(QApplication::activeWindow() != this  && QApplication::activeWindow() != m_selectMoreCommandWidget) {
            slDisableSelectFocus();
            this->hide();
            m_selectMoreCommandWidget->hide();
        }
    }

    // 需要区分鼠标点击和鼠标拖拽事件
    if (e->type() == QEvent::MouseMove || QEvent::Move) {
        m_clickFlag = false;
    }
    if (e->type() == QEvent::MouseButtonPress) {
        m_clickFlag = true;
    }

    // 单独再处理一下按键事件
    // 解决场景：更多下拉弹出后，并且拖动悬浮条后，键入esc不响应问题
    if (e->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Escape) {
            close();
        }
    }

    return QWidget::event(e);
}

void SelectWidget::moveEvent(QMoveEvent *event)
{
    //基类调用
    QWidget::moveEvent(event);
    if(m_selectMoreCommandWidget->isVisible() == true)
    {
        // 获取新的位置
        QPoint newPos = event->pos();
        m_selectMoreCommandWidget->move(newPos.x()+m_moreCommandStartPos,newPos.y()+this->height()+10);

    }
}

void SelectWidget::keyPressEvent(QKeyEvent *event)
{
    //按ESC键关闭窗口
    if(event->key() == Qt::Key_Escape)
    {
        close();
    }
    // 确保基类也能处理事件
    QWidget::keyPressEvent(event);
}

//需求:插入文字到光标位置
void SelectWidget::insertText(const QString &text)
{
    QDBusInterface interface("org.ukui.inputmethod",
                             "/commitstring",
                             "org.ukui.inputmethod.commitstring",
                             QDBusConnection::sessionBus());

    interface.call("CommitString", text);
}


void SelectWidget::closeWidgetAndinstructPage(int x, int y)
{
    KyInfo() << "=======closeWidgetAndinstructPage======";
    //在点击非划词区域时隐藏划词界面
    //TODO:Bug界面隐藏后菜单上下文点击会失效
    emit sigDisableMorePage();
    m_selectMoreCommandWidget->setVisible(false);
    this->hide();
}


void SelectWidget::showScribeWordsAtCursorPosition()
{
    KyInfo() << "=======showScribeWordsAtCursorPosition======";
    KTrackData *node = kdk_dia_data_init(KEVENTSOURCE_DESKTOP, KEVENT_CLICK);
    kdk_dia_upload_default(node, "send_scribe_words_waken_event", "scribeWordsPage");
    kdk_dia_data_free(node);

    close();
    showSelectWidgetWithAnimation();

    return;
}

void SelectWidget::initAnimation()
{
    m_animation = new QPropertyAnimation(this, "windowOpacity");
    m_animation->setDuration(3000);
    m_animation->setEasingCurve(QEasingCurve::OutInQuad);
    // 关键：连接动画结束信号，完成最终隐藏+恢复透明度
    connect(m_animation, &QPropertyAnimation::finished, this, [=]() {
        this->hide(); // 动画渐变完成后再隐藏
        this->setWindowOpacity(1.0); // 恢复透明度，避免下次显示异常
        m_selectMoreCommandWidget->hide(); // 同步隐藏更多指令窗口
    });

    return;
}

void SelectWidget::initMenu()
{
    //添加菜单选项
    //菜单
    settingRightClickMenu= new QMenu(this);
    settingRightClickMenu->setAccessibleName("kylin-aiassistant_QMenu_settingRightClickMenu");
    //发送指令
    sendAction = new QAction(tr("Send to Qi Ling"));
    settingRightClickMenu->addAction(sendAction);
    connect(sendAction,&QAction::triggered,this,[&](){
        slSettingButtonClicked();
    },Qt::AutoConnection);
    //主页
    homePageAction = new QAction(tr("Qi Ling HomePage"));
    settingRightClickMenu->addAction(homePageAction);
    connect(homePageAction,&QAction::triggered,this,[&](){
        emit sigHomePageShow();
    },Qt::AutoConnection);

    settingRightClickMenu->addSeparator();

    //关闭划词选项
    disableAction = new QAction(tr("Disable Floating Bar For Scribe Words"));
    settingRightClickMenu->addAction(disableAction);
    connect(disableAction,&QAction::triggered,this,[&](){
        emit sigDisableScribe();
    },Qt::AutoConnection);

    settingRightClickMenu->addSeparator();

    //设置
    settingAction = new QAction(tr("Setting"));
    settingRightClickMenu->addAction(settingAction);
    connect(settingAction,&QAction::triggered,this,[&](){
        emit sigSettingShow();
    },Qt::AutoConnection);
    //关闭
    closeAction = new QAction(tr("Close"));
    connect(closeAction,&QAction::triggered,this,[&](){
        KTrackData *node = kdk_dia_data_init(KEVENTSOURCE_DESKTOP, KEVENT_CLICK);
        kdk_dia_upload_default(node, "send_close_scribe_words_click_event", "scribeWordsPage");
        kdk_dia_data_free(node);
        this->hide();
    },Qt::AutoConnection);
    settingRightClickMenu->addAction(closeAction);
}


void SelectWidget::showSelectWidgetWithAnimation()
{
    //清理并重新生成quickWidget内容
    if(m_quickWidget) {
        m_quickWidget->deleteLater();
        m_quickWidget = nullptr;
    }

    initQmlSelectWidget();

    QQuickItem *item=m_quickWidget->rootObject();
    if (item) {
        QMetaObject::invokeMethod(item, "onUpdateRectSize");

    } else {
        return;
    }

    QPoint cursorPos = QCursor::pos();
#ifdef UKUI_WINDOWHELPER_SUPPORT
    cursorPos.setX(10);
    cursorPos.setY(10);
    m_ukuiWindowHelper->openUnderCursor(cursorPos);
#else
    QRect availableGeometry = QGuiApplication::primaryScreen()->availableGeometry();
    // 调整划词弹出窗口位置，确保屏幕范围内可见，让0 <= x <= availableGeometry.right() - this->width(), 0 <= y <= availableGeometry.bottom() - this->height(),
    int x = qBound(0, cursorPos.x(), availableGeometry.right() - this->width());
    int y = qBound(0, cursorPos.y(), availableGeometry.bottom() - this->height());
    move(x, y);
#endif

    this->show();

    return;
}

void SelectWidget::hideSelectWidgetWithAnimation()
{
    // QML隐藏
    QQuickItem *item = m_quickWidget->rootObject();
    if(item) {
        // 调用 QML 的 startHideAnimation 方法
        QMetaObject::invokeMethod(item, "startHideAnimation");
    }
}

void SelectWidget::onHideAnimationFinished()
{
    this->hide();
    m_selectMoreCommandWidget->hide();
    QQuickItem *item = m_quickWidget->rootObject();
    if(item) {
        QMetaObject::invokeMethod(item, "resetOpacity");
    }
}

void SelectWidget::slEnableSelectFocus()
{
    qDebug() << "设置划词焦点";
#ifdef UKUI_WINDOWHELPER_SUPPORT
    if (m_ukuiWindowHelper) {
        UkuiWindowHelper::States targetStates;
        targetStates |= UkuiWindowHelper::State::KeepAbove;
        targetStates |= UkuiWindowHelper::State::Focusable;
        targetStates |= UkuiWindowHelper::State::Activatable;
        m_ukuiWindowHelper->setWindowState(targetStates);
    }
#endif
    this->activateWindow();
    this->setFocus(Qt::MouseFocusReason);
    m_quickWidget->setFocus();

    return;
}

void SelectWidget::slDisableSelectFocus()
{
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
bool SelectWidget::isIncludeMenuPage(int x, int y)
{
    if(settingRightClickMenu){
        QRect geoMenuRect = settingRightClickMenu->geometry();
        return geoMenuRect.contains(x,y);
    }

    return false;
}

bool SelectWidget::isIncludeMoreCommandWidget(int x, int y)
{
    if(m_selectMoreCommandWidget){
        QRect moreCmdWidget = m_selectMoreCommandWidget->geometry();
        return moreCmdWidget.contains(x,y);
    }

    return false;
}
