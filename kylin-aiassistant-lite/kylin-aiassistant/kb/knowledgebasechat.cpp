#include <QCoreApplication>
#include "knowledgebasechat.h"
#include <cstring>
#include <QDateTime>
#include <QDir>
#include <QSet>

#define KB_DB_PATH "/.config/kylin-aiassistant/knowledgeBase"
#define SYSTEM_GUIDE_APP_ID "kylin-ai-knowledge-base-service"
#define SYSTEM_GUIDE_PASSWORD "k6bf1d_349ecd"
#define SYSTEM_GUIDE_DATABASE_PATH "/usr/share/kylin-ai-runtime/knowledge-base/"

static const char * appName = "kylin-aiassistant";
static const char * KBName = "kylinAiassistant";
static KnowledgeBase *g_kbChat = nullptr;

static int KBCallback(const KBChatResponse *response, void *user_data){
    KnowledgeBase *kb = static_cast<KnowledgeBase *>(user_data);
    if (kb == nullptr) {
        KyInfo() << "kb is null.";
        return 0;
    }

    // 超时处理已移至前端 QML 端

    int errorCode = 0;
    errorCode = kb_chat_response_get_error_code(response);

    KbChatMessage *msg = new KbChatMessage();
    // 解析返回数据
    const KBChatResult * result = kb_chat_response_get_result(response);

    msg->m_isEnd = kb_chat_result_is_finished(result);
    msg->m_model = kb_chat_result_get_model(result);

    if (errorCode == -1) {
        qInfo() << "KB KBCallback get errorCode FAILED";
        return 0;
    } else if (errorCode != 0) {
        KyInfo() << "kbCallBack errorCode=" << errorCode;
        // 返回错误码时，从response中读取错误信息
        msg->m_content = kb_chat_response_get_error_message(response);
        // 向前端发送错误信息
        Q_EMIT kb->reply(msg, errorCode);

        return 0;
    }

    // 是否需要在此处维护HISTORY?
    // const char * question = kb_chat_result_get_question(result);
    const char * answer = kb_chat_result_get_response(result);
    const char * reasoningContent = kb_chat_result_get_reasoning_content(result);

    int referenceCount = 0;
    const KBChatReference ** reference = kb_chat_result_get_reference(result, &referenceCount);

    // 系统指南模式(promptId==4)不允许拼接indexString
    QString indexString = "";
    if (!kb->isSystemGuideMode()) {
        for(int i = 0; i < referenceCount; ++i){
            const KBChatReference * item = reference[i];
            int index = kb_chat_reference_get_file_number(item);
            int chunkIndex = kb_chat_reference_get_chunk_index(item);
            KyInfo() << "--index--" << index << "--chunkIndex--" << chunkIndex;
            msg->m_chunkIndexMap->insert(index, chunkIndex);
            QChar circledChar(0x2460 + index - 1);
            indexString = indexString.append(QString(QChar(circledChar.unicode())));
        }
    }

    msg->m_content = QString::fromUtf8(answer).append(indexString);
    msg->m_reasonContent = QString::fromUtf8(reasoningContent);

    if(msg->m_reasonContent == ""){
        msg->m_isReasoningEnd = true;
    }else{
        msg->m_isReasoningEnd = false;
    }

    int fileIdCount;
    const char ** fileIds = kb_chat_result_get_reference_file_ids(result, &fileIdCount);
    for(int i = 0; i < fileIdCount; ++i){
        const char * item = fileIds[i];
        msg->m_fileIds.append(QString::fromStdString(item));

    }

    int count = 0;
    const KBChatSource ** source = kb_chat_result_get_source(result, &count);
    for (int i = 0; i < count; ++i) {
        const KBChatSource * item = source[i];
        const char * fileID = kb_chat_source_get_file_id(item);
        const char * content = kb_chat_source_get_content(item);
        //msg->m_fileMap->insert(fileID, QString::fromStdString(content));
        msg->m_fileContents.append(QString::fromStdString(content));
    }

    // 向前端发送回答信息
    Q_EMIT kb->reply(msg, errorCode);

    return 0;
}

KnowledgeBase::KnowledgeBase(QObject *parent)
    : QThread{parent}
{
    // 超时处理已移至前端 QML 端
    start();
}

