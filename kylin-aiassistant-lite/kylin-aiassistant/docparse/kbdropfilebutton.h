#ifndef KBDROPFILEBUTTON_H
#define KBDROPFILEBUTTON_H

#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>

#include <global/kaglobal.h>

class KBDropFileButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KBDropFileButton(QWidget* parent = nullptr);
    ~KBDropFileButton();

protected:
    /**
     * @brief 拖拽进入
     * @param event 事件对象
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief 拖拽释放
     * @param event 事件对象
     */
    void dropEvent(QDropEvent* event) override;

private slots:
    /**
     * @brief 点击选择文件
     */
    void onSelectFile();

signals:
    void sigFileDropped(const QStringList &filePaths);
    void sigSelectFile();

private:
    /**
     * @brief 初始化界面布局
     */
    void initLayout();

    /**
     * @brief 初始化信号槽连接
     */
    void initConnect();

private:
    QLabel *m_pIcon = nullptr;
    QLabel *m_pLabel = nullptr;
    QLabel *m_pTipLabel = nullptr;
};

#endif // KBDROPFILEBUTTON_H
