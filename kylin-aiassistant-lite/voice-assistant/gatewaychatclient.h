#ifndef GATEWAYCHATCLIENT_H
#define GATEWAYCHATCLIENT_H

#include <QDir>
#include <QObject>
#include <QString>
#include "unixsocketwebsocket.h"

class GatewayChatClient : public QObject
{
    Q_OBJECT

public:
    explicit GatewayChatClient(QObject *parent = nullptr);
    ~GatewayChatClient();

    void sendMessage(const QString &content);
    void cancelResponse(bool reportError = true);
    void shutdown();

    QString sessionId() const { return m_sessionId; }

    static QString encodeContentWithScreenshotImage(const QString &contentWithoutImageSuffix,
                                                     const QString &pngRawBase64);
    bool isConnected() const;

signals:
    void replyStarted();
    void replyProgress();
    void chunkReceived(const QString &content);
    void thinkingReceived(const QString &content);
    void replyFinished(const QString &fullResponse);
    void errorOccurred(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onSocketError();

private:
    friend class VoiceInputRegression;
    void ensureConnected();
    void openGatewayWebSocket();
    void flushPendingOutbound();
    bool sendPayload(const QString &content);
    void parseMessage(const QString &jsonMessage);
    QString readKylinBotToml() const;
    QString loadHostFromConfigs(const QString &configPath) const;
    int loadPortFromConfigs(const QString &configPath) const;
    QString loadOriginFromConfigs(const QString &host, const QString &configPath, int defaultPort) const;
    QString loadGatewayToken() const;
    void saveToken(const QString &token);
    void clearGatewayToken();
    void requestNewPairingCode();
    void doPairWithCode(const QString &pairingCode);
    QString networkErrorMessage() const;
    QString serviceErrorMessage() const;
    QString mappedSocketErrorMessage() const;
    void emitReplyRoundError(const QString &message);

    UnixSocketWebSocket *m_webSocket = nullptr;
    QString m_sessionId;
    QString m_gatewayToken;
    QString m_pendingMessage;
    QString m_accumulatedChunk;
    bool m_connectedToGateway = false;
    bool m_pairingRequestInFlight = false;
    bool m_pairingNotRequired = false;
    bool m_shuttingDown = false;
    int m_authRetryCount = 0;
    bool m_replyRoundErrorEmitted = false;
};

#endif // GATEWAYCHATCLIENT_H
