#ifndef SELECTCONTROL_H
#define SELECTCONTROL_H

#include <QObject>
#include <QQuickView>
#include <QTimer>
#include <QClipboard>

#include "textselect/selection.h"
#include "textselect/clipboard.h"
#include "global/kaglobal.h"
class Clipboard;
class QAction;
class QQmlApplicationEngine;

class SelectControl : public QObject
{
    Q_OBJECT
public:
    static SelectControl* getInstance(QObject *parent = nullptr);

    bool getSelectWindowVisible() const { return m_selectWindowVisible; }

    void setSelectWindowVisible(bool visible);

    ~SelectControl();
    /**
     * @brief 初始化划词控制模块
     */
    void initSelectControl();

    /**
     * @brief 初始化划词信号槽
     */
    void initSelectSlots();

    /**
     * @brief 显示划词图标界面
     */
    void onShowScribeWindow();

    /**
     * @brief 初始化信号槽连接
     */
    void initConnect();
private:
    explicit SelectControl(QObject *parent = nullptr);
    static SelectControl* m_instance;        // 单例实例

public slots:
    /**
     * @brief 启动发送划词信息
     */
    void onStartSendSelectMsg();

    /**
     * @brief 自动显示划词窗口
     */
    void autoShowSelectWindow(); //自动显示划词控件

    /**
     * @brief 响应划词内容变更信号
     */
    void handleSelectionTextChanged();

    /**
     * @brief 获取当前划词内容
     * @return
     */
    QString getCurSelectMsg();

public:
    ///////////////////////////////划词菜单功能////////////////////

    /**
     * @brief 显示主界面
     */
    void showMainView();

    /**
     * @brief 关闭划词功能
     */
    Q_INVOKABLE void disableScribe();

    /**
     * @brief 通过快捷键等手段主动唤起全局划词悬浮条（与选区变更自动唤起无关）
     */
    Q_INVOKABLE void invokeGlobalScribeFloatingBar();

    /**
     * @brief 按当前配置刷新「快捷键唤起」所用全局快捷键 Meta+Alt+C（KDED/KGlobalAccel）
     */
    void refreshGlobalScribeShortcut();

    /**
     * @brief 显示设置页面
     */
    Q_INVOKABLE void showSettingPage();

    /**
     * @brief 判断当前是否在划词应用内
     * @return
     */
    bool isCursorInSelfWindow();

    /**
     * @brief 获取当前全局鼠标位置
     * @return QPoint 包含 x 和 y 坐标的点
     */
    Q_INVOKABLE QPoint getGlobalCursorPosition();


    /////////////////////////////焦点问题//////////////////////////
public:
    ///
    /// \brief 开启窗口焦点
    ///
    Q_INVOKABLE void enableWindowFocus();

    /**
     * @brief 重置窗口焦点
     */
    Q_INVOKABLE void restoreWindowNoFocus();

    /**
     * @brief 关闭所有活动的非模态弹窗
     */
    Q_INVOKABLE void closeAllPopupWindows();

signals:

    /**
     * @brief 关闭朗读和划词窗
     */
    void sigCloseReadingAndSelectWindow();

public slots:
    /**
     * @brief 监听划词界面激活变化
     */
    void onSelectWindowActiveChanged();


private:
    QWindow* m_selectWindow = nullptr; // ✅ 新增：保存弹窗窗口指针
    Clipboard *m_selectclip = nullptr;
    QTimer *selectTimer = nullptr;
    QTimer* m_clipboardTimer = nullptr; // 划词定时器
    QString m_initClipText; // 初始化划词内容
    QString m_lastValidText; // 缓存上一次文本，避免重复执行
    bool m_isFirstValidSelect = false;  // 标记是否是用户首次有效选中
    bool m_selectWindowVisible = false;      // 划词窗口显隐状态缓存
    QAction *m_scribeShortcutInvokeAction = nullptr; // 「快捷键唤起」全局快捷键 QAction

private:
#ifdef UKUI_WINDOWHELPER_SUPPORT
    UkuiWindowHelper *m_ukuiWindowHelper = nullptr;
#endif
};

#endif // SELECTCONTROL_H
