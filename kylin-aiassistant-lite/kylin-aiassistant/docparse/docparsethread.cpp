#include "docparsethread.h"
#include <ukui-log4qt.h>
#include <QEventLoop>
#include <QDebug>
#include "../global/kaglobal.h"

// 处理GLib和Qt宏冲突
#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

DocParseThread::DocParseThread(QObject *parent)  : QThread{parent}
{
    start();
}

DocParseThread::~DocParseThread()
{
    if (mDocParseSession != nullptr) {
        genai_text_stop_chat(mDocParseSession);
        genai_text_destroy_session(&mDocParseSession);
    }

    if (mDocParseConfig != nullptr) {
        chat_model_config_destroy(&mDocParseConfig);
    }
}

void DocParseThread::setModel(const QString &modelName)
{
    if (mDocParseSession == nullptr) {
        return;
    }

    if (mDocParseConfig == nullptr) {
        mDocParseConfig = createDocParseConfig();
    }

    if (mDocParseConfig == nullptr) {
        return;
    }

    // 设置模型名称
    chat_model_config_set_name(mDocParseConfig, modelName.toUtf8().constData());

    // 应用到会话
    genai_text_set_model_config(mDocParseSession, mDocParseConfig);

    KyInfo() << "DocParseThread-Model set to:" << modelName;
    return;
}

void DocParseThread::run()
{
    QEventLoop loop;

    initDocParseSession();

    loop.exec();

    return;
}

// AI回调函数
void docParseSessionCallback(ChatResult *result, void *userData) {
    KyInfo() << "docParseSessionCallback";
    int errorCode = chat_result_get_error_code(result);
    if(errorCode != 0)
    {
        QString errorStr = Globals::getErrorMsg(errorCode);
        DocParseThread *docParseThread = static_cast<DocParseThread*>(userData);
        if (docParseThread != nullptr)
        {
            Q_EMIT docParseThread->receivedError(errorStr);
        }
        return;
    }

    const char *message = chat_result_get_assistant_message(result);
    if (message == NULL) {
        KyInfo() << "Cannot get chat message!";
        return;
    }

    KyInfo() << "Chat message: " << QString::fromUtf8(message);

    QJsonObject msg;
    msg["msgType"] = "recvChatMsg";

    QJsonObject msgBody;
    bool isEnd = chat_result_get_is_end(result);
    msgBody["isEnd"] = isEnd;
    msgBody["recvMsg"] = message;

    const char *reasonMessage = chat_result_get_reasoning_message(result);
    if ((reasonMessage != NULL) && (reasonMessage[0] != '\0')) {
        //KyInfo() << "reasonMessage message: " << QString::fromUtf8(reasonMessage);
        msgBody["reasonMessage"] = reasonMessage;
    }

    msg["msgBody"] = msgBody;

    // 发射信号，如果需要传递到UI层
    DocParseThread *docParseThread = static_cast<DocParseThread*>(userData);
    if (docParseThread != nullptr)
    {
        Q_EMIT docParseThread->receivedMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));
    }

    return;
}

ChatModelConfig *DocParseThread::createDocParseConfig()
{
    ChatModelConfig *config = chat_model_config_create();
    if (config == NULL) {
        emit receivedError("Failed to create model config!");
        return NULL;
    }

    chat_model_config_set_name(config, "百度-ERNIE-Bot-4");
    //chat_model_config_set_top_k(config, 0.5);
    // 设置非流式会话
    //chat_model_config_set_stream(config, false);

    return config;
}

void DocParseThread::initDocParseSession()
{
    mDocParseConfig = createDocParseConfig();
    if (mDocParseConfig == NULL) {
        KyInfo() << "Failed to create doc parse session config!";
        return;
    }

    mDocParseSession = genai_text_create_session();
    if (mDocParseSession == NULL) {
        KyInfo() << "Failed to create doc parse session!";
        return;
    }

    genai_text_set_model_config(mDocParseSession, NULL);

    int errorCode = genai_text_init_session(mDocParseSession);
    if (errorCode != 0) { // AISDK_NO_ERROR 应该是0，根据你的实际错误码定义调整
        KyInfo() << "Init doc parse session error, error code:" << errorCode;
        return;
    }

    genai_text_result_set_callback(mDocParseSession, docParseSessionCallback, this);

    setModel(ConfigManager::getInstance()->getConfigValue("cur-nlp-model").toString());

    KyInfo() << "Doc parse session initialized successfully!";
    return;
}

void DocParseThread::sendMessage(const QString message)
{
    QJsonObject data = QJsonDocument::fromJson(message.toUtf8()).object();

    if (!data.contains("msgType")) {
        KyInfo() << "Not carried msgType!";
        return;
    }

    if (!data["msgType"].isString()) {
        KyInfo() << "The type is not a string!";
        return;
    }

    QString msgType = data["msgType"].toString();
    if (msgType == "sendChatMsg") {
        sendChatMsg(data);
        return;
    }

    if (msgType == "sendStopMsg") {
        sendStopMsg(data);
        return;
    }

    KyInfo() << "Unknown message type:" << msgType;
    return;
}

void DocParseThread::sendChatMsg(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("inputMsg") || !msgBody["inputMsg"].isString()) {
        KyInfo() << "Not carried inputMsg!";
        return;
    }

    QString inputMsg = msgBody["inputMsg"].toString();
    if (mDocParseSession == NULL) {
        KyInfo() << "Doc parse session not initialized!";
        return;
    }

    KyInfo() << "Doc parse inputMsg : " << inputMsg;

    // 直接发送用户输入的消息，不添加额外的格式
    genai_text_chat_async(mDocParseSession, inputMsg.toUtf8().constData());

    return;
}

void DocParseThread::stopChat()
{
    KyInfo() << "DocParseThread::stopChat called";
    
    if (mDocParseSession == NULL) {
        KyInfo() << "Doc parse session not initialized!";
        return;
    }

    genai_text_stop_chat(mDocParseSession);
    KyInfo() << "genai_text_stop_chat called for follow up session";
    
    return;
}

void DocParseThread::sendStopMsg(const QJsonObject &data)
{
    KyInfo() << "sendStopMsg";
    stopChat();
    return;
}