KnowledgeBase::~KnowledgeBase()
{
    if (m_session) {
        kb_session_destroy(m_session);
    }
    if (m_systemGuideSession) {
        kb_session_destroy(m_systemGuideSession);
    }
    if (m_systemGuideConfig) {
            kb_chat_config_free(m_systemGuideConfig);
            m_systemGuideConfig = nullptr;
    }
}


KnowledgeBase *KnowledgeBase::getPtr()
{
    if (g_kbChat == nullptr){
        g_kbChat = new KnowledgeBase;
    }

    return g_kbChat;
}

void KnowledgeBase::init()
{
    bool ret = 0;
    ret = initSession();
    if (!ret) return;

    // 从配置文件中读取知识库ID，如果ID未空，创建知识库； 如果ID不为空，根据session和ID获取KBinfo实例
    QVariant varId = ConfigManager::getInstance()->getConfigValue("knowledgebase-id");
    QByteArray byteArray = varId.toString().toUtf8();  // 先转为 QByteArray
    m_KBID  = strdup(byteArray.data());  // 获取 const char*
    if (byteArray.isEmpty()) {
        ret = initKBID();
        if (!ret) return;
    } else {
        m_KBInfo = kb_knowledgebase_info_get(m_session,m_KBID,&m_error);
        if (!m_KBInfo) {
            qInfo() << "KB kb_knowledgebase_info_get FAILED";
            qInfo() << "reason :" << kb_get_error_message(m_error);

            return;
        }
    }

    qInfo() << "KB init KB_ID KB_INFO SUCCESS";

    initHistory();
    initConfig();

}

bool KnowledgeBase::initSession()
{
    qInfo() << "KB auth_create_credential SUCCESS";

    QString kbPath = QDir::homePath().append(KB_DB_PATH);
    if (!QDir().exists(kbPath)) {
        QDir().mkdir(kbPath);
    }

    m_session = kb_session_create(appName, appName, kbPath.toUtf8().constData(), &m_error);
    if (!m_session) {
        qInfo() << "KB kb_session_create FAILED";
        qInfo() << "reason :" << kb_get_error_message(m_error);

        return false;
    }
    qInfo() << "KB kb_session_create SUCCESS";


    m_error = kb_session_init(m_session);
    if (m_error != static_cast<int>(AI_COMMON_SUCCESS)) {
        qInfo() << "KB kb_session_init FAILED";
        qInfo() << "reason :" << kb_get_error_message(m_error);

        return false;
    }
    qInfo() << "KB kb_session_init SUCCESS";

    return true;
}


///
/// \brief KnowledgeBase::initKBID
///  初始化知识库，配置文件中数据库ID为空时，会执行以下代码
/// \return
///
bool KnowledgeBase::initKBID()
{
    m_KBInfo = kb_knowledgebase_info_new(KBName, "用于AI的综合知识库");
    if (!m_KBInfo) {
        qInfo() << "KB kb_knowledgebase_info_new FAILED";
        kb_knowledgebase_info_free(m_KBInfo);

        return false;
    }

    char * kb_id= kb_knowledgebase_create(m_session, m_KBInfo, &m_error);
    if (!kb_id) {
        // 如果知识库已经存在，根据name获取知识库ID
        if (m_error != KB_ERROR_KB_ALREADY_EXISTS){
            qInfo() << "KB kb_knowledgebase_create FAILED";
            qInfo() << "reason :" << kb_get_error_message(m_error);

            return false;
        }
        m_error = static_cast<int>(AI_COMMON_SUCCESS);

        qInfo() << "KB kb_knowledgebase already exists";
        // 遍历infos列表，查找对应的知识库
        int kb_info_count;
        KnowledgeBaseInfo **kb_infos =
            kb_knowledgebase_info_list(m_session, &kb_info_count, &m_error);
        for (int i = 0; i < kb_info_count; i++) {
          std::string name = kb_knowledgebase_info_get_name(kb_infos[i]);
          if (strcmp(name.c_str(), KBName) == 0) {
            kb_id = strdup(kb_knowledgebase_info_get_id(kb_infos[i]));
            m_KBInfo = kb_infos[i];
            break;
          }
        }


    }

    m_KBID = strdup(kb_id);
    ConfigManager::getInstance()->updateConfig("knowledgebase-id",QVariant(m_KBID));
    qInfo() << "KB kb_knowledgebase_create SUCCESS" << m_KBID << "    " << m_KBInfo;
    kb_knowledgebase_info_set_id(m_KBInfo, m_KBID);
    kb_free_string(kb_id);

    return true;
}

