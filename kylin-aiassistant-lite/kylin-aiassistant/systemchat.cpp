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
#include "systemchat.h"


#include <list>
#include <algorithm>
#include <iostream>
#include <cmark.h>

bool is_reply_end = true;
static QString imageUrl = "";

// Markdown转HTML函数
static QString markdownToHtml(const QString &markdown)
{
    if (markdown.isEmpty()) {
        return QString();
    }
    QByteArray markdownBytes = markdown.toUtf8();
    char *html = cmark_markdown_to_html(markdownBytes.constData(), markdownBytes.length(), CMARK_OPT_DEFAULT);
    if (!html) {
        return QString();
    }
    QString result = QString::fromUtf8(html);
    // cmark_markdown_to_html返回的内存不需要手动释放（内部使用malloc）
    return result;
}

static SystemChat *g_nlpChat = nullptr;

//应答消息格式
//{
//    "message_type": "chat",
//    "content": [
//        [
//            {"type": "text", "text": { "sentence_id": 1, "is_end": true, "result": "" }},
//            {"type": "file_data", "file_data": "image/png;base64,base64_image"}
//        ]
//    ],
//    "error_code": 1,
//    "error_message": "error message",
//    "model":"AI助手"
//}

//回调函数处理chat返回的消息
static void chatCallback(const std::string text_data)
{

    KyInfo() << "======chatCallback时间=====:";


    SystemChat *systemChat = SystemChat::getPtr();

    // 超时处理已移至前端 QML 端

    const char *result = text_data.data();


    //处理JSON数据
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(result, &jsonError);
    if(jsonError.error != QJsonParseError::NoError)
    {
        KyInfo() << "JSON parsing error: " << jsonError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    int errorCode = obj.value("error_code").toInt();
    QString errorMsg = obj.value("error_message").toString();
    QString messageType = obj.value("message_type").toString();
    QString model = obj.value("model").toString();
    QString feedbackURL = "";
    if(obj.contains("feedback_url")) {
        feedbackURL = obj.value("feedback_url").toString();
    }

    if(model == ""){
        KyInfo() << "model is empty";
        model = "ai";
    }

    QJsonArray contentArray = obj["content"].toArray();
    QJsonArray infoSourceArray;
    QMap<int, QString> toolMap;
    QMap<int, QString> infoSourceMap;
    QString fileData = "";

    if(messageType == "tool_call"){

        // 遍历content数组,取出tool_id tool_name
        for(const QJsonValue &value: contentArray)
        {
            QJsonObject itemObject = value.toObject();

            if(itemObject["type"].toString() == "text")
            {
                systemChat->text = itemObject["text"].toString();
            }
            // 检查是否包含tool_id和tool_name
            if(itemObject.contains("tool_id") && itemObject.contains("tool_name"))
            {
                int toolId = itemObject["tool_id"].toInt();
                QString toolName = itemObject["tool_name"].toString();
                toolMap.insert(toolId, toolName);
            }
        }

        for(auto i = toolMap.begin(); i != toolMap.end(); ++i)
        {
            KyInfo() << "toolid:" << i.key() << "toolName:" << i.value();
        }

        QString fileType = obj.value("file_type").toString();
        if(fileType == "image")
        {
            //错误处理
            if(errorCode != 0)
            {
                Q_EMIT systemChat->toolReply(errorMsg, toolMap, imageUrl, errorCode, model, feedbackURL);
            }
            else
            {
                Q_EMIT systemChat->toolReply(systemChat->text, toolMap, imageUrl, errorCode, model, feedbackURL);
                KyInfo() << "________________emit toolReply success";
            }
            imageUrl = "";
        }
        else if (fileType == "document") {

        }
        else if (fileType == "schedule") {
            if (toolMap.contains(21)) {
                if (errorCode != 0) {
                    const InnerMessage message("tool_call", "", errorMsg, true, true, 1, errorCode, model, feedbackURL);
                    Q_EMIT systemChat->reply(message);

                    return;
                }
            }
            else if (toolMap.contains(22)) {
                if(errorCode != 0)
                {
                    const InnerMessage message("tool_call", "", errorMsg, true, true, 1, errorCode, model, feedbackURL);
                    Q_EMIT systemChat->reply(message);

                    return;
                }
            }
        }
    }
    else if(messageType == "image_generation")
    {
        QJsonValue itemObject = contentArray.at(0);
        QString status = itemObject["status"].toString();
        if(status == "generating")
        {
            //向前端发送生成中
            Q_EMIT systemChat->sigGeneratingPic(model, feedbackURL);

        }
        else if(status == "generated")
        {
            if(itemObject["type"].toString() == "file_data")
            {
                fileData =itemObject["file_data"].toString();
            }
            //向前端发送生成的图片
            if(fileData != "")
            {
                Q_EMIT systemChat->sigPictureUpdate(fileData, errorMsg);
            }
        }
        else if(status == "canceled")
        {
            Q_EMIT systemChat->sigPictureUpdate("", "");

        }
        else  //status == failed
        {
            if(errorCode != 0)
            {
                //21 未配置模型错误，未配置模型时，不显示生成失败图片
                if(errorCode != 21){
                    //文生图失败时，向前端发送生成失败的图片
                    Q_EMIT systemChat->sigPictureUpdate(fileData, "failed");
                }

                const InnerMessage message("image_generation","", errorMsg, true, true, 1, errorCode, model, feedbackURL);
                Q_EMIT systemChat->reply(message);


            }
        }
    }
    else // messageType == "chat"
    {
        // 遍历content数组
        for(const QJsonValue &value: contentArray)
        {
            // 假设每个元素都是一个对象
            QJsonObject itemObject = value.toObject();

            KyInfo() << "文本类回答-chat-" << itemObject;
            // 检查type是否为"text"
            if(itemObject["type"].toString() == "text")
            {
                // 获取text子对象
                QJsonObject textObject = itemObject["text"].toObject();
                systemChat->is_end = textObject["is_end"].toBool();

                // 将Markdown转换为HTML
                QString markdownText = textObject["result"].toString();
                systemChat->result_msg = markdownText;

                systemChat->result_index = textObject["sentence_id"].toInt();

                // 推理内容也需要转换为HTML
                QString reasoningMarkdown = textObject["reasoning_content"].toString();
                systemChat->reasoningContentMsg = reasoningMarkdown;
                if(systemChat->reasoningContentMsg == ""){
                    systemChat->is_reasoning_end = true;
                }else{
                    systemChat->is_reasoning_end = false;
                }
//                if(systemChat->is_end){
//                    systemChat->is_reasoning_end = true;
//                }
                //systemChat->reasoningContentMsg = textObject["result"].toString();
            }

            if(itemObject["type"].toString() == "file_data")
            {
                fileData = itemObject["file_data"].toString();
            }
        }

        // 检查是否包含information_sources字段（模型引用的信息）
        if(obj.contains("information_sources") && !obj.value("information_sources").isNull())
        {
            infoSourceArray = obj["information_sources"].toArray();
        }

        // 遍历information-sources数组
        for(const QJsonValue &value: infoSourceArray)
        {
            QJsonObject itemObject = value.toObject();
            // 检查是否包含text和tool_id
            if(itemObject.contains("text") && itemObject.contains("tool_id"))
            {
                int toolId = itemObject["tool_id"].toInt();
                QString sourceName = itemObject["text"].toString();
                infoSourceMap.insert(toolId, sourceName);
            }

        }
        for(auto i = infoSourceMap.begin(); i != infoSourceMap.end(); ++i)
        {
            KyInfo() << "information-sources: "
                     << "toolid:" << i.key() << "sourceName:" << i.value();
        }

        // 解析联网搜索信息来源（模型查询的网页）- 不依赖 is_end，避免丢失先到达的搜索结果
        // 注意：当前使用 OsAssistant 的 JSON 回调，无法直接获取 ChatResult 对象
        // 如果 JSON 中没有搜索信息，需要修改 OsAssistant 来提供 C SDK 回调：
        // const ChatResultSearchInfo *searchInfo = chat_result_get_search_info(result, nullptr);
        // size_t count = chat_result_search_info_get_count(searchInfo, nullptr);
        // for (size_t i = 0; i < count; i++) {
        //     const ChatResultSearchItem *item = chat_result_search_info_get_item(searchInfo, i, nullptr);
        //     size_t index = chat_result_search_item_get_index(item, nullptr);
        //     std::string title = chat_result_search_item_get_title(item, nullptr);
        //     std::string url = chat_result_search_item_get_url(item, nullptr);
        // }
        QJsonArray webSearchSourcesArray;
        // 检查JSON中是否有搜索信息字段（可能的字段名：search_info, search_results, web_search_results等）
        if (obj.contains("search_info") && !obj.value("search_info").isNull()) {
            QJsonArray searchInfoArray = obj["search_info"].toArray();
            for(const QJsonValue &value: searchInfoArray) {
                if (!value.isObject()) {
                    continue;
                }
                QJsonObject itemObject = value.toObject();
                QJsonObject sourceObj;
                // 解析 index（示例代码中是 size_t，JSON 中可能是 int）
                if (itemObject.contains("index")) {
                    sourceObj["index"] = itemObject["index"].toInt();
                }
                // 解析 title（示例代码中是 std::string）
                if (itemObject.contains("title")) {
                    sourceObj["title"] = itemObject["title"].toString();
                }
                // 解析 url（示例代码中是 std::string）
                if (itemObject.contains("url")) {
                    sourceObj["url"] = itemObject["url"].toString();
                }
                // web search 独立的数据结构，不伪装成 information_sources
                sourceObj["type"] = "web_search";
                // 只有当至少包含 title 或 url 时才添加
                if (sourceObj.contains("title") || sourceObj.contains("url")) {
                    webSearchSourcesArray.append(sourceObj);
                }
            }
        } else if (obj.contains("search_results") && !obj.value("search_results").isNull()) {
            QJsonArray searchResultsArray = obj["search_results"].toArray();
            for(const QJsonValue &value: searchResultsArray) {
                if (!value.isObject()) {
                    continue;
                }
                QJsonObject itemObject = value.toObject();
                QJsonObject sourceObj;
                if (itemObject.contains("index")) {
                    sourceObj["index"] = itemObject["index"].toInt();
                }
                if (itemObject.contains("title")) {
                    sourceObj["title"] = itemObject["title"].toString();
                }
                if (itemObject.contains("url")) {
                    sourceObj["url"] = itemObject["url"].toString();
                }
                sourceObj["type"] = "web_search";
                if (sourceObj.contains("title") || sourceObj.contains("url")) {
                    webSearchSourcesArray.append(sourceObj);
                }
            }
        }

        is_reply_end = systemChat->is_end;

        QJsonValue itemObject = contentArray.at(0);
        QString status = itemObject["status"].toString();

        switch(systemChat->target)
        {
        case TartgetType::RECVMSG:
            //错误处理
            if(errorCode != 0)
            {
                const InnerMessage message("chat","", errorMsg, true, true, 1, errorCode, model, feedbackURL);
                Q_EMIT systemChat->reply(message);
                break;
            }
            if(status != ""){
                if(status == "generating")
                {
                    //向前端发送生成中
                    Q_EMIT systemChat->sigGeneratingPic(model, feedbackURL);

                }
                else if(status == "generated")
                {
                    if(itemObject["type"].toString() == "file_data")
                    {
                        fileData =itemObject["file_data"].toString();
                    }
                    //向前端发送生成的图片
                    if(fileData != "")
                    {
                        Q_EMIT systemChat->pictureOperateUpdate(fileData, errorMsg);
                    }
                }
                else  //status == failed
                {
                    if(errorCode != 0)
                    {
                        const InnerMessage message("chat", "", errorMsg, true, true, 1, errorCode, model, feedbackURL);
                        Q_EMIT systemChat->reply(message);
                    }
                }
            }else{
                const InnerMessage message("chat", systemChat->reasoningContentMsg, systemChat->result_msg, systemChat->is_reasoning_end, systemChat->is_end, systemChat->result_index, errorCode, model, feedbackURL);
                Q_EMIT systemChat->reply(message);
            }

            // 发送 information_sources（模型引用的信息）- 只在回答结束时发送
            if(obj.contains("information_sources") && !obj.value("information_sources").isNull() && systemChat->is_end == true)
            {
                Q_EMIT systemChat->sigInfoSourceUpdate(infoSourceMap);
            }
            
            // 发送联网搜索信息来源（模型查询的网页）- 不依赖 is_end，有数据就发送
            if (!webSearchSourcesArray.isEmpty())
            {
                Q_EMIT systemChat->sigWebSearchSourceUpdate(webSearchSourcesArray);
            }
            break;
        case TartgetType::SUMMARY_TEXT:
            Q_EMIT systemChat->sigGenerateSummary(systemChat->result_msg, systemChat->is_end, systemChat->result_index, errorCode, model);
            break;
        }
    }
}

void SystemChat::run()
{
    QObject workerAnchor;
    workerAnchor.moveToThread(QThread::currentThread());

    connect(this, &SystemChat::sigEnqueueSend, &workerAnchor,
            [this](QString text, QString filePath, bool webSearch, bool deepThinking,
                   bool intentRecognition, QStringList documentPaths, bool appendDocumentText) {
                if (m_lastInitError.code == ErrorCode::Ok) {
                    sendMessageImpl(text, filePath, webSearch, deepThinking, intentRecognition,
                                    documentPaths, appendDocumentText);
                    return;
                }
                PendingChatSend pending;
                pending.text = text;
                pending.filePath = filePath;
                pending.documentPaths = documentPaths;
                pending.webSearch = webSearch;
                pending.deepThinking = deepThinking;
                pending.intentRecognition = intentRecognition;
                pending.appendDocumentText = appendDocumentText;
                m_pendingSend = pending;
                m_hasPendingSend = true;
                KyInfo() << "SystemChat SDK not ready, cache single pending sendMessage (latest replaces previous)";
                Q_EMIT sigRetryInit();
            }, Qt::QueuedConnection);

    connect(this, &SystemChat::sigRetryInit, &workerAnchor, [this]() {
        initSystemChat();
    }, Qt::QueuedConnection);

    initSystemChat();

    QEventLoop loop;
    loop.exec();
}

SystemChat::SystemChat(QObject *parent)
    : QThread (parent)
{
    m_lastInitError.code = ErrorCode::InitNotCompleted;

    ChatDatabase *database = ChatDatabase::getChatDatabase();
    int history_id_database = database->selectMaxHistoryId();
    int history_id_settings  = ConfigManager::getInstance()->getConfigValue("history-id").toInt();
    m_sessionID = history_id_database >= history_id_settings ? history_id_database : history_id_settings;

    m_features = new Features(0);

    start();
}

SystemChat::~SystemChat()
{
}

void SystemChat::setActor(int actorId)
{
    if (m_osassistant == nullptr) {
        KyInfo() << "setActor: m_osassistant is null";
        return;
    }
    m_osassistant->setPromptId(actorId);
}

std::vector<Prompt> SystemChat::getPrompts()
{
    m_promptList.clear();
    if (m_osassistant == nullptr)
        return m_promptList;
    m_promptList = m_osassistant->prompts();
//    for (const auto& prompt : m_promptList) {
//        KyInfo() << "========================";
//        KyInfo() << "Prompt ID:" << prompt.id;
//        KyInfo() << "Prompt Name:" << QString::fromStdString(prompt.name);
//        KyInfo() << "Prompt Content:" << QString::fromStdString(prompt.content);
//        KyInfo() << "Prompt promptType:" << prompt.type;
//        KyInfo() << "Prompt isActive:" << prompt.active;
//        KyInfo() << "========================";
//    }

    return m_promptList;
}

bool SystemChat::setCommandOrder(QList<CommandTemplateItem> commandTemplateList)
{
    std::list<int> promptIdList;

    for (const auto& item : commandTemplateList) {
        promptIdList.push_back(item.actorId);  // 按新顺序添加 promptId
    }

    KyInfo() << "promptIdList:" << promptIdList;

    // 确保列表不为空
    if (promptIdList.empty()) {
        KyInfo() << "错误：promptIdList 为空，无法更新顺序";
        return false;
    }

    // 检查是否包含负数等无效ID
    bool hasInvalidId = std::any_of(promptIdList.begin(), promptIdList.end(), [](int id) { return id <= 0; });
    if (hasInvalidId) {
        KyInfo() << "错误：promptIdList 包含无效 ID，无法更新顺序";
        return false;
    }

    if (!m_osassistant) {
        KyInfo() << "setCommandOrder: m_osassistant is null";
        return false;
    }

    bool ret = m_osassistant->setPromptOrder(promptIdList);  // 更新顺序到 SDK

//    if (ret) {
//        // 更新后的 prompts 列表日志输出
//        std::vector<Prompt> prompts = getPrompts();

//        KyInfo() << "========================";
//        for (const auto& prompt : prompts) {
//            KyInfo() << "Prompt ID:" << prompt.id;
//        }
//        KyInfo() << "========================";
//    }

    return ret;
}


int SystemChat::createCommand(QString name, QString content)
{
    if (m_osassistant == nullptr)
        return -1;
    KyInfo() << "create prompt" << "name" << name << "content" << content;
    int id = m_osassistant->createPrompt(name.toStdString(), content.toStdString());
    return id;
}

bool SystemChat::updateCommand(int actorId, QString name, QString content)
{
    if (m_osassistant == nullptr)
        return false;
    bool ret = m_osassistant->updatePrompt(actorId, name.toStdString(), content.toStdString());
    return ret;
}

bool SystemChat::deleteCommand(int actorId)
{
    if (m_osassistant == nullptr)
        return false;
    bool ret = m_osassistant->deletePrompt(actorId);
    return ret;
}

void SystemChat::stopChat()
{
    if (m_osassistant == nullptr)
        return;
    m_osassistant->stopChat();
}

void SystemChat::clearContext()
{
    if (m_osassistant == nullptr)
        return;
    m_osassistant->clearContext();
}

SystemChat *SystemChat::getPtr()
{
    if (g_nlpChat == nullptr) {
        g_nlpChat = new SystemChat;
    }

    return g_nlpChat;
}

void SystemChat::releaseOsAssistant()
{
    if (m_osassistant != nullptr) {
        delete m_osassistant;
        m_osassistant = nullptr;
    }
}

void SystemChat::initSystemChat()
{
    if (m_lastInitError.code == ErrorCode::Ok && m_osassistant != nullptr) {
        return;
    }

    releaseOsAssistant();

    m_osassistant = new kyai::assistant::OsAssistant();
    m_osassistant->setChatAsyncCallback(chatCallback);

    KyInfo() << "*** start initSystemChat m_osassistant->init()";

    Error error = m_osassistant->init();
    m_lastInitError = error;
    //SDK ErrorCode 内部定义冲突
    if (error.code != kyai::assistant::ErrorCode::Ok) {
        KyInfo() << "[ChatSDK] init attempt finished: FAILED, code=" << static_cast<int>(error.code)
                 << "msg=" << Globals::getErrorMsg(static_cast<int>(error.code));
        if (m_hasPendingSend) {
            const QString errMsg =
                Globals::getErrorMsg(static_cast<int>(error.code));
            const InnerMessage initFailMsg("chat", "", errMsg, true, true, 1,
                                           static_cast<int>(error.code), QStringLiteral("ai"), QString());
            Q_EMIT reply(initFailMsg);
            m_hasPendingSend = false;
        }
        // 不在此处自动重试；仅在下一次 sendMessage → sigEnqueueSend 时再触发 init
        return;
    }

    KyInfo() << "*** end initSystemChat m_osassistant->init()";
    KyInfo() << "[ChatSDK] init attempt finished: SUCCESS";

    m_promptList = m_osassistant->prompts();

    *m_features = m_osassistant->supportedFeatures();

    if (m_hasPendingSend) {
        PendingChatSend one = m_pendingSend;
        m_hasPendingSend = false;
        KyInfo() << "flush single pending chat send";
        sendMessageImpl(one.text, one.filePath, one.webSearch, one.deepThinking,
                        one.intentRecognition, one.documentPaths, one.appendDocumentText);
    }

    Q_EMIT sigInitCompleted();
}

void SystemChat::initWithHistory(std::vector<Message> messages)
{
    if (m_osassistant == nullptr) {
        KyInfo() << "initWithHistory: m_osassistant is null";
        return;
    }
    Error error = m_osassistant->initWithChatHistory(messages);
    if(error.code != kyai::assistant::ErrorCode::Ok){
        KyInfo() << "init with history failed.";
    }
}

void SystemChat::sendMessage(QString text, QString filePath, bool webSearch, bool deepThinking,
                             bool intentRecognition, QStringList documentPaths,
                             bool appendDocumentText)
{
    Q_EMIT sigEnqueueSend(text, filePath, webSearch, deepThinking, intentRecognition,
                          documentPaths, appendDocumentText);
}

void SystemChat::sendMessageImpl(QString text, QString filePath, bool webSearch,
                                 bool deepThinking, bool intentRecognition,
                                 const QStringList &documentPaths,
                                 bool appendDocumentText)
{
    if (m_osassistant == nullptr) {
        KyInfo() << "sendMessageImpl skipped: m_osassistant is null";
        return;
    }
    // 创建顶层对象
    QJsonObject jsonObject;
    // 设置message_type字段
    jsonObject["message_type"] = "chat";
    // 创建content数组
    QJsonArray contentArray;

    QStringList pathsToExtract = documentPaths;
    if (filePath.length() > 0) {
        // 判断文件格式
        FileType type = judgeFileType(filePath);
        if (type == IMAGE) {
            // 图片URL消息
            QJsonObject imageUrlMessage;
            imageUrlMessage["type"] = "image_url";
            imageUrlMessage["image_url"] = filePath;
            contentArray.append(imageUrlMessage);
            imageUrl = filePath;
        } else if (type == DOCUMENT) {
            pathsToExtract.prepend(filePath);
        }
    }

    if (!pathsToExtract.isEmpty()) {
        DocumentReader reader;
        QStringList documentBlocks;
        for (const QString &documentPath : pathsToExtract) {
            const DocumentContent document = reader.read(documentPath);
            if (!document.isValid()) {
                KyInfo() << "DocumentReader failed for" << documentPath << document.error;
                Q_EMIT sigDocumentParseFailed(documentPath, document.error);
                return;
            }
            documentBlocks.append(QStringLiteral("===== %1 =====\n%2")
                                      .arg(QFileInfo(documentPath).fileName(), document.text));
        }

        const QString documentText = documentBlocks.join(QStringLiteral("\n\n"));
        if (appendDocumentText) {
            text += QStringLiteral("\n\n参考文档内容：\n") + documentText;
        } else {
            text = QStringLiteral("请仅根据以下内容作为参考，回答用户的问题。\n")
                + documentText + QStringLiteral("\n\n用户问题：\n") + text;
        }
    }

    // 文本消息
    if (text.length() > 0) {
        QJsonObject textMessage;
        textMessage["type"] = "text";
        textMessage["text"] = text;
        contentArray.append(textMessage);
    }

    // 将content数组设置到顶层对象
    jsonObject["content"] = contentArray;

    // 设置意图识别
    jsonObject["intent_recognition"] = intentRecognition;

    // 设置联网搜索和深度思考
    jsonObject["web_search"] = webSearch;
    jsonObject["enable_thinking"] = deepThinking;

    QJsonDocument jsonDoc(jsonObject);
    KyInfo() << "m_osassistant sendMessage chatAsync.";
    m_osassistant->chatAsync(jsonDoc.toJson().toStdString());
}

void SystemChat::sendToolMessage(int toolId, FileType fileType, QString para)
{
    if (m_osassistant == nullptr) {
        KyInfo() << "sendToolMessage: m_osassistant is null";
        return;
    }
    QJsonObject jsonObject;
    // message_type字段
    jsonObject["message_type"] = "tool_call";
    // file_type字段
    QString type = "";
    switch (fileType){
        case IMAGE:
            type = "image";
            break;
        case DOCUMENT:
            type = "document";
            break;
        case SOURCE:
            type = "source_url";
            break;
        case SCHEDULE:
            type = "schedule";
            break;
    }
    jsonObject["file_type"] = type;

    // content数组
    QJsonArray contentArray;

    // 调用的工具
    QJsonObject textMessage;
    textMessage["type"] = "tool_id";
    textMessage["tool_id"] = toolId;
    contentArray.append(textMessage);

    if(fileType == IMAGE) {
        // 图片URL
        QJsonObject imageUrlMessage;
        imageUrlMessage["type"] = "image_url";
        imageUrlMessage["image_url"] = para;
        contentArray.append(imageUrlMessage);
    }
    else if(fileType == DOCUMENT) {
        // 文件URL
        QJsonObject imageUrlMessage;
        imageUrlMessage["type"] = "image_url";
        imageUrlMessage["image_url"] = para;
        contentArray.append(imageUrlMessage);
    }
    else if (fileType == SCHEDULE) {
        QJsonObject scheduleMessage;
        scheduleMessage["type"] = "schedule_date";
        scheduleMessage["schedule_date"] = para;
        contentArray.append(scheduleMessage);
    }
    else {
        QJsonObject imageUrlMessage;
        contentArray.append(imageUrlMessage);
    }

    jsonObject["content"] = contentArray;
    KyInfo() << "m_osassistant sendToolMessage chatAsync.";
    QJsonDocument jsonDoc(jsonObject);
    m_osassistant->chatAsync(jsonDoc.toJson().toStdString());

    // 超时处理已移至前端 QML 端
}

void SystemChat::setSession(int sessionID)
{
    m_sessionID = sessionID;
}

int SystemChat::getSession()
{
    return m_sessionID;
}

bool SystemChat::getFeatureStatus(Features::Feature feature)
{
    KyInfo() << "init getFeatureStatus";

    bool isSupported = m_features->containsFeature(feature);

    KyInfo() << "end getFeatureStatus";

    return isSupported;
}

QList<ModelItem *> SystemChat::getNLPModelList()
{
    if (m_osassistant == nullptr) {
        return {};
    }

    std::string t1 = m_osassistant->getConfiguredNlpModelInfos();
    const char *ret = t1.data();
    // Qt 5.12 环境下，后端可能返回空字符串，这里需要显式检查
    if (t1.empty()) {
        KyInfo() << "getNLPModelList Failed！ (empty JSON)";
        return {};
    }

    QList<ModelItem *> result;
    // 解析JSON文档
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(ret, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误:" << parseError.errorString();
        qDebug() << "origin text: " << ret;
        return result;
    } else {
        qDebug().noquote() << jsonDoc.toJson(QJsonDocument::Indented);
    }

    // 获取根对象
    if (!jsonDoc.isObject()) {
        qDebug() << "JSON文档不是一个对象";
        return result;
    }

    QJsonObject rootObj = jsonDoc.object();

    // 提取模型信息数组
    if (!rootObj.contains("currentModel")) {
        qDebug() << "缺少currentModel字段";
        return result;
    }

    ModelItem *first = new ModelItem;
    first->m_modelName = tr("System Default");
    first->m_description = tr("Intelligently invoke the system's default model configuration");
    first->m_type = "intelligent";
    first->m_iconPath = "qrc:/html/icon/intelligent-model.svg";
    first->m_icomPathWhite = "qrc:/html/icon/intelligent-model-white.svg";
    first->m_isChecked = false;
    first->m_isModelEnabled = true;      // 智能模式始终可用
    first->m_isSharedModel = false;      // 智能模式不是共享模型
    // 这里插入的智能模式，将部署类型设置为 -1，避免跟模型的冲突
    first->m_deployType = -1;

    QString curModelName = rootObj["currentModel"].toString();
    qDebug() << "************* cur model:" << curModelName;
    if(curModelName == ""){
       first->m_isChecked = true;
    }
    result.append(first);

    // 提取模型信息数组
    if (!rootObj.contains("modelInfos") || !rootObj["modelInfos"].isArray()) {
        qDebug() << "缺少modelInfos数组或格式错误";
        return result;
    }

    QJsonArray modelArray = rootObj["modelInfos"].toArray();

    // 按deployType分组存储模型信息
    QMap<int, QList<QJsonObject>> deployTypeMap;
    // 记录deployType首次出现的顺序
    QList<int> deployTypeOrder;

    // 遍历模型信息数组，先按deployType分组
    for (const auto& modelValue : modelArray) {
        if (!modelValue.isObject()) {
            qDebug() << "模型信息不是一个对象";
            continue;
        }

        QJsonObject modelObj = modelValue.toObject();

        // 提取System Default的按钮状态相关字段
        if (modelObj.contains("priority") && modelObj["priority"].isDouble()
                && modelObj.contains("isModelSelected") && modelObj["isModelSelected"].isBool()) {
            int priority = modelObj["priority"].toInt();
            bool isModelSelected = modelObj["isModelSelected"].toBool();

            if (priority == 1 && isModelSelected == true) {
                if (modelObj.contains("supportsThinking") && modelObj["supportsThinking"].isBool()) {
                    first->m_isSupportsThinking = modelObj["supportsThinking"].toBool();
                }

                if (modelObj.contains("supportsThinkingToggle") && modelObj["supportsThinkingToggle"].isBool()) {
                    first->m_isSupportsThinkingToggle = modelObj["supportsThinkingToggle"].toBool();
                }

                if (modelObj.contains("supportsWebSearch") && modelObj["supportsWebSearch"].isBool()) {
                    first->m_isSupportsWebSearch= modelObj["supportsWebSearch"].toBool();
                }
            }
        }

        // 按deployType分组
        int deployType = 0;
        if (modelObj.contains("deployType") && modelObj["deployType"].isDouble()) {
            deployType = modelObj["deployType"].toInt();
        }

        // 记录deployType首次出现的顺序
        if (!deployTypeOrder.contains(deployType)) {
            deployTypeOrder.append(deployType);
        }

        deployTypeMap[deployType].append(modelObj);
    }

    // 对每个deployType组内的模型按modelName字母顺序排序
    for (auto& deployType : deployTypeMap.keys()) {
        auto& modelList = deployTypeMap[deployType];
        std::sort(modelList.begin(), modelList.end(), [](const QJsonObject& a, const QJsonObject& b) {
            QString nameA = a.contains("modelName") ? a["modelName"].toString() : "";
            QString nameB = b.contains("modelName") ? b["modelName"].toString() : "";
            return nameA.localeAwareCompare(nameB) < 0;
        });
    }

    // 按deployType首次出现的顺序依次插入模型信息
    for (auto& deployType : deployTypeOrder) {
        for (const auto& modelObj : deployTypeMap[deployType]) {
            // 提取每个模型的信息
            ModelItem *info = new ModelItem;

            // 设置默认值
            info->m_isModelEnabled = true;    // 默认可用
            info->m_isSharedModel = false;    // 默认不是共享模型

            if (modelObj.contains("brand") && modelObj["brand"].isString()) {
                QString brand = modelObj["brand"].toString();
                if (brand == "deepseek") {
                    info->m_iconPath = "qrc:/html/icon/deepseek-model.svg";
                    info->m_icomPathWhite = "qrc:/html/icon/deepseek-model-white.svg";
                } else if (brand == "qwen") {
                    info->m_iconPath = "qrc:/html/icon/qwen-model.svg";
                    info->m_icomPathWhite = "qrc:/html/icon/qwen-model-white.svg";
                } else if (brand == "custom") {
                    info->m_iconPath = "qrc:/html/icon/custom-model.svg";
                    info->m_icomPathWhite = "qrc:/html/icon/custom-model-white.svg";
                } else if (brand == "shared") {  // 新增：共享模型图标
                    info->m_iconPath = "qrc:/html/icon/custom-model.svg";
                    info->m_icomPathWhite = "qrc:/html/icon/custom-model-white.svg";
                }
            }
            if (modelObj.contains("modelName") && modelObj["modelName"].isString()) {
                info->m_modelName = modelObj["modelName"].toString();
            }

            if (modelObj.contains("deployType") && modelObj["deployType"].isDouble()) {
                int num = modelObj["deployType"].toInt();
                info->m_deployType = num;
                switch (num) {
                case 0:
                    info->m_description = tr("Local Model");
                    info->m_type = "local";
                    break;
                case 1:
                    info->m_description = tr("Cloud Model");
                    info->m_type = "cloud";
                    break;
                case 2:
                    info->m_description = tr("Optional Model");
                    info->m_type = "optional";
                    break;
                case 3:  // 共享模型类型
                    info->m_description = tr("Shared Model");
                    info->m_type = "shared";
                    break;
                default:
                    break;
                }
            }
            // 解析是否可用字段
            if (modelObj.contains("isModelEnabled") && modelObj["isModelEnabled"].isBool()) {
                info->m_isModelEnabled = modelObj["isModelEnabled"].toBool();
            }

            // 解析是否是共享模型字段
            if (modelObj.contains("isSharedModel") && modelObj["isSharedModel"].isBool()) {
                info->m_isSharedModel = modelObj["isSharedModel"].toBool();
                // 如果是共享模型，则强制类型为shared
                if (info->m_isSharedModel) {
                    info->m_type = "shared";
                    info->m_description = tr("Shared Model");
                }
            }

            if (modelObj.contains("supportsThinking") && modelObj["supportsThinking"].isBool()) {
                info->m_isSupportsThinking = modelObj["supportsThinking"].toBool();
            }

            if (modelObj.contains("supportsThinkingToggle") && modelObj["supportsThinkingToggle"].isBool()) {
                info->m_isSupportsThinkingToggle = modelObj["supportsThinkingToggle"].toBool();
            }

            if (modelObj.contains("supportsWebSearch") && modelObj["supportsWebSearch"].isBool()) {
                info->m_isSupportsWebSearch= modelObj["supportsWebSearch"].toBool();
            }

            if (info->m_modelName == curModelName){
                info->m_isChecked = true;
            }

            result.append(info);
        }
    }

    return result;
}

QString SystemChat::getNLPCurModel()
{
    QString res = ConfigManager::getInstance()->getConfigValue("cur-nlp-model").toString();
    if (res == ""){
        res = tr("System Default");
    }

    return res;
}

void SystemChat::setNLPModel(QString name)
{
    m_osassistant->setCurrentNlpModel(name.toStdString());
    Q_EMIT sigNLPModelChanged();
}

FileType SystemChat::judgeFileType(QString filePath)
{
    //判断是否为支持的图片格式
    if(g_supportPicFormats.contains(QFileInfo(filePath).suffix().toLower()))
    {
        return IMAGE;
    }
    else if(g_supportDocFormats.contains(QFileInfo(filePath).suffix().toLower()))
    {
        return DOCUMENT;
    }
    else
    {
        return ERROR;
    }
}

// 超时处理已移至前端 QML 端
