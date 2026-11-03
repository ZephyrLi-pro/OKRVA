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
#include "commandtemplatelist.h"

#include <QVariantList>
#include <QDebug>

//CommandTemplateList* CommandTemplateList::m_commandTemplateListModel = nullptr;
CommandTemplateList* CommandTemplateList::originalCommandListModel = nullptr;

CommandTemplateList::CommandTemplateList(QObject *parent)
    : QAbstractListModel{parent}
{

}

int CommandTemplateList::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_commandTemplateList.count();
}

QVariant CommandTemplateList::data(const QModelIndex &index, int role) const
{
    if(index.row() < 0 || m_commandTemplateList.size() <= index.row()) {
        return QVariant();
    }

    CommandTemplateItem item = m_commandTemplateList.at(index.row());
    switch (role) {
        case CommandTemplateRoleType::NameRole:
            return item.name;
        case CommandTemplateRoleType::ActorRole:
            return item.actorId;
        case CommandTemplateRoleType::ContentRole:
            return item.content;
        case CommandTemplateRoleType::CommandTypeRole:
            return static_cast<int>(item.commandType);
        case CommandTemplateRoleType::IconRole:
            return item.iconPath;
        case CommandTemplateRoleType::DescRole:
            return item.description;
        case CommandTemplateRoleType::IconHighlightRole:
            return item.iconHighlightPath;
        case CommandTemplateRoleType::IsActiveRole:
            return item.isActive;
        default:
            break;
    }

    return QVariant();
}

QHash<int, QByteArray> CommandTemplateList::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[CommandTemplateRoleType::NameRole]="name";
    roles[CommandTemplateRoleType::ActorRole]="actorId";
    roles[CommandTemplateRoleType::ContentRole]="content";
    roles[CommandTemplateRoleType::CommandTypeRole]="commandType";
    roles[CommandTemplateRoleType::IconRole] = "iconPath";
    roles[CommandTemplateRoleType::DescRole] = "description";
    roles[CommandTemplateRoleType::IconHighlightRole] = "iconHighlightPath";
    roles[CommandTemplateRoleType::IsActiveRole] = "isActive";

    return roles;
}

CommandTemplateList *CommandTemplateList::getInstance()
{
    if(originalCommandListModel == nullptr) {
        originalCommandListModel = new CommandTemplateList;
    }

    return originalCommandListModel;
}

QList<CommandTemplateItem> CommandTemplateList::getCommandTemplateList()
{
    KyInfo() << "m_commandTemplateList" << m_commandTemplateList;

    return m_commandTemplateList;
}

QVariant CommandTemplateList::getData(const int row, int role) const
{

     CommandTemplateItem item = m_commandTemplateList.at(row);
    switch (role) {
        case 0:
            return item.name;
        case 1:
            return item.actorId;
        case 2:
            return row;
        case 3:
            return item.content;
        case 4:
            return static_cast<int>(item.commandType);
        case 5:
            return item.iconPath;
        case 6:
            return item.description;
        case 7:
            return item.iconHighlightPath;
        case 8:
            return item.isActive;
        default:
            break;
    }
}

int CommandTemplateList::getPromptId(QString name)
{
    int promptId = -2; // -2返回为一个无意义的值，为了和promptId默认为-1区分开，防止模型没有整理周报功能去生成周报
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.name == name) {

            promptId = item.actorId;
            break;
        }
    }
    if (promptId == -2) {
        KyInfo() << "获取promptId失败";
    } else {
        KyInfo() << "获取promptId成功" << promptId;
    }
    return promptId;
}

int CommandTemplateList::count()
{
    return m_commandTemplateList.count();
}

QString CommandTemplateList::getPromptIconpath(int id)
{
    QString promptIconPath = "";
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.actorId == id) {
            promptIconPath = item.iconPath;
            break;
        }
    }
    return promptIconPath;
}

QString CommandTemplateList::getPromptIconHighlightpath(int id)
{
    QString promptIconHighlightPath = "";
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.actorId == id) {
            promptIconHighlightPath = item.iconHighlightPath;
            break;
        }
    }

    return promptIconHighlightPath;
}