void KnowledgeBase::initHistory()
{
    m_history = kb_chat_history_new();
}

void KnowledgeBase::initConfig()
{
    m_config = kb_chat_config_new();
}

bool KnowledgeBase::getDocumentList(QStringList& docList)
{
    int docCount;
    int error;

    char** kb_documents = kb_document_list(m_session, m_KBID, &docCount, &error);
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
        KyInfo() << "获取文档列表失败" << kb_get_error_message(error) << error;
        docList.clear(); // 清空列表，避免返回无效数据
        return false;
    }

    docList = charArrayToStringList(kb_documents);
    KyInfo() << "kd_documents:" << docList;
    return true;
}

bool KnowledgeBase::addDocument(const QString &docPath)
{

    if (!QFile::exists(docPath)) {
       KyInfo() << "当前待上传文件不存在" << docPath;
       return false;
    }
    int error;
    QByteArray idByteArray = docPath.toUtf8();  // 先转为 QByteArray
    const char* doc_id = idByteArray.constData();

    error = kb_document_add(m_session, m_KBID, doc_id, doc_id, NULL);
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
      KyInfo() << "文档上传失败"<< kb_get_error_message(error);
      return false;
    }
    KyInfo() << "文档上传成功 " << error;

    return true;
}

bool KnowledgeBase::addDocumentByUuid(const QString &docPath, const QString &uuid)
{
    KyInfo() << "Add doc by uuid : path " << docPath << " uuid " << uuid;
    if (!QFile::exists(docPath)) {
       KyInfo() << "当前待上传文件不存在" << docPath;
       return false;
    }

    int error;
    QByteArray pathByteArray = docPath.toUtf8();  // 先转为 QByteArray
    const char *path = pathByteArray.constData();

    QByteArray uuidByteArray = uuid.toUtf8();  // 先转为 QByteArray
    const char *docId = uuidByteArray.constData();


    error = kb_document_add(m_session, m_KBID, path, docId, NULL);
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
      KyInfo() << "文档上传失败"<< kb_get_error_message(error);
      return false;
    }

    KyInfo() << "文档上传成功 " << error;

    return true;
}

KbDocumentItem KnowledgeBase::getUploadProgress(const QString &docUuid)
{
    int status;
    int error;
    int last_progress = -1;

    KbDocumentItem result;
    result.percent = 0;
    result.uuid = docUuid;

    QByteArray idByteArray = docUuid.toUtf8();
    const char* doc_id = idByteArray.constData();

    KnowledgeBaseInfo *kb_info = kb_knowledgebase_info_get(m_session, m_KBID, &error);
    KyInfo() << "=======kb_info====" << kb_info << error;

    // 检查获取知识库信息是否失败
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
        KyInfo() << "获取知识库信息失败: " << kb_get_error_message(error);
        result.status = DocumentStatus::UPLOAD_ERROR;
        result.errorCode = error;
        return result;
    }

    int progress = kb_document_get_processing_progress(m_session, kb_info,
                                                       doc_id, &status, &error);
    KyInfo() << "current upload progress msg:"<<"doc_id" <<doc_id <<";error code"<< error
             <<";progress:"<<progress <<";status:"<< status << ";m_session:"
             << m_session << ";m_kbinfo:"<<m_KBInfo <<"m_kbID:"<<m_KBID;

    // 处理进度获取的错误
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
        KyInfo() << "获取进度失败: " << kb_get_error_message(error);
        result.status = DocumentStatus::UPLOAD_ERROR;
        result.errorCode = error;
        return result;
    }

    // 处理进度值异常
    if (progress < 0) {
        KyInfo() << kb_get_error_message(error);
        result.status = DocumentStatus::UPLOAD_ERROR;
        result.errorCode = error;
        return result;
    }

    // 更新进度
    if(progress != last_progress) {
        result.percent = progress;
        result.status = DocumentStatus::UPLOADING;
    }

    // 更新状态
    if (status == KB_DOC_STATUS_COMPLETED) {
        result.status = DocumentStatus::UPLOAD_COMPLETE;
        result.percent = 100;
    } else if(status == KB_DOC_STATUS_ERROR) {
        result.status = DocumentStatus::UPLOAD_ERROR;
    } else {
        result.status = DocumentStatus::UPLOADING;
    }

    result.errorCode = error;
    return result;
}

