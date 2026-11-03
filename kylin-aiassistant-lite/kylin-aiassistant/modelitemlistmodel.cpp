#include "modelitemlistmodel.h"

ModelItemListModel* ModelItemListModel::m_ptr = nullptr;

ModelItemListModel::ModelItemListModel(QObject *parent)
    : QAbstractListModel{parent}
{

}

ModelItemListModel::~ModelItemListModel()
{
    if(m_ptr != nullptr){
        delete m_ptr;
    }
}

ModelItemListModel *ModelItemListModel::getInstance()
{
    if(m_ptr == nullptr){
        m_ptr = new ModelItemListModel;
    }

    return m_ptr;
}

void ModelItemListModel::setItems(const QList<ModelItem *> &items)
{
    beginResetModel();
    qDeleteAll(m_items); // 删除所有 ModelItem*
    m_items.clear();     // 清空列表

    m_items = items;
    endResetModel();
}

int ModelItemListModel::rowCount(const QModelIndex &) const
{
    return m_items.count();
}

QVariant ModelItemListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    ModelItem* item = m_items[index.row()];
    switch (role) {
    case NameRole:
        return item->m_modelName;
    case DescriptionRole:
        return item->m_description;
    case TypeRole:
        return item->m_type;
    case IconPathRole:
        return item->m_iconPath;
    case IconPathWhiteRole:
        return item->m_icomPathWhite;
    case IsCheckedRole:
        return item->m_isChecked;
    case IsSupportsThinkingRole:
        return item->m_isSupportsThinking;
    case IsSupportsThinkingToggleRole:
        return item->m_isSupportsThinkingToggle;
    case IsSupportsWebSearchRole:
        return item->m_isSupportsWebSearch;
    case IsModelEnabledRole:
        return item->m_isModelEnabled;
    case IsSharedModelRole:
        return item->m_isSharedModel;
    default:
        return QVariant();
    }
}

QVariantMap ModelItemListModel::get(int index) const
{
    QVariantMap map;
    if (index < 0 || index >= m_items.size())
        return map;

    ModelItem* item = m_items[index];
    map["modelName"] = item->m_modelName;
    map["description"] = item->m_description;
    map["modelType"] = item->m_type;
    map["iconPath"] = item->m_iconPath;
    map["iconPathWhite"] = item->m_icomPathWhite;
    map["isChecked"] = item->m_isChecked;
    map["isSupportsThinking"] = item->m_isSupportsThinking;
    map["isSupportsThinkingToggle"] = item->m_isSupportsThinkingToggle;
    map["isSupportsWebSearch"] = item->m_isSupportsWebSearch;
    map["isModelEnabled"] = item->m_isModelEnabled;
    map["isSharedModel"] = item->m_isSharedModel;

    return map;
}

QHash<int, QByteArray> ModelItemListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "modelName";
    roles[DescriptionRole] = "description";
    roles[TypeRole] = "modelType";
    roles[IconPathRole] = "iconPath";
    roles[IconPathWhiteRole] = "iconPathWhite";
    roles[IsCheckedRole] = "isChecked";
    roles[IsSupportsThinkingRole] = "isSupportsThinking";
    roles[IsSupportsThinkingToggleRole] = "isSupportsThinkingToggle";
    roles[IsSupportsWebSearchRole] = "isSupportsWebSearch";
    roles[IsModelEnabledRole] = "isModelEnabled";
    roles[IsSharedModelRole] = "isSharedModel";
    return roles;
}
