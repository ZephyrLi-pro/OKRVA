#ifndef COMMANDINSTRUCTIONMODEL_H
#define COMMANDINSTRUCTIONMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QUuid>

class CommandInstructionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum CommandRole {
        UuidRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        DescriptionRole,
        IsDefaultRole
    };
    Q_ENUM(CommandRole)

    explicit CommandInstructionModel(QObject *parent = nullptr);
    static CommandInstructionModel* getInstance();

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 数据操作
    Q_INVOKABLE void loadFromFile();
    Q_INVOKABLE bool saveToFile();
    Q_INVOKABLE void addCommand(const QString &name, const QString &summary, const QString &description);
    Q_INVOKABLE bool updateCommand(const QString &uuid, const QString &name, const QString &summary, const QString &description);
    Q_INVOKABLE bool removeCommand(const QString &uuid);
    Q_INVOKABLE bool moveCommand(int fromIndex, int toIndex);
    Q_INVOKABLE QVariantMap getCommand(const QString &uuid) const;
    Q_INVOKABLE int count() const;

    // 从旧系统迁移指令（在 SystemChat 初始化完成后调用）
    // 添加迁移标志检查，避免重复迁移
    Q_INVOKABLE void migrateFromOldSystem();

signals:
    void commandUpdated(const QString &uuid, const QString &name, const QString &summary, const QString &description);
    void commandDeleted(const QString &uuid);

private:
    struct CommandItem {
        QString uuid;
        QString name;
        QString summary;
        QString description;
        bool isDefault;
    };

    QList<CommandItem> mCommands;
    QList<CommandItem> mTextSelectCommands;  // 仅供划词使用的指令（不参与 model 行列表）
    static CommandInstructionModel* mInstance;
    QString mDataFilePath;
    bool mMigratedFromOldSystem;  // 标记是否已从旧系统迁移

    void ensureDataDirectoryExists();
    int findIndexByUuid(const QString &uuid) const;
    void initializeDefaultCommands();
    void initializeTextSelectCommands();
    bool isTextSelectUuid(const QString &uuid) const;
    QString generateUuid() const;
    QString localizedDefaultName(const QString &uuid) const;
    QString localizedDefaultSummary(const QString &uuid) const;
};

#endif // COMMANDINSTRUCTIONMODEL_H