void CommandTemplateList::initData(std::vector<Prompt> promptList)
{
    // actorId 到图标路径的映射
    QMap<int, QString> iconMap = {
        {1, "qrc:/icon/icon/setting/text-expend.svg"},
        {2, "qrc:/icon/icon/setting/text-polish.svg"},
        {3, "qrc:/icon/icon/setting/translate.svg"},
        {4, "qrc:/icon/icon/setting/system-QA.svg"},
        {5, "qrc:/icon/icon/setting/todo-generate.svg"},
        {6, "qrc:/icon/icon/setting/weekly-report.svg"},
        {7, "qrc:/icon/icon/setting/new-schedule.svg"},
        {8, "qrc:/icon/icon/setting/text-proofread.svg"},
        {9, "qrc:/icon/icon/setting/summary.svg"},
        {13, "qrc:/icon/icon/setting/system-control.svg"}
    };

    QMap<int, QString> iconHighlightMap = {
        {1, "qrc:/icon/icon/setting/text-expend-highlight.svg"},
        {2, "qrc:/icon/icon/setting/text-polish-highlight.svg"},
        {3, "qrc:/icon/icon/setting/translate-highlight.svg"},
        {4, "qrc:/icon/icon/setting/system-QA-highlight.svg"},
        {5, "qrc:/icon/icon/setting/todo-generate-highlight.svg"},
        {6, "qrc:/icon/icon/setting/weekly-report-highlight.svg"},
        {7, "qrc:/icon/icon/setting/new-schedule-highlight.svg"},
        {8, "qrc:/icon/icon/setting/text-proofread-highlight.svg"},
        {9, "qrc:/icon/icon/setting/summary-highlight.svg"},
        {13, "qrc:/icon/icon/setting/system-control-highlight.svg"}
    };

    // actorId 到指令名称的映射（支持国际化）
    QMap<int, QString> nameMap = {
        {1, QObject::tr("Text Expend")},
        {2, QObject::tr("Text Polish")},
        {3, QObject::tr("Translation")},
        {4, QObject::tr("System Q&A")},
        {5, QObject::tr("Todo Generation")},
        {6, QObject::tr("Weekly Report")},
        {7, QObject::tr("New Schedule")},
        {8, QObject::tr("Text Proofreading")},
        {9, QObject::tr("Summary")},
        {13, QObject::tr("System Control")}
    };

    // actorId 到说明文案的映射
    QMap<int, QString> descriptionMap = {
        {1, QObject::tr("Add content to make expression more complete")},
        {2, QObject::tr("Optimize statements for smoother expression")},
        {3, QObject::tr("Quick Chinese English Bi directional Translation")},
        {4, QObject::tr("Answering questions related to the Galaxy Kirin operating system")},
        {5, QObject::tr("Extract tasks, create to-do list")},
        {6, QObject::tr("One click generation of weekly report content summary")},
        {7, QObject::tr("Extract time content information and add schedule")},
        {8, QObject::tr("Check for syntax errors and correct them")},
        {9, QObject::tr("Extract key points, quickly generate summaries")},
        {13, QObject::tr("Manipulate system functions through statements")}
    };

    m_commandTemplateList.clear();


    beginResetModel();

    for (const auto& prompt : promptList) {
        CommandTemplateItem item;
        item.actorId = prompt.id;
        item.isActive = prompt.active;
        //优先使用映射的翻译名称，如果没有映射则使用原始名称
        if (nameMap.contains(item.actorId)) {
            item.name = nameMap.value(item.actorId);
        } else {
            item.name = QString::fromStdString(prompt.name);
        }

        item.content = QString::fromStdString(prompt.content);
        item.commandType = prompt.type;

        if (iconMap.contains(item.actorId)) {
            item.iconPath = iconMap.value(item.actorId);
        } else {
            item.iconPath = iconMap.value(item.actorId, "qrc:/icon/icon/setting/prompt.svg");
        }

        if (iconHighlightMap.contains(item.actorId)) {
            item.iconHighlightPath = iconHighlightMap.value(item.actorId);
        } else {
            item.iconHighlightPath = iconHighlightMap.value(item.actorId, "qrc:/icon/icon/setting/prompt-highlight.svg");
        }

        if (descriptionMap.contains(item.actorId)) {
            item.description = descriptionMap.value(item.actorId);
        } else {
            item.description = QString::fromStdString(prompt.content);
        }

        m_commandTemplateList << item;
        qInfo() << "resetModel: " << m_commandTemplateList.count();
    }
    endResetModel();

}

