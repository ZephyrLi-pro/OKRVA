#ifndef GATEWAYWEBSOCKETMANAGER_H
#define GATEWAYWEBSOCKETMANAGER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "global/kaglobal.h"
#include "unixsocketwebsocket.h"

class GatewayWebSocketManager : public QObject {
    Q_OBJECT
public:
    explicit GatewayWebSocketManager(const QString& sessionId, QObject* parent = nullptr);
    ~GatewayWebSocketManager();

    void sendMessage(const QString& content);
    void cancelResponse();
    bool isConnected() const;
    QString getSessionId() const { return m_sessionId; }

signals:
    void reply(const InnerMessage message);
    void connected(const QString& sessionId);
    void disconnected(const QString& sessionId);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onError(QAbstractSocket::SocketError error);

private:
    UnixSocketWebSocket* m_webSocket = nullptr;
    QString m_sessionId;
    int m_messageIndex;
    QString m_accumulatedChunk;
    QString m_accumulatedThinking;
    bool m_isReplying;
    bool m_expectingReply = false;
    QString m_pendingMessage;
    bool m_pairingInProgress = false;

    void ensureConnected();
    void parseAndEmitMessage(const QString& jsonMessage);
    void saveToken(const QString& token);
    void requestNewPairingCode();
    void doPairWithCode(const QString& pairingCode);
    void performSyncPairing();
    bool validateGatewayToken();
};

#endif

