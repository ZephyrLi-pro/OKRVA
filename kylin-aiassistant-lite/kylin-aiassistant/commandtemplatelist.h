#ifndef COMMANDTEMPLATELIST_H
#define COMMANDTEMPLATELIST_H

#include <QAbstractListModel>
#include <QObject>

#include <kylin-ai/private/osassistant/osassistant.h>
#include <ukui-log4qt.h>

#include "global/kaglobal.h"

using namespace kyai::assistant;

// sdk提示词类型
//enum PromptContentAccessType {
//    ReadOnly = 1,   // 预设的提示词——文本扩写、文本润色、中英互译、内容校对、总结概括，仅支持查看不可修改内容
//    ReadWrite,      // 自定义提示词
//    NoAccess        // 预设提示词——系统助手、生成代办、整理周报、新建日程，不可进行提示词查看
//};

typedef struct {
//    int index;
    int actorId;
    QString name;
    QString content;
    PromptContentAccessType commandType;
    QString iconPath;
    QString description;
    QString iconHighlightPath;
    bool isActive;
} CommandTemplateItem;

class CommandTemplateList : public QAbstractListModel
{
    Q_OBJECT
public:

    enum CommandTemplateRoleType {
        NameRole = Qt::UserRole+1,
        ActorRole,
        ContentRole,
        CommandTypeRole,
        IconRole,
        DescRole,
        IconHighlightRole,
        IsActiveRole
    };

    Q_ENUM(CommandTemplateRoleType)

    explicit CommandTemplateList(QObject *parent = nullptr);
    static CommandTemplateList *getInstance();
    static CommandTemplateList *originalCommandListModel;


    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    QList<CommandTemplateItem> getCommandTemplateList();
    Q_INVOKABLE QVariant getData(const int row,int role) const;
    int getPromptId(QString name);
    Q_INVOKABLE int count();
    QString getPromptIconpath(int id);
    Q_INVOKABLE QString getPromptIconHighlightpath(int id);

    void createCommand(const QString &name, const QString &content);
    void updateCommand(int actorId, const QString &name, const QString &content);
    Q_INVOKABLE void moveCommand(const int fromIndex,const int toIndex);

    void initData(std::vector<Prompt> promptList);
    void loadData(QList<CommandTemplateItem> commandTemplateList);
    void resetAllData();
    void addData(PromptContentAccessType commandType, int actorId, const QString &name, const QString &content);
    void updateData(int actorId, const QString &name, const QString &content);
    void updateAllData();

public slots:
    void deleteCommand(int actorId, const QString &name);
    void removeData(int actorId, const QString &name);

signals:
    void sigCreateCommand(QString name, QString content);
    void sigUpdateCommand(int actorId, QString name, QString content);
    void sigUpdateUI();

    // 更新qml主页选中提示词
    void sigUpdateActiveCommand(int actorId, QString name);
    void sigDeleteActiveCommand(int actorId, QString name);

private:
//    static CommandTemplateList *m_commandTemplateListModel;
    QList<CommandTemplateItem> m_commandTemplateList;
};

#endif // COMMANDTEMPLATELIST_H
