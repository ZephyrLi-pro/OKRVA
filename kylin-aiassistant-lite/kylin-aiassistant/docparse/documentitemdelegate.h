#ifndef DOCUMENTITEMDELEGATE_H
#define DOCUMENTITEMDELEGATE_H

#include "documentlistview.h"

#include <QObject>
#include <QStyledItemDelegate>

// 前向声明
class DocumentListModel;

class DocumentItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DocumentItemDelegate(DocumentListModel *model, QObject *parent = nullptr);  // 修改构造函数
    ~DocumentItemDelegate();

    /**
     * @brief 返回项的尺寸
     * @param option 样式选项
     * @param index 索引
     * @return QSize 尺寸
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 创建编辑器控件
     * @param parent 父窗口指针
     * @param option 样式选项
     * @param index 索引
     * @return QWidget* 编辑器指针
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 更新控件数据（滚动时复用控件，仅更新数据）
     * @param editor 编辑器控件
     * @param index 索引
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief 更新编辑器几何位置
     * @param editor 编辑器指针
     * @param option 样式选项
     * @param index 索引
     */
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 更新单个列表项
     * @param parent 当前列表视图指针
     * @param index 要更新的模型索引
     */
    void updateItemWidget(DocumentListView *parent, const QModelIndex &index) const;

    /**
     * @brief 设置当前可见范围
     * @param start 起始索引
     * @param end 结束索引
     */
    void setVisibleRange(int start, int end);

signals:
    void sigDeleteBtnClicked(const QString &uuid);

private:
    int m_visibleStart = 0; // 当前可见范围起始索引
    int m_visibleEnd = 9;  // 当前可见范围结束索引
    DocumentListModel *m_model = nullptr;  // 添加模型指针
};

#endif // DOCUMENTITEMDELEGATE_H
