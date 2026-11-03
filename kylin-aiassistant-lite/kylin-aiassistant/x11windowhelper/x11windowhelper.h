#ifndef X11WINDOWHELPER_H
#define X11WINDOWHELPER_H

#include <QObject>
#include <QWindow>
#include <QQuickWindow>
#include <QVariant>
#include <QDebug>

/**
 * @brief X11WindowHelper 类
 * 
 * 用于在 Linux X11 环境下移除窗口标题栏的辅助类
 * 使用原生 X11 API 来控制窗口装饰
 * 
 * 使用方法:
 * 1. 在 C++ 中创建对象并注册到 QML
 * 2. 在 QML 中调用 removeTitleBar(window) 方法
 */
class X11WindowHelper : public QObject
{
    Q_OBJECT

public:
    explicit X11WindowHelper(QObject *parent = nullptr);
    ~X11WindowHelper();

    /**
     * @brief 移除窗口标题栏（装饰）
     * @param window 要操作的 QWindow 或 QQuickWindow 对象
     * @return 是否成功移除标题栏
     */
    Q_INVOKABLE bool removeTitleBar(QObject *window);

    /**
     * @brief 检查当前是否运行在 X11 环境下
     * @return 是否为 X11 环境
     */
    Q_INVOKABLE bool isX11Environment();

    /**
     * @brief 设置窗口置顶
     * @param window 要操作的窗口对象
     * @param above 是否置顶
     * @return 是否设置成功
     */
    Q_INVOKABLE bool setWindowAlwaysOnTop(QObject *window, bool above);

    /**
     * @brief 提升窗口到最前面并激活（获取焦点）
     * @param window 要操作的窗口对象
     * @return 是否操作成功
     */
    Q_INVOKABLE bool raiseAndActivateWindow(QObject *window);

    /**
     * @brief 设置窗口跳过任务栏
     * @param window 要操作的窗口对象
     * @param skipTaskbar 是否跳过任务栏
     * @return 是否设置成功
     */
    Q_INVOKABLE bool setSkipTaskbar(QObject *window, bool skipTaskbar);

signals:
    /**
     * @brief 标题栏移除成功的信号
     */
    void titleBarRemoved();

    /**
     * @brief 操作失败的信号
     * @param errorMessage 错误信息
     */
    void operationFailed(const QString &errorMessage);

private:
    /**
     * @brief 获取 QWindow 的 X11 窗口 ID
     * @param window QWindow 对象
     * @return X11 窗口 ID
     */
    WId getX11WindowId(QWindow *window);

    /**
     * @brief 使用 X11 API 移除窗口装饰
     * @param x11Window X11 窗口 ID
     * @return 是否成功
     */
    bool removeX11WindowDecoration(WId x11Window);

    bool m_isX11;  // 是否为 X11 环境
};

#endif // X11WINDOWHELPER_H
