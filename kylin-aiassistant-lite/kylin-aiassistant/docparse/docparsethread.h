#ifndef DOCPARSETHREAD_H
#define DOCPARSETHREAD_H

#include <QObject>
#include <QThread>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QClipboard>
#include <kylin-ai/genai/text/chat.h>
#include <kylin-ai/genai/text/config.h>
#include <kylin-ai/genai/text/error.h>
#include <kylin-ai/genai/text/result.h>

class DocParseThread : public QThread
{
    Q_OBJECT
public:
    explicit DocParseThread(QObject *parent = nullptr);
    ~DocParseThread();

    // 设置模型
    void setModel(const QString &modelName);
    
    // 停止聊天
    void stopChat();

signals:
    void receivedMessage(const QString message);
    void receivedError(const QString error);

public slots:
    void sendMessage(const QString message);

protected:
    void run();

private:
    void initDocParseSession();
    ChatModelConfig *createDocParseConfig();
    void sendChatMsg(const QJsonObject &data);
    void sendStopMsg(const QJsonObject &data);

    ChatModelConfig *mDocParseConfig = nullptr;
    GenAiTextSession *mDocParseSession = nullptr;
};

#endif // DOCPARSETHREAD_H