bool KnowledgeBase::deleteDocument(const QString &docPath)
{
    //获取当前已有的知识库文档列表
    int docCount;
    int error;
    char **kb_documents = kb_document_list(m_session,m_KBID,&docCount,&error);
    if (error != static_cast<int>(AI_COMMON_SUCCESS))
    {
        KyInfo() << "获取文档列表失败" << kb_get_error_message(error);
        return false;
    }
    QStringList docList = charArrayToStringList(kb_documents);
    bool isDelete = false;
    KyInfo() << "====about to delete docpath====" << docPath;
    //命中已存在文档后执行删除功能
    for(int i = 0; i < docList.size(); i++)
    {
        if(docList.at(i) == docPath)
        {
            KyInfo() << "====matched====" << docPath;
            QByteArray idByteArray = docPath.toUtf8();  // 先转为 QByteArray
            const char* doc_id = idByteArray.constData();
            KyInfo() << "调用sdk删除知识库starttime=" <<  QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
            error = kb_document_delete(m_session, m_KBID, doc_id);
            KyInfo() << "调用sdk删除知识库endtime====" <<  QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
            if(!error)
            {
                KyInfo() <<doc_id << "删除成功";
                isDelete = true;
            }
            else {
                KyInfo() <<doc_id << "删除失败" << error ;
                isDelete = false;
            }
            break;
        }
    }
    return isDelete;
}

bool KnowledgeBase::deleteDocumentList(const QStringList &uuidList)
{
    int error;
    // 1. 保存真实字符串内存
    QVector<QByteArray> utf8Strings;
    utf8Strings.reserve(uuidList.size());

    // 2. 构造 C 风格数组
    const char** docs = new const char*[uuidList.size()];

    for (int i = 0; i < uuidList.size(); ++i) {
        utf8Strings.append(uuidList[i].toUtf8());
        docs[i] = utf8Strings[i].constData();
    }

    bool isDelete = false;
    int **errorCode = 0;
    KyInfo() << "调用sdk删除知识库starttime=" <<  QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    error = kb_document_delete_multi(m_session, m_KBID, docs, uuidList.size(), errorCode);
    KyInfo() << "调用sdk删除知识库result" << error << "errorCode" << errorCode;
    KyInfo() << "调用sdk删除知识库endtime====" <<  QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    if(!error)
    {
        KyInfo() << "批量删除成功";
        isDelete = true;
    }
    else {
        KyInfo()  << "批量删除失败" << error ;
        isDelete = false;
    }
    return isDelete;
}

bool KnowledgeBase::cancelDocumentAddition(const QString &docUuid)
{
    KyInfo() << "====about to cancel document addition====" << docUuid;
    
    int error;
    QByteArray uuidByteArray = docUuid.toUtf8();  // 先转为 QByteArray
    const char* doc_id = uuidByteArray.constData();
    
    // 接口需要传入文档ID数组，即使只有一个文档也需要是数组形式
    const char* document_ids[] = { doc_id };
    int docs_count = 1;
    
    error = kb_document_cancel_addition(m_session, m_KBID, document_ids, docs_count);
    if (error != static_cast<int>(AI_COMMON_SUCCESS)) {
        KyInfo() << doc_id << "取消上传失败" << error << kb_get_error_message(error);
        return false;
    }
    
    KyInfo() << doc_id << "取消上传成功";
    return true;
}

void KnowledgeBase::sendKBQuestion(QString question)
{
    KyInfo() << "KnowledgeBase sendKBQuestion question: " << question;
    m_isSystemGuideMode = false;  // 设置为普通知识库模式
    int errorCode = kb_chat_stream(m_session, "", m_KBID, question.toUtf8().constData(), m_history, m_config, KBCallback, this);
    if (errorCode != static_cast<int>(AI_COMMON_SUCCESS)){
        qInfo() << "KB sendKBQuestion FAILED!!  " << kb_get_error_message(errorCode);

    }

    // 超时处理已移至前端 QML 端

    return;
}

void KnowledgeBase::kbChatHistoryAddQuestion(QString question)
{
    KyInfo() << "kbChatHistoryAddQuestion=" << question;

    if(m_history != nullptr){
        kb_chat_history_add_question(m_history, question.toUtf8().constData());
    }

    return;
}

