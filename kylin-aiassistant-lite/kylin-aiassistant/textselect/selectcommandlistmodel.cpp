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
#include "selectcommandlistmodel.h"
#include <ukui-log4qt.h>

SelectCommandListModel* SelectCommandListModel::selectCommandListModelInstance = nullptr;

SelectCommandListModel::SelectCommandListModel(QObject *parent)
    : QAbstractListModel{parent}
{
    initDefaultCommands();
}

SelectCommandListModel *SelectCommandListModel::getInstance()
{
    if (selectCommandListModelInstance == nullptr) {
        selectCommandListModelInstance = new SelectCommandListModel;
    }
    return selectCommandListModelInstance;
}

void SelectCommandListModel::initDefaultCommands()
{
    beginResetModel();
    m_commandList.clear();

    // Initialize 5 built-in commands
    // Summarize (501)
    SelectCommandItem summaryItem;
    summaryItem.m_actorId = 501;
    summaryItem.m_name = QObject::tr("Summarize");
    summaryItem.m_iconPath = "qrc:/icon/icon/setting/summary.svg";
    summaryItem.m_description = QObject::tr("Summarize key information and generate concise summary");
    summaryItem.m_isActive = true;
    m_commandList.append(summaryItem);

    // Explain (502)
    SelectCommandItem explainItem;
    explainItem.m_actorId = 502;
    explainItem.m_name = QObject::tr("Explain");
    explainItem.m_iconPath = "qrc:/icon/icon/setting/system-QA.svg";  // Temporarily use system Q&A icon
    explainItem.m_description = QObject::tr("Parse complex content and output easy-to-understand explanation");
    explainItem.m_isActive = true;
    m_commandList.append(explainItem);

    // Translate (503)
    SelectCommandItem translateItem;
    translateItem.m_actorId = 503;
    translateItem.m_name = QObject::tr("Translate");
    translateItem.m_iconPath = "qrc:/icon/icon/setting/translate.svg";
    translateItem.m_description = QObject::tr("Quickly translate languages while maintaining semantic accuracy");
    translateItem.m_isActive = true;
    m_commandList.append(translateItem);

    // Read (504)
    SelectCommandItem readAloudItem;
    readAloudItem.m_actorId = 504;
    readAloudItem.m_name = QObject::tr("Read");
    readAloudItem.m_iconPath = "qrc:/html/html/img/chathome/read.svg";
    readAloudItem.m_description = QObject::tr("Convert selected text to natural language playback");
    readAloudItem.m_isActive = true;
    m_commandList.append(readAloudItem);

    // Ask (505)
    SelectCommandItem askItem;
    askItem.m_actorId = 505;
    askItem.m_name = QObject::tr("Ask");
    askItem.m_iconPath = "qrc:/icon/icon/setting/system-QA.svg";  // Use system Q&A icon
    askItem.m_description = QObject::tr("Quickly get relevant answers based on selected text");
    askItem.m_isActive = true;
    m_commandList.append(askItem);

    endResetModel();
}

int SelectCommandListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_commandList.count();
}

QVariant SelectCommandListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_commandList.size()) {
        return QVariant();
    }

    SelectCommandItem item = m_commandList.at(index.row());
    switch (role) {
        case SelectCommandRoleType::NameRole:
            return item.m_name;
        case SelectCommandRoleType::ActorRole:
            return item.m_actorId;
        case SelectCommandRoleType::IconRole:
            return item.m_iconPath;
        case SelectCommandRoleType::DescRole:
            return item.m_description;
        case SelectCommandRoleType::IsActiveRole:
            return item.m_isActive;
        default:
            break;
    }

    return QVariant();
}

QHash<int, QByteArray> SelectCommandListModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[SelectCommandRoleType::NameRole] = "name";
    roles[SelectCommandRoleType::ActorRole] = "actorId";
    roles[SelectCommandRoleType::IconRole] = "iconPath";
    roles[SelectCommandRoleType::DescRole] = "description";
    roles[SelectCommandRoleType::IsActiveRole] = "isActive";

    return roles;
}

QVariant SelectCommandListModel::getData(const int row, int role) const
{
    if (row < 0 || row >= m_commandList.size()) {
        return QVariant();
    }

    SelectCommandItem item = m_commandList.at(row);
    switch (role) {
        case 0:  // NameRole
            return item.m_name;
        case 1:  // ActorRole
            return item.m_actorId;
        case 2:  // IconRole
            return item.m_iconPath;
        case 3:  // DescRole
            return item.m_description;
        case 4:  // IsActiveRole
            return item.m_isActive;
        default:
            break;
    }

    return QVariant();
}

int SelectCommandListModel::count() const
{
    return m_commandList.count();
}
