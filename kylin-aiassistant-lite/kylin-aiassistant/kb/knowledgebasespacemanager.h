#ifndef KNOWLEDGEBASESPACEMANAGER_H
#define KNOWLEDGEBASESPACEMANAGER_H

#include <QObject>
#include <ukui-log4qt.h>
#include "global/kaglobal.h"

class KnowledgeBaseSpaceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString spaceStatus READ getSpaceStatus NOTIFY spaceStatusChanged)
    Q_PROPERTY(int spaceUsedPercent READ getSpaceUsedPercent NOTIFY spaceUsedPercentChanged)

public:
    static KnowledgeBaseSpaceManager* getInstance();

    Q_INVOKABLE QString getSpaceStatus();
    Q_INVOKABLE int getSpaceUsedPercent();

    // 添加文件时调用，增加已使用空间
    void addSpace(qint64 size);
    
    // 删除文件时调用，减少已使用空间
    void removeSpace(qint64 size);
    
    // 初始化已使用空间（从配置文件读取）
    void initialize();

    qint64 getTotel();
    qint64 getUsed();

signals:
    void spaceStatusChanged();
    void spaceUsedPercentChanged();

private:
    explicit KnowledgeBaseSpaceManager(QObject *parent = nullptr);
    ~KnowledgeBaseSpaceManager() = default;
    
    // 禁止拷贝和赋值
    KnowledgeBaseSpaceManager(const KnowledgeBaseSpaceManager&) = delete;
    KnowledgeBaseSpaceManager& operator=(const KnowledgeBaseSpaceManager&) = delete;

    static KnowledgeBaseSpaceManager* m_instance;
    
    qint64 m_total;    // 总空间容量 B
    qint64 m_used;     // 已用空间, 单位B
    
    void updateUsedSpace();
    int getProgressPercent(qint64 used, qint64 total);
};

#endif // KNOWLEDGEBASESPACEMANAGER_H

