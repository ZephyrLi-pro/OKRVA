#include "followupmanager.h"
#include <ukui-log4qt.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>

FollowUpManager::FollowUpManager(QObject *parent)
    : QObject(parent)
{
}

FollowUpManager::~FollowUpManager()
{
    destroy();
}

void FollowUpManager::initialize()
{
    if (m_followUpThread != nullptr) {
        KyInfo() << "FollowUpManager already initialized";
        return;
    }

    // 创建追问线程
    m_followUpThread = new DocParseThread(this);
    connect(m_followUpThread, &DocParseThread::receivedMessage,
            this, &FollowUpManager::onRecFollowUpMsg);
    connect(m_followUpThread, &DocParseThread::receivedError,
            this, &FollowUpManager::onFollowUpError);

    // 创建超时定时器
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(TIMEOUT_INTERVAL);
    connect(m_timeoutTimer, &QTimer::timeout, this, &FollowUpManager::onTimeout);

    KyInfo() << "FollowUpManager initialized";
}

void FollowUpManager::buildFollowUpRequest(const QString &answerContent, bool isUserStopped)
{
    // 隐藏追问
    return;

    // 如果用户已经停止回答，不发送追问请求
    if (isUserStopped && m_timeoutTimer && m_timeoutTimer->isActive()) {
        KyInfo() << "User stopped reply, skipping follow up request";
        emit questionsGenerated(QStringList());
        return;
    }

    if (answerContent.isEmpty()) {
        KyInfo() << "No previous AI answer to follow up";
        emit questionsGenerated(QStringList());
        return;
    }

    QJsonObject messageObj;
    messageObj["msgType"] = "sendChatMsg";

    QString promptTemplate =
            "请严格按以下JSON格式解析回答内容并返回，不要返回其他任何内容：\n\n"
            "{\n"
            "  \"questions\": [\"问题1\", \"问题2\", \"问题3\"]\n"
            "}\n\n"
            "要求：\n"
            "1. questions：共生成3个问题，每个问题字数不超过25字\n"
            "2. 问题需与回答内容高度相关，具备实际询问价值\n"
            "3. 返回的问题不要与需要生成相关问题的回答内容一样\n"
            "需要生成相关问题的回答内容如下：\n\n";

    QString inputText = promptTemplate + answerContent;

    QJsonObject msgBody;
    msgBody["inputMsg"] = inputText;
    messageObj["msgBody"] = msgBody;

    QJsonDocument doc(messageObj);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    // 重置状态
    m_followUpRecvMsg.clear();
    m_isFollowUpEnd = false;
    m_followUpTimeout = false;
    m_isProcessing = true;

    // 启动超时定时器
    if (m_timeoutTimer) {
        m_timeoutTimer->start();
        KyInfo() << "Follow up request sent, starting timeout timer";
    }

    // 发送请求
    if (m_followUpThread) {
        m_followUpThread->sendMessage(jsonString);
    }

    return;
}

