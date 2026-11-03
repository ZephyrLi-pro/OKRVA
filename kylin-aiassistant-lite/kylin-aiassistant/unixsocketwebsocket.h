#ifndef UNIXSOCKETWEBSOCKET_H
#define UNIXSOCKETWEBSOCKET_H

#include <QObject>
#include <QLocalSocket>
#include <QByteArray>
#include <QTimer>
#include <QMap>

struct UnixHttpResponse {
    int statusCode = 0;
    QString errorString;
    QByteArray body;
};

class UnixSocketWebSocket : public QObject
{
    Q_OBJECT
public:
    enum State {
        ClosedState,
        ConnectingState,
        ConnectedState
    };
    Q_ENUM(State)

    explicit UnixSocketWebSocket(QObject* parent = nullptr);
    ~UnixSocketWebSocket();

    void open(const QString& socketPath, const QString& requestPath,
              const QMap<QString, QString>& queryParams,
              const QString& protocol = QString());
    void close();
    qint64 sendTextMessage(const QString& message);
    State state() const { return m_state; }
    QString errorString() const { return m_errorString; }

    void setSocketPath(const QString& path) { m_configuredSocketPath = path; }
    QString socketPath() const;

    UnixHttpResponse httpGet(const QString& path, const QMap<QString, QString>& headers = QMap<QString, QString>());
    UnixHttpResponse httpPost(const QString& path, const QByteArray& body = QByteArray(), const QMap<QString, QString>& headers = QMap<QString, QString>());
    UnixHttpResponse httpDelete(const QString& path, const QMap<QString, QString>& headers = QMap<QString, QString>());

signals:
    void connected();
    void disconnected();
    void textMessageReceived(const QString& message);
    void socketError(QAbstractSocket::SocketError error);

private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketErrorOccurred(QLocalSocket::LocalSocketError error);
    void onSocketDisconnected();
    void onTimeout();

private:
    void sendHandshake();
    void parseHandshakeResponse();
    void readWebSocketFrame();
    bool sendWebSocketFrame(const QString& text);
    void sendControlFrame(int opcode, const QByteArray& payload);

    UnixHttpResponse sendHttpRequest(const QString& method, const QString& path,
                                     const QByteArray& body, const QMap<QString, QString>& headers);

    QLocalSocket* m_socket = nullptr;
    State m_state = ClosedState;
    QString m_socketPath;
    QString m_wsPath;
    QString m_configuredSocketPath;
    QMap<QString, QString> m_queryParams;
    QString m_protocol;
    QByteArray m_readBuffer;
    QString m_key;
    bool m_handshakeDone = false;
    QString m_errorString;
    QTimer* m_timer = nullptr;
};

#endif // UNIXSOCKETWEBSOCKET_H
