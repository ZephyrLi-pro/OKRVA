#include "paragraphmanager.h"

ParagraphManager::ParagraphManager(QObject *parent)
    : QObject(parent)
    , m_loaded(false)
{
}

ParagraphManager::~ParagraphManager()
{
}

bool ParagraphManager::loadParagraphs(const QString &filePath)
{
    // 清空现有数据
    m_paragraphs.clear();
    m_currentRoundQueue.clear();
    m_usedInCurrentRound.clear();
    m_loaded = false;

    // 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件:" << filePath;
        return false;
    }

    // 读取JSON数据
    QByteArray jsonData = file.readAll();
    file.close();

    // 解析JSON
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << parseError.errorString();
        return false;
    }

    if (!jsonDoc.isObject()) {
        qWarning() << "JSON不是对象格式";
        return false;
    }

    // 提取段落
    QJsonObject jsonObj = jsonDoc.object();
    if (!jsonObj.contains("paragraphs") || !jsonObj["paragraphs"].isArray()) {
        qWarning() << "JSON格式错误: 缺少paragraphs数组";
        return false;
    }

    QJsonArray paragraphsArray = jsonObj["paragraphs"].toArray();
    for (const QJsonValue &value : paragraphsArray) {
        if (value.isObject()) {
            QJsonObject paragraphObj = value.toObject();
            if (paragraphObj.contains("content") && paragraphObj["content"].isString()) {
                m_paragraphs.append(paragraphObj["content"].toString());
            }
        }
    }

    if (m_paragraphs.isEmpty()) {
        qWarning() << "未找到任何段落";
        return false;
    }

    m_loaded = true;

    // 初始化第一轮
    initNewRound();

    qDebug() << "成功加载" << m_paragraphs.size() << "个段落，已初始化第一轮";
    return true;
}

void ParagraphManager::initNewRound()
{
    // 清除当前轮的数据
    m_currentRoundQueue.clear();
    m_usedInCurrentRound.clear();

    // 复制所有段落
    QStringList availableParagraphs = m_paragraphs;

    // 随机打乱顺序
    std::shuffle(availableParagraphs.begin(), availableParagraphs.end(),
                *QRandomGenerator::global());

    // 加入队列
    for (const QString &paragraph : availableParagraphs) {
        m_currentRoundQueue.enqueue(paragraph);
    }

    qDebug() << "新一轮初始化完成，本轮包含" << m_currentRoundQueue.size() << "个段落";
}

QString ParagraphManager::getNextParagraph()
{
    if (!m_loaded || m_paragraphs.isEmpty()) {
        return QString("段落数据未加载或为空");
    }

    // 如果当前轮队列为空，初始化新一轮
    if (m_currentRoundQueue.isEmpty()) {
        initNewRound();
    }

    // 从队列中取出下一个段落
    QString nextParagraph = m_currentRoundQueue.dequeue();
    m_usedInCurrentRound.append(nextParagraph);

    // 计算当前轮已使用和剩余的数量
    int usedCount = m_usedInCurrentRound.size();
    int totalCount = m_paragraphs.size();

    qDebug() << "获取段落: 当前轮已使用" << usedCount << "/" << totalCount;

    return nextParagraph;
}

int ParagraphManager::paragraphCount() const
{
    return m_paragraphs.size();
}

QStringList ParagraphManager::getAllParagraphs() const
{
    return m_paragraphs;
}
