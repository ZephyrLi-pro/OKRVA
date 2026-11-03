#ifndef KNOWLEDGEBASECHAT_H
#define KNOWLEDGEBASECHAT_H

#include <QObject>
#include <QThread>
#include <QEventLoop>
#include <QDebug>

#include <kylin-ai/advai/knowledgebase/knowledgebaseinfo.h>
#include <kylin-ai/advai/knowledgebase/chatconfig.h>
#include <kylin-ai/advai/knowledgebase/chathistory.h>
#include <kylin-ai/advai/knowledgebase/chatresponse.h>
#include <kylin-ai/advai/knowledgebase/chatresult.h>
#include <kylin-ai/advai/knowledgebase/chatsource.h>
#include <kylin-ai/advai/knowledgebase/error.h>
#include <kylin-ai/advai/knowledgebase/session.h>
#include <kylin-ai/advai/knowledgebase/types.h>
#include <global/kaglobal.h>
#include <chatmessage.h>

class KnowledgeBase : public QThread
{
    Q_OBJECT
public:
    static KnowledgeBase *getPtr();
    void init();
    bool initSession();
    bool initKBID();
    void initHistory();
    void initConfig();
    
    // 系统指南相关方法
    void initForSystemGuide();
    bool initSystemGuideSession();
    void sendSystemGuideQuestion(QString question);

    ///
    /// \brief 获取知识库文档列表
    /// \param docList 用于返回文档列表
    /// \return 返回是否获取成功
    ///
    bool getDocumentList(QStringList& docList);

    ///
    /// \brief 知识库添加文件
    /// \param 文件信息
    /// \return 返回是否上传成功
    ///
    bool addDocument(const QString &docPath);

    bool addDocumentByUuid(const QString &docPath, const QString &uuid);

    ///
    /// \brief 获取上传信息
    /// \return 上传信息
    ///
    KbDocumentItem getUploadProgress(const QString &docUuid);

    ///
    /// \brief 删除文档
    /// \param 删除文档信息
    /// \return
    ///
    bool deleteDocument(const QString &docPath);

    ///
    /// \brief 批量删除文档
    /// \param 批量删除文档信息
    /// \return
    ///
    bool deleteDocumentList(const QStringList &uuidList);

    ///
    /// \brief 取消文档上传
    /// \param docUuid 文档UUID
    /// \return 返回是否取消成功
    ///
    bool cancelDocumentAddition(const QString &docUuid);


    void sendKBQuestion(QString question);
    void kbChatHistoryAddQuestion(QString question); // 知识库历史对话添加问题
    void kbChatHistoryAddResponse(QString response); // 知识库历史对话添加回答
    void clearKBChatHistory(); // 清除知识库历史对话
    void initKBChatHistory(); // 新建知识库历史对话
    void clearSystemGuideHistory(); // 清除系统指南历史对话
    void initSystemGuideHistory(); // 新建系统指南历史对话
    void stop();
    void stopSystemGuide(); // 停止系统指南会话

    bool KBAddQuestion(QString question);
    bool KBAddResponse(QString response);
    void clearHistory();

    void setModel(QString model);
    int getError();
    bool isSystemGuideMode() const;  // 检查是否是系统指南模式

    ///
    /// \brief setDocIds 设置知识库问答范围，仅在给出的文件中找寻答案；如果列表为空，则针对整个知识库进行问答
    /// \param docList  文件路径列表（uuid）
    ///
    void setDocIds(QStringList docIdList);
    
public:
    ///
    /// \brief 字符串数组格式转换
    /// \return
    ///
    QStringList charArrayToStringList(char **arrayList);
protected:
    void run();
private:
    explicit KnowledgeBase(QObject *parent = nullptr);
    ~KnowledgeBase();

Q_SIGNALS:
    void reply(KbChatMessage * msg, int errorCode);

private:
    int m_error;  // 知识库状态，非0即为错误
    KBSession *m_session = nullptr;  // Session ID
    KnowledgeBaseInfo *m_KBInfo = nullptr; // 知识库信息实例
    char *m_KBID = nullptr; // 知识库ID
    KBChatHistory *m_history = nullptr;  // 知识库上下文
    KBChatConfig *m_config = nullptr;    //问答配置信息
    
    // 系统指南相关成员
    KBSession *m_systemGuideSession = nullptr;  // 系统指南Session ID
    KBChatHistory *m_systemGuideHistory = nullptr;  // 系统指南上下文
    KBChatConfig *m_systemGuideConfig = nullptr;    // 系统指南问答配置信息
    bool m_isSystemGuideMode = false;  // 标识当前是否是系统指南模式
};

#endif // KNOWLEDGEBASECHAT_H