void KnowledgeBase::kbChatHistoryAddResponse(QString response)
{
    KyInfo() << "kbChatHistoryAddResponse=" << response;

    if(m_history != nullptr){
        kb_chat_history_add_response(m_history, response.toUtf8().constData());
    }

    return;
}

void KnowledgeBase::clearKBChatHistory()
{
    if(m_history != nullptr){
        kb_chat_history_free(m_history);
    }

    return;
}

void KnowledgeBase::initKBChatHistory()
{
    KyInfo() << "initKBChatHistory";

    m_history = kb_chat_history_new();

    return;
}

void KnowledgeBase::clearSystemGuideHistory()
{
    if(m_systemGuideHistory != nullptr){
        kb_chat_history_free(m_systemGuideHistory);
        m_systemGuideHistory = nullptr;
    }
    return;
}

void KnowledgeBase::initSystemGuideHistory()
{
    KyInfo() << "initSystemGuideHistory";
    if (m_systemGuideHistory == nullptr) {
        m_systemGuideHistory = kb_chat_history_new();
    }
    return;
}

void KnowledgeBase::stop()
{
    // 停止普通知识库会话
    if (m_session) {
        int errorCode = kb_chat_stream_stop(m_session, "");
        if (errorCode != static_cast<int>(AI_COMMON_SUCCESS)){
            qInfo() << "KB kb_chat_stream_stop FAILED!!  " << kb_get_error_message(errorCode);
        }
    }
    // 停止系统指南会话
    stopSystemGuide();
}

void KnowledgeBase::stopSystemGuide()
{
    if (m_systemGuideSession) {
        int errorCode = kb_chat_stream_stop(m_systemGuideSession, "");
        if (errorCode != static_cast<int>(AI_COMMON_SUCCESS)){
            qInfo() << "System Guide KB kb_chat_stream_stop FAILED!!  " << kb_get_error_message(errorCode);
        }
    }
}

bool KnowledgeBase::KBAddQuestion(QString question)
{
    int ret = 0;
    ret = kb_chat_history_add_question(m_history, question.toUtf8().constData());
    if (ret == -1 ) {
        return false;
    }

    return true;
}

bool KnowledgeBase::KBAddResponse(QString response)
{
    int ret = 0;
    ret = kb_chat_history_add_response(m_history, response.toUtf8().constData());
    if (ret == -1 ) {
        return false;
    }

    return true;
}

void KnowledgeBase::clearHistory()
{
    int count = kb_chat_history_get_message_count(m_history);
    for (int i = 0; i < count; ++i ){
        kb_chat_history_delete_question(m_history, i);
        kb_chat_history_delete_response(m_history, i);
    }
}

void KnowledgeBase::setModel(QString model)
{
    kb_chat_config_set_model(m_config, model.toUtf8().constData());
    // 同时设置系统指南的模型配置
    if (m_systemGuideConfig) {
        kb_chat_config_set_model(m_systemGuideConfig, model.toUtf8().constData());
    }
}

int KnowledgeBase::getError()
{
    return m_error;
}

bool KnowledgeBase::isSystemGuideMode() const
{
    return m_isSystemGuideMode;
}

QStringList KnowledgeBase::charArrayToStringList(char **arrayList)
{
    QStringList stringList;
    if (!arrayList) return stringList; // 处理空指针

    // 遍历 C 风格字符串数组（假设以 nullptr 结尾）
    for (int i = 0; arrayList[i] != nullptr; ++i) {
        // 将 const char* 转换为 QString 并添加到列表
        stringList.append(QString::fromUtf8(arrayList[i]));
    }
    return stringList;
}

void KnowledgeBase::run()
{
    QEventLoop loop;

    init();

    loop.exec();

    return;
}

/**
 * @brief 将QStringList转换为const char**和元素个数
 * @param list 输入的QStringList
 * @param doc_ids 输出的C风格字符串数组（调用者需要负责释放内存）
 * @param count 输出的元素个数
 */
