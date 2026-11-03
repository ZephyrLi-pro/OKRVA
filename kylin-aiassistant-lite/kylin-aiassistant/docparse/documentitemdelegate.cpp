#include "documentitemdelegate.h"
#include "documentitemwidget.h"
#include "documentlistmodel.h"

DocumentItemDelegate::DocumentItemDelegate(DocumentListModel *model, QObject *parent)  // 修改构造函数
    : QStyledItemDelegate(parent)
    , m_model(model)  // 初始化模型指针
{

}

DocumentItemDelegate::~DocumentItemDelegate()
{

}

QSize DocumentItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    // 此处需为固定size，不然会影响item刷新及三态
    return QSize(348, 36);
}

QWidget *DocumentItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    // 只给可见范围内的项创建控件
    int row = index.row();
    if (row < m_visibleStart || row > m_visibleEnd) {
        return nullptr;
    }

    DocumentItemWidget *editor = new DocumentItemWidget(parent);
    connect(editor, &DocumentItemWidget::sigDeleteBtnClicked, this, &DocumentItemDelegate::sigDeleteBtnClicked);
    return editor;
}

void DocumentItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    DocumentItemWidget *widget = qobject_cast<DocumentItemWidget*>(editor);
    if (widget == nullptr) {
        KyInfo() << "DocumentItemWidget is nullptr";
        return;
    }

    // 使用传入的模型实例，而不是单例
    if (m_model == nullptr) {
        KyInfo() << "model is nullptr";
        return;
    }

    // 获取可见范围内的单个文档
    QList<DocumentItem*> visibleDocs = m_model->getVisibleDocs(index.row(), index.row());
    if (!visibleDocs.isEmpty()) {
        widget->setData(visibleDocs.first());
    }

    return;
}

void DocumentItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
    return;
}

void DocumentItemDelegate::setVisibleRange(int start, int end)
{
    KyInfo() << "setVisibleRange" << start << "->" << end;
    m_visibleStart = start;
    m_visibleEnd = end;
    return;
}

void DocumentItemDelegate::updateItemWidget(DocumentListView *parent, const QModelIndex &index) const
{
    if (parent == nullptr) {
        KyInfo() << "parent is nullptr";
        return;
    }

    if (!index.isValid()) {
        KyInfo() << "invalid index";
        return;
    }

    KyInfo() << "====updateItemWidget====" << index;

    QWidget *editor = parent->indexWidget(index);
    if (!editor) {
        parent->openPersistentEditor(index);
        editor = parent->indexWidget(index);

        if (!editor) {
            KyInfo() << "Failed to create editor for index:" << index;
            return;
        }
    }

    DocumentItemWidget *itemWidget = qobject_cast<DocumentItemWidget*>(editor);
    if (itemWidget) {
        DocumentItem *item = static_cast<DocumentItem*>(index.internalPointer());
        itemWidget->setData(item);
        itemWidget->update();
    }

    return;
}
