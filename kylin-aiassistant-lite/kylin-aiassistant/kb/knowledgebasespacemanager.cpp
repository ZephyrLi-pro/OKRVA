#include "knowledgebasespacemanager.h"

KnowledgeBaseSpaceManager* KnowledgeBaseSpaceManager::m_instance = nullptr;

KnowledgeBaseSpaceManager::KnowledgeBaseSpaceManager(QObject *parent)
    : QObject(parent)
    , m_total(2U * 1024 * 1024 * 1024)  // 2GB
    , m_used(0)
{
}

KnowledgeBaseSpaceManager* KnowledgeBaseSpaceManager::getInstance()
{
    if (m_instance == nullptr) {
        m_instance = new KnowledgeBaseSpaceManager();
    }
    return m_instance;
}

void KnowledgeBaseSpaceManager::initialize()
{
    /* 这里跟工行版本知识库的区别是，工行版本知识库的已使用的空间容量是从SDK接口获取所有的文档列表，然后累加计算的。
     * V11 版本这里实现为初始从配置文件里读取。添加和删除知识库文件的时候，都需要实时更新配置文件。
     */
    m_used = ConfigManager::getInstance()->getConfigValue("space-used", 0).toLongLong();
    KyInfo() << "KnowledgeBaseSpaceManager initialized, used space: " << m_used << " B";
}

qint64 KnowledgeBaseSpaceManager::getTotel()
{
    return m_total;
}

qint64 KnowledgeBaseSpaceManager::getUsed()
{
    return m_used;
}

QString KnowledgeBaseSpaceManager::getSpaceStatus()
{
    if (m_used == 0) {
        return QString(tr("space capacity ") + "0 GB/2.0 GB");
    }

    return QString(tr("space capacity ") + Globals::formatFileSize(m_used, 2) + "/2.0 GB");
}

int KnowledgeBaseSpaceManager::getSpaceUsedPercent()
{
    int res = getProgressPercent(m_used, m_total);
    KyInfo() << "======当前已使用空间大小 ：" << m_used << " B" << " value:" << res;

    return res;
}

void KnowledgeBaseSpaceManager::addSpace(qint64 size)
{
    if (size <= 0) {
        return;
    }
    
    m_used += size;
    updateUsedSpace();
}

void KnowledgeBaseSpaceManager::removeSpace(qint64 size)
{
    if (size <= 0) {
        return;
    }
    
    m_used -= size;
    if (m_used < 0) {
        m_used = 0;
    }
    updateUsedSpace();
}

void KnowledgeBaseSpaceManager::updateUsedSpace()
{
    KyInfo() << "space-used" << m_used;

    ConfigManager::getInstance()->updateConfig("space-used", m_used);

    Q_EMIT spaceStatusChanged();
    Q_EMIT spaceUsedPercentChanged();
}

int KnowledgeBaseSpaceManager::getProgressPercent(qint64 used, qint64 total)
{
    // 计算百分比
    double percent = (double)used / total * 100.0;

    // 小于1%时按1%计算，否则取整数部分
    if (percent < 1.0 && percent > 0) {
        return 1;
    } else {
        return (int)percent;
    }
}

