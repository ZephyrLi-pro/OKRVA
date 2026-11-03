#include "documentlistview.h"
#include "documentitemdelegate.h"

#include <QDebug>
#include <QScrollBar>

DocumentListView::DocumentListView(QWidget *parent)
    : QListView(parent)
{
    //    setAcceptDrops(true);          // 接受拖放
    //    setDragEnabled(true);          // 启用拖动
    //    setDragDropMode(QAbstractItemView::DropOnly); // 设置拖拽模式

    setAcceptDrops(false);
    setDragEnabled(false);
    setDragDropMode(QAbstractItemView::NoDragDrop);

    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    verticalScrollBar()->setProperty("drawScrollBarGroove", false);
    setEditTriggers(QAbstractItemView::EditTrigger::AllEditTriggers);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::NoSelection);
    setSpacing(0);
    setObjectName("listview");
    setStyleSheet("#listview {"
                  "background-color: rgba(0, 0, 0, 0.03);"
                  "border: 1px solid rgba(0, 0, 0, 0.1);"
                  "border-radius: 6px;"
                  "padding: 8px 0px 8px 8px;"
                  "}"
                  "#listview::item:hover {"
                  "background: transparent; "
                  "}");

    // 固定项高度（确保计算可见范围准确）
    setUniformItemSizes(true);
    // 监听滚动事件
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DocumentListView::onScroll);
}

DocumentListView::~DocumentListView()
{

}

void DocumentListView::setModel(QAbstractItemModel *model)
{
    m_model = model;
    QListView::setModel(m_model);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DocumentListView::openAllEditor);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &DocumentListView::onRowsInserted);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &DocumentListView::onDataChanged);
    openAllEditor();

    return;
}

void DocumentListView::openAllEditor()
{
    if (m_model == nullptr)
        return;

    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex index = m_model->index(row, 0);
        openPersistentEditor(index);
    }

    update();
    refreshVisibleItems();
    return;
}


void DocumentListView::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    if (m_model == nullptr) return;
    // 仅遍历新增的行
    for (int row = first; row <= last; ++row) {
        QModelIndex index = m_model->index(row, 0);
        if (!index.isValid()) continue;
        // 仅当编辑器未创建时才调用（避免重复）
        if (!isPersistentEditorOpen(index)) {
            openPersistentEditor(index);
        }
    }
    update();
}

void DocumentListView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles);
    auto delegate = qobject_cast<DocumentItemDelegate*>(itemDelegate());
    if (!delegate)
        return;

    // 遍历所有发生变化的行，更新对应itemwidget
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        QModelIndex index = m_model->index(row, 0);
        if (index.isValid()) {
            delegate->updateItemWidget(this, index);
        }
    }

    return;
}

int DocumentListView::firstVisibleIndex() const
{
    if (model()->rowCount() == 0) return 0;
    // 根据滚动条位置计算
    return verticalScrollBar()->value() / ITEM_HEIGHT;
}

void DocumentListView::onScroll(int value)
{
    Q_UNUSED(value);
    refreshVisibleItems();
    return;
}

void DocumentListView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    refreshVisibleItems();
    return;
}

void DocumentListView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QListView::rowsInserted(parent, start, end);
    refreshVisibleItems();
    return;
}

// 拖拽进入
void DocumentListView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) { // 是否为文件
        event->acceptProposedAction();
    } else {
        event->ignore();
    }

    return;
}

// 拖拽移动
void DocumentListView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

// 拖拽释放
void DocumentListView::dropEvent(QDropEvent* event)
{
    event->ignore();

    // 是否为文件
    if (!event->mimeData()->hasUrls()) {
        return;
    }

    QStringList filePaths;
    const QList<QUrl> urls = event->mimeData()->urls();
    KyInfo() << "------dropEvent------" << urls;
    if (urls.isEmpty()) {
        return;
    }

    // 遍历所有拖拽的文件URL
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            filePaths.append(url.toLocalFile());
        } else {
            KyInfo() << "Invalid file path!";
        }
    }

    if (!filePaths.isEmpty()) {
        KyInfo() << "Dropped file:" << filePaths;
        emit sigFileDropped(filePaths); // 发射多文件信号
        event->acceptProposedAction();
    }

    return;
}

void DocumentListView::scrollToBottom()
{
    if (!model() || model()->rowCount() == 0) {
        return; // 无数据时不滚动
    }

    if (verticalScrollBar() && verticalScrollBar()->maximum() > 0) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }

    return;
}

void DocumentListView::scrollToTop()
{
    if (!model() || model()->rowCount() == 0) {
        return; // 无数据时不滚动
    }

    if (verticalScrollBar() && verticalScrollBar()->maximum() > 0) {
        verticalScrollBar()->setValue(0);
    }

    return;
}

void DocumentListView::refreshVisibleItems()
{
    int firstVisible = firstVisibleIndex(); // 第一个可见项索引
    int lastVisible = firstVisible + ITEM_COUNT - 1;

    // 告诉委托当前可见范围
    auto *delegate = qobject_cast<DocumentItemDelegate*>(itemDelegate());
    if (delegate) {
        delegate->setVisibleRange(firstVisible, lastVisible);
        for (int row = firstVisible; row <= lastVisible; ++row) {
            QModelIndex index = m_model->index(row, 0);
            if (!index.isValid()) {
                continue;
            }

            openPersistentEditor(index);
        }
    }

    // 刷新可见区域
    viewport()->update();
    return;
}