void FollowUpManager::stopFollowUpRequest()
{
    KyInfo() << "stopFollowUpRequest called";
    
    if (m_timeoutTimer && m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
    
    m_followUpTimeout = true;
    m_isProcessing = false;
    
    if (m_followUpThread) {
        m_followUpThread->stopChat();
    }
    
    emit questionsGenerated(QStringList());
    m_followUpRecvMsg.clear();
    m_isFollowUpEnd = false;
}

void FollowUpManager::destroy()
{
    KyInfo() << "FollowUpManager::destroy() called";
    
    // 停止超时定时器
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
    
    // 停止并销毁追问线程
    if (m_followUpThread) {
        m_followUpThread->stopChat();
        m_followUpThread->quit();
        m_followUpThread->wait();
        m_followUpThread->deleteLater();
        m_followUpThread = nullptr;
    }
    
    // 清理状态
    m_followUpRecvMsg.clear();
    m_isFollowUpEnd = false;
    m_followUpTimeout = false;
    m_isProcessing = false;
    
    KyInfo() << "FollowUpManager destroyed";
}

bool FollowUpManager::isProcessing() const
{
    return m_isProcessing;
}

void FollowUpManager::onRecFollowUpMsg(const QString &message)
{
    if (m_followUpTimeout) {
        KyInfo() << "Follow up already timeout, ignoring callback message";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) {
        KyInfo() << "Failed to parse follow up JSON message:" << message;
        return;
    }

    QJsonObject rootObj = doc.object();
    QString msgType = rootObj["msgType"].toString();
    if (msgType != "recvChatMsg") {
        KyInfo() << "Invalid follow up msg type:" << msgType;
        return;
    }

    QJsonObject msgBody = rootObj["msgBody"].toObject();
    QString recvMsg = msgBody["recvMsg"].toString();
    bool isEnd = msgBody["isEnd"].toBool();

    // 累计追问结果
    m_followUpRecvMsg.append(recvMsg);
    m_isFollowUpEnd = isEnd;

    if (isEnd && m_timeoutTimer && m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
        KyInfo() << "Follow up message ended, stopping timeout timer";
    }

    KyInfo() << "mIsFollowUpEnd:" << m_isFollowUpEnd;
    if (m_isFollowUpEnd) {
        KyInfo() << "Follow up completed, result:" << m_followUpRecvMsg;
        
        QStringList questions = parseQuestions(m_followUpRecvMsg);
        
        KyInfo() << "Parsed questions:" << questions;
        
        m_isProcessing = false;
        emit questionsGenerated(questions);
        
        m_followUpRecvMsg.clear();
        m_isFollowUpEnd = false;
    }
}

void FollowUpManager::onFollowUpError(const QString &error)
{
    KyInfo() << "Follow up thread error:" << error;
    
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    
    m_isProcessing = false;
    emit requestFailed(error);
}

void FollowUpManager::onTimeout()
{
    KyInfo() << "Follow up request timeout, calling stopChat() to stop callback";
    
    m_followUpTimeout = true;
    m_isProcessing = false;
    
    if (m_followUpThread) {
        KyInfo() << "Calling stopChat() on follow up thread";
        m_followUpThread->stopChat();
    }
    
    emit questionsGenerated(QStringList());
    m_followUpRecvMsg.clear();
    m_isFollowUpEnd = false;
}

QStringList FollowUpManager::parseQuestions(const QString &result)
{
    QStringList questions;
    
    // 尝试解析JSON格式
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8(), &parseError);
    
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject rootObj = doc.object();
        if (rootObj.contains("questions") && rootObj["questions"].isArray()) {
            QJsonArray questionsArray = rootObj["questions"].toArray();
            for (int i = 0; i < questionsArray.size() && i < 3; ++i) {
                QString question = questionsArray[i].toString().trimmed();
                if (!question.isEmpty()) {
                    questions.append(question);
                }
            }
            KyInfo() << "Successfully parsed questions from JSON";
            return questions;
        }
    }
    
    // 如果JSON解析失败，尝试从文本中提取JSON（可能包含markdown代码块）
    QString cleanedResult = result.trimmed();
    // 移除可能的markdown代码块标记
    if (cleanedResult.startsWith("```json")) {
        cleanedResult = cleanedResult.mid(7);
    } else if (cleanedResult.startsWith("```")) {
        cleanedResult = cleanedResult.mid(3);
    }
    if (cleanedResult.endsWith("```")) {
        cleanedResult = cleanedResult.left(cleanedResult.length() - 3);
    }
    cleanedResult = cleanedResult.trimmed();
    
    // 再次尝试解析
    doc = QJsonDocument::fromJson(cleanedResult.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject rootObj = doc.object();
        if (rootObj.contains("questions") && rootObj["questions"].isArray()) {
            QJsonArray questionsArray = rootObj["questions"].toArray();
            for (int i = 0; i < questionsArray.size() && i < 3; ++i) {
                QString question = questionsArray[i].toString().trimmed();
                if (!question.isEmpty()) {
                    questions.append(question);
                }
            }
            KyInfo() << "Successfully parsed questions from cleaned JSON";
            return questions;
        }
    }
    
    // 如果JSON解析完全失败，记录错误并返回空列表
    KyInfo() << "Failed to parse questions from JSON, error:" << parseError.errorString();
    KyInfo() << "Raw result:" << result;
    return questions;
}