void CommandTemplateList::loadData(QList<CommandTemplateItem> commandTemplateList)
{
    // 清空现有数据
    beginResetModel();
    m_commandTemplateList.clear();
    endResetModel();

    for (const auto& item : commandTemplateList) {
        beginInsertRows(QModelIndex(), m_commandTemplateList.count(), m_commandTemplateList.count());
        m_commandTemplateList.append(item);
        endInsertRows();
    }
}

void CommandTemplateList::resetAllData()
{
    QList<CommandTemplateItem> list = m_commandTemplateList;
    beginResetModel();
    m_commandTemplateList.clear();
    m_commandTemplateList = list;
    endResetModel();

}

void CommandTemplateList::createCommand(const QString &name, const QString &content)
{
    emit sigCreateCommand(name, content);
}

void CommandTemplateList::updateCommand(int actorId, const QString &name, const QString &content)
{
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.actorId == actorId) {

            emit sigUpdateCommand(actorId, name, content);
        }
    }
}

void CommandTemplateList::deleteCommand(int actorId, const QString &name)
{
    KyInfo() << "deletecommand";
    emit sigDeleteActiveCommand(actorId, name);
}

void CommandTemplateList::moveCommand(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_commandTemplateList.count() ||
        toIndex < 0 || toIndex >= m_commandTemplateList.count() || fromIndex == toIndex) {
        return;
    }

    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    m_commandTemplateList.move(fromIndex, toIndex);
    endMoveRows();

    emit dataChanged(index(0, 0), index(m_commandTemplateList.count() - 1, 0)); // 更新整个列表

}

void CommandTemplateList::addData(PromptContentAccessType commandType, int actorId, const QString &name, const QString &content)
{
    CommandTemplateItem item;
    item.commandType = commandType;
    item.actorId = actorId;
    item.name = name;
    item.content = content;
    item.iconPath = "qrc:/icon/icon/setting/prompt.svg";
    item.description = content;
    item.iconHighlightPath = "qrc:/icon/icon/setting/prompt-highlight.svg";
    item.isActive = true;

    beginInsertRows(QModelIndex(), m_commandTemplateList.count(), m_commandTemplateList.count());
    m_commandTemplateList << item;
    endInsertRows();

    resetAllData();
    // emit sigUpdateUI();
}

void CommandTemplateList::updateData(int actorId, const QString &name, const QString &content)
{
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.actorId == actorId) {

            item.name = name;
            item.content = content;
            item.description = content;
            item.isActive = true;
            
            QModelIndex modelIndex = this->index(i);
            emit dataChanged(modelIndex, modelIndex);

            // emit sigUpdateUI();
            emit sigUpdateActiveCommand(actorId, name);

            KyInfo() << "提示词编辑完成" << actorId << name;
            break;
        }
    }
}

void CommandTemplateList::removeData(int actorId, const QString &name)
{
    for (int i = 0; i < m_commandTemplateList.size(); ++i) {
        CommandTemplateItem &item = m_commandTemplateList[i];
        if (item.actorId == actorId && item.name == name) {

            beginRemoveRows(QModelIndex(), i, i);
            m_commandTemplateList.removeAt(i);
            endRemoveRows();

            // emit sigUpdateUI();
            KyInfo() << "提示词删除完成" << actorId << name;
            break;
        }
    }
}

// operator<< 格式化输出
QDebug operator<<(QDebug debug, const CommandTemplateItem &item)
{
    QDebugStateSaver saver(debug); // 保存 debug 的状态
    debug.nospace() << "CommandTemplateItem: "
                    << ", commandType: " << static_cast<int>(item.commandType)
                    << ", actorId: " << item.actorId
                    << ", name: " << item.name
                    << ", content: " << item.content
                    << ", iconPath: " << item.iconPath
                    << ", isActive: " << item.isActive
                    << ", iconHighlightPath: " << item.iconHighlightPath << ")";
    return debug;
}

void CommandTemplateList::updateAllData()
{
    beginResetModel();
    endResetModel();
    return;
}