void qStringListToCStrings(const QStringList &list, const char ***doc_ids, int *count)
{
    // 校验输入指针有效性
    if (!doc_ids || !count) {
        return;
    }

    *count = list.size();
    if (list.isEmpty()) {
        *doc_ids = nullptr;
        return;
    }

    // 分配指针数组内存
    *doc_ids = new const char*[*count];

    // 将每个QString转换为UTF-8编码的const char*并存储
    for (int i = 0; i < *count; ++i) {
        // 注意：这里使用QByteArray::constData()，QByteArray会自动管理内存
        // 但为了让C风格数组长期有效，我们需要复制一份内存
        QByteArray byteArray = list.at(i).toUtf8();
        char *cStr = new char[byteArray.size() + 1];
        qstrcpy(cStr, byteArray.constData());
        (*doc_ids)[i] = cStr;
    }
}

/**
 * @brief 释放由qStringListToCStrings分配的内存
 * @param doc_ids 需要释放的字符串数组
 * @param count 数组元素个数
 */
void freeCStrings(const char **doc_ids, int count)
{
    if (!doc_ids) {
        return;
    }

    // 先释放每个字符串的内存
    for (int i = 0; i < count; ++i) {
        delete[] doc_ids[i];
    }
    // 再释放指针数组的内存
    delete[] doc_ids;
}

void KnowledgeBase::setDocIds(QStringList docIdList)
{
    if (docIdList.isEmpty()) {
        kb_chat_config_set_doc_ids(m_config, nullptr, 0);

        return;
    }

    const char **doc_ids = nullptr;
    int count = 0;

    // 转换
    qStringListToCStrings(docIdList, &doc_ids, &count);
    kb_chat_config_set_doc_ids(m_config, doc_ids, count);
    // 释放内存
    freeCStrings(doc_ids, count);

    return;
}

// 系统指南相关方法实现
void KnowledgeBase::initForSystemGuide()
{
    bool ret = initSystemGuideSession();
    if (!ret) {
        KyInfo() << "System Guide KB session init failed";
        return;
    }
    
    m_systemGuideHistory = kb_chat_history_new();

    // 创建系统指南专用config
    if (m_systemGuideConfig) {
        kb_chat_config_free(m_systemGuideConfig);
        m_systemGuideConfig = nullptr;
    }
    m_systemGuideConfig = kb_chat_config_new();
    if (m_systemGuideConfig) {
        kb_chat_config_set_diversify_query_enabled(m_systemGuideConfig, false);
    }
    
    KyInfo() << "System Guide KB init SUCCESS";
}

bool KnowledgeBase::initSystemGuideSession()
{
    KyInfo() << "System Guide KB initSession start";
    
    // 确保数据库路径目录存在
    QDir dbDir(SYSTEM_GUIDE_DATABASE_PATH);
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(SYSTEM_GUIDE_DATABASE_PATH)) {
            KyInfo() << "System Guide KB: Failed to create database directory:" << SYSTEM_GUIDE_DATABASE_PATH;
            return false;
        }
    }
    
    m_systemGuideSession = kb_session_create(SYSTEM_GUIDE_APP_ID, SYSTEM_GUIDE_PASSWORD, SYSTEM_GUIDE_DATABASE_PATH, &m_error);
    if (!m_systemGuideSession) {
        KyInfo() << "System Guide KB kb_session_create FAILED";
        KyInfo() << "reason :" << kb_get_error_message(m_error);
        return false;
    }
    KyInfo() << "System Guide KB kb_session_create SUCCESS";
    
    m_error = kb_session_init(m_systemGuideSession);
    if (m_error != static_cast<int>(AI_COMMON_SUCCESS)) {
        KyInfo() << "System Guide KB kb_session_init FAILED";
        KyInfo() << "reason :" << kb_get_error_message(m_error);
        return false;
    }
    KyInfo() << "System Guide KB kb_session_init SUCCESS";
    
    return true;
}

void KnowledgeBase::sendSystemGuideQuestion(QString question)
{
    KyInfo() << "System Guide KB sendSystemGuideQuestion question: " << question;
    m_isSystemGuideMode = true;  // 设置为系统指南模式
    // 知识库Id传空字符串
    int errorCode = kb_chat_stream(m_systemGuideSession, "", "", question.toUtf8().constData(), m_systemGuideHistory, m_systemGuideConfig, KBCallback, this);    if (errorCode != static_cast<int>(AI_COMMON_SUCCESS)){
        qInfo() << "System Guide KB sendSystemGuideQuestion FAILED!!  " << kb_get_error_message(errorCode);
    }
    
    // 超时处理已移至前端 QML 端
    
    return;
}

