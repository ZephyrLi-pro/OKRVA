#ifndef FILEDIALOGHELPER_H
#define FILEDIALOGHELPER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;

/**
 * @brief FileDialogHelper
 * 统一封装文件选择/保存对话框的接口，提供给 C++ 与 QML 调用。
 */
class FileDialogHelper : public QObject
{
    Q_OBJECT
public:
    explicit FileDialogHelper(QObject *parent = nullptr);

    /**
     * @brief 保存图片（从 base64 数据）
     * @param base64Image base64 编码的图片数据（可包含 data:image/png;base64, 前缀）
     * @return 保存成功返回 true，取消或失败返回 false
     */
    Q_INVOKABLE bool saveImage(const QString &base64Image);

    // ---- 静态 C++ 帮助接口（可在各个模块直接调用） ----
    static QString openSupportedFile(int typeNum, QWidget *parent);
    static bool saveImage(const QString &base64Image, QWidget *parent);

    /** 将对话框置于鼠标所在屏幕工作区几何中心（避免侧边栏模式下相对窄主窗口居中） */
    static void centerDialogOnDesktop(QWidget *dialog);

private:
    static void setupRemovableSidebar(class QFileDialog &dialog);
};

#endif // FILEDIALOGHELPER_H


