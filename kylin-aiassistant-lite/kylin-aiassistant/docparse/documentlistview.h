#ifndef DOCUMENTLISTVIEW_H
#define DOCUMENTLISTVIEW_H

#include <QObject>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollBar>

#include "documentitemwidget.h"
#include "documentlistmodel.h"

#define ITEM_COUNT 13
#define ITEM_HEIGHT 36

class DocumentListView : public QListView
{
    Q_OBJECT
public:
    explicit DocumentListView(QWidget *parent = nullptr);
    ~DocumentListView();

    /**
     * @brief 重定义setModel函数
     * @param model 数据模型指针
     */
    void setModel(QAbstractItemModel *model) override;

    /**
     * @brief 滚动到底部
     */
    void scrollToBottom();

    /**
     * @brief 滚动到顶部
     */
    void scrollToTop();

    /**
     * @brief 刷新可见区域
     */
    void refreshVisibleItems();
    
protected:
    /**
     * @brief 拖拽进入
     * @param event 事件对象
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief 拖拽移动处理
     * @param event 事件对象
     */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /**
     * @brief 拖拽释放
     * @param event 事件对象
     */
    void dropEvent(QDropEvent* event) override;

    /**
     * @brief 窗口大小变化时刷新可见项
     * @param event 事件对象
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief 数据新增时刷新
     * @param parent 模型索引
     * @param start 起始行数
     * @param end 结束行数
     */
    void rowsInserted(const QModelIndex &parent, int start, int end) override;

public slots:

    /**
     * @brief 递归打开编辑控件
     */
    void openAllEditor();

    /**
     * @brief 处理模型数据添加
     * @param parent 模型索引
     * @param first 起始行数
     * @param last 结束行数
     */
    void onRowsInserted(const QModelIndex &parent, int first, int last);

    /**
     * @brief 处理模型数据变化
     * @param topLeft 左上角索引
     * @param bottomRight 右下角索引
     * @param roles 角色列表
     */
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

    /**
     * @brief 滚动时刷新可见项
     * @param value 滚动条位置
     */
    void onScroll(int value);

signals:
    void sigBlankPage();
    void sigFileDropped(const QStringList &filePaths);

private:
    /**
     * @brief 计算第一个可见项的索引
     */
    int firstVisibleIndex() const;

private:
    QAbstractItemModel *m_model;

};
#endif // DOCUMENTLISTVIEW_H
