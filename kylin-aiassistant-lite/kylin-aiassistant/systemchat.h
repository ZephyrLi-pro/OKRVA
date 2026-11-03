#ifndef SYSTEMCHAT_H
#define SYSTEMCHAT_H

#include "global/kaglobal.h"
#include "commandtemplatelist.h"
#include "modelitemlistmodel.h"
#include "./global/documentreader.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
//#include <kylin-ai/private/osassistant/osassistant.h>
#include <string>
#include <aiconfig/modelconfig.h>
#include <string>
#include <aiconfig/modelconfig.h>
#include <QThread>
#include <QEventLoop>
#include <chatdatabase.h>

using namespace kyai::assistant;

class SystemChat : public QThread
{
    Q_OBJECT
public:
    explicit SystemChat(QObject *parent = nullptr);
    ~SystemChat();

    static SystemChat *getPtr();
    void initSystemChat();
	void initCommandList();
    void initWithHistory(std::vector<Message> messages);

    void setActor(int actorId);
    std::vector<Prompt> getPrompts();
    int createCommand(QString name, QString content);
    bool updateCommand(int actorId, QString name, QString content);
    bool deleteCommand(int actorId);
    bool setCommandOrder(QList<CommandTemplateItem> commandTemplateList);

    void stopChat();
    void clearContext();
    void sendMessage(QString text, QString filePath, bool webSearch, bool deepThinking,
                     bool intentRecognition = true,
                     QStringList documentPaths = QStringList(),
                     bool appendDocumentText = false);
    void sendToolMessage(int toolId, FileType fileType, QString para);
    void setSession(int sessionID);
    int getSession();
    bool getFeatureStatus(Features::Feature feature);   //根据AI SDK返回是否支持某种能力

    ///
    /// \brief getNLPModelList  获取当前NLP所有可用模型信息
    /// \return 模型信息列表
    ///
    QList<ModelItem *> getNLPModelList();

    ///
    /// \brief getNLPCurModel  获取当前配置NLP模型名
    /// \return 模型名称
    ///
    QString getNLPCurModel();

    ///
    /// \brief setNLPModel 设置使用NLP模型
    /// \param name 模型名称
    ///
    void setNLPModel(QString name);

protected:
    void run();

signals:
    void reply(const InnerMessage message);
    void sigGenerateSummary(const QString &message, bool is_end, int index, int errorCode, QString model);
    void sigPictureUpdate(const QString &data, const QString &errorMessage);
    void pictureOperateUpdate(const QString &data, const QString &errorMessage);
    void sigInfoSourceUpdate(QMap<int, QString> imageToolMap);
    void sigWebSearchSourceUpdate(QJsonArray searchSourcesArray);  // 联网搜索信息来源
    void toolReply(const QString &message, QMap<int, QString> imageToolMap, QString imageUrl, int errorCode,  QString model, QString feedbackURL);
    void sigSoundMsg(const QString &message, bool is_end, int index);
    void sigGeneratingPic(QString model, QString feedbackURL);
    void sigInitCompleted();
    void sigNLPModelChanged();
    void sigDocumentParseFailed(QString filePath, QString reason);

    /// 在工作线程排队处理 sendMessage（调用线程立即返回，不阻塞）
    void sigEnqueueSend(QString text, QString filePath, bool webSearch, bool deepThinking,
                        bool intentRecognition, QStringList documentPaths,
                        bool appendDocumentText);

    /// 在工作线程触发重新执行 initSystemChat（供初始化失败或待发缓存时排队）
    void sigRetryInit();

private:
    FileType judgeFileType(QString filePath);
    void releaseOsAssistant();
    void sendMessageImpl(QString text, QString filePath, bool webSearch, bool deepThinking,
                         bool intentRecognition, const QStringList &documentPaths,
                         bool appendDocumentText);

    struct PendingChatSend {
        QString text;
        QString filePath;
        QStringList documentPaths;
        bool webSearch = false;
        bool deepThinking = false;
        bool intentRecognition = true;
        bool appendDocumentText = false;
    };

public:
    TartgetType target = RECVMSG;
    bool is_end;
    bool is_reasoning_end;
    QString result_msg;
    int result_index;
    QString text;
    bool soundFlag = false;
    QString reasoningContentMsg;

private:
    OsAssistant *m_osassistant = nullptr;
    std::vector<Prompt> m_promptList;
    Features *m_features = nullptr;
    int m_sessionID;            //助手维护，用于区分会话上下文

    PendingChatSend m_pendingSend;
    bool m_hasPendingSend = false;
    /// 最近一次 init() 结果；构造时为 InitNotCompleted，成功后为 Ok
    Error m_lastInitError;
};

#endif // SYSTEMCHAT_H
