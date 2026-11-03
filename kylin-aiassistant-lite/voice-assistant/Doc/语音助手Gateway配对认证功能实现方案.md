# 语音助手 Gateway 配对认证功能实现方案

## 一、需求来源

| 需求编号 | 描述 |
|---------|------|
| 1 | 将 `~/.kylinbot/config.toml` 中 `[gateway] require_pairing` 从 `false` 恢复为 `true`，启用 Gateway 的配对验证机制 |
| 2 | 语音助手 `GatewayChatClient` 连接 WebSocket 时返回 401 Unauthorized，需要添加 token 权限验证逻辑 |
| 3 | 参考主工程 `GatewayWebSocketManager` 已有的 pairing code → token 认证流程，为 voice-assistant 补齐该能力 |

## 二、改动前现状分析

### 已实现

- 主工程 `kylin-aiassistant/gatewaywebsocketmanager.cpp` 已有完整的 Gateway 配对认证流程：
  - 读取 `ConfigManager` 中保存的 `gateway-token`
  - 无 token 时调用 `/admin/paircode/new` 获取 pairing code
  - 拿到 pairing code 后通过 `X-Pairing-Code` 请求头 POST `/pair` 获取 token
  - token 保存到 `ConfigManager`，连接 WebSocket 时在 URL query 中添加 `&token=xxx`
- `~/.kylinbot/config.toml` 的 `[gateway]` 节包含 `require_pairing` 开关

### 未实现/有问题

1. **voice-assistant 无 token 机制**：`GatewayChatClient::ensureConnected()` 直接构造 WebSocket URL 并连接，URL 中没有 `token` 参数
2. **配置开关被关闭**：`~/.kylinbot/config.toml` 中 `require_pairing = false`，导致 Gateway 不验证 token，语音助手在无 token 的情况下也能连接成功
3. **启用后 401 报错**：一旦 `require_pairing = true`，Gateway 会校验 token，voice-assistant 因不带 token 而收到 401 Unauthorized

**报错信息**：

```
voice-assistant gateway websocket error: "QWebSocketPrivate::processHandshake: Unhandled http status code: 401 (Unauthorized)."
```

### 根因分析

| 组件 | 状态 |
|------|------|
| `GatewayWebSocketManager`（主工程） | ✅ 有完整 token 认证流程 |
| `GatewayChatClient`（voice-assistant） | ❌ 无 token，连接时缺少 `token` query 参数 |
| `~/.kylinbot/config.toml` | ❌ `require_pairing = false`，验证被绕过 |

## 三、设计方案

### 3.1 认证流程

完全参照 `GatewayWebSocketManager` 的实现模式，为 `GatewayChatClient` 补齐 pairing 认证逻辑：

```
voice-assistant 发起连接
  │
  ├─ m_gatewayToken 非空？
  │   ├─ 是 → 构造带 token 的 WebSocket URL → 连接
  │   └─ 否 → requestNewPairingCode()
  │             │
  │             ├─ POST /admin/paircode/new
  │             │   ├─ pairing_required == false → 直接连接
  │             │   ├─ pairing_code 非空 → doPairWithCode(code)
  │             │   └─ pairing_code 为空 → 报错提示用户检查 Gateway 服务
  │             │
  │             └─ doPairWithCode(code)
  │                 ├─ POST /pair (X-Pairing-Code: code)
  │                 │   ├─ 成功 → saveToken(token) → ensureConnected()
  │                 │   └─ 失败 → errorOccurred 信号
  │                 └─
  │
  └─ WebSocket 连接成功 → flushPendingOutbound()
```

### 3.2 Token 持久化

**存储位置**：`~/.config/kylin-aiassistant/settings.ini`

与主工程通过 `ConfigManager::getInstance()->getConfigValue("gateway-token")` 共享同一个配置文件，但 voice-assistant 作为独立进程，不依赖主工程的 `ConfigManager` 单例，直接通过 `QSettings` 读写：

```cpp
// 读 token
QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
return ini.value("gateway-token").toString();

// 写 token
QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
ini.setValue("gateway-token", token);
```

**与主工程的兼容性**：主工程 `ConfigManager` 同样以 `QSettings` 操作 `settings.ini`，两者读写同一 key `gateway-token`，数据格式兼容。

### 3.3 401 处理与 Token 失效重试

当 WebSocket 连接收到 401 时，说明已保存的 token 可能已失效（如 Gateway 重启后 token 清除）。此时需要：

1. 清除 `m_gatewayToken` 内存变量
2. 从 `settings.ini` 中删除 `gateway-token` 条目
3. 下次 `ensureConnected()` 调用时自动走 pairing 流程获取新 token

```cpp
void GatewayChatClient::onSocketError()
{
    const QString socketError = m_webSocket->errorString();
    if (socketError.contains("401")) {
        m_gatewayToken.clear();
        QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
        ini.remove("gateway-token");
    }
    Q_EMIT errorOccurred(mappedSocketErrorMessage());
}
```

### 3.4 架构决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 是否引入 ConfigManager 依赖 | 否 | voice-assistant 是独立子进程，不依赖主工程的 `ConfigManager` 单例；直接用 `QSettings` 读写 `settings.ini` 更轻量 |
| 是否复用 GatewayWebSocketManager 的代码 | 否，重新实现 | 两个类继承体系不同、信号接口不同、错误处理方式不同；强行复用会引入不必要的主工程依赖 |
| Token 在 URL query 传递而非 HTTP Header | 是 | 与 `GatewayWebSocketManager` 保持一致，WebSocket 握手阶段无法设置自定义 Header，只能通过 query 传递 |
| `QNetworkAccessManager` 生命周期 | 类成员 | 配对流程可能多次调用（token 失效重试），保持 `m_networkManager` 为成员变量避免重复创建 |

## 四、文件改动详情

### 4.1 `~/.kylinbot/config.toml`

**改动**：`[gateway] require_pairing = false` → `require_pairing = true`

**目的**：恢复 Gateway 的配对验证机制，要求客户端提供有效 token 才能连接。

### 4.2 `voice-assistant/gatewaychatclient.h`

**新增头文件引用**：

```cpp
#include <QNetworkAccessManager>
#include <QNetworkReply>
```

**新增私有方法**：

```cpp
QString loadGatewayToken() const;       // 从 settings.ini 读取已保存的 token
void saveToken(const QString &token);   // 保存 token 到 settings.ini 和成员变量
void requestNewPairingCode();           // POST /admin/paircode/new 获取配对码
void doPairWithCode(const QString &pairingCode);  // POST /pair 获取 token
```

**新增成员变量**：

```cpp
QNetworkAccessManager *m_networkManager = nullptr;  // HTTP 请求管理器
QString m_gatewayToken;                              // 已保存的 gateway token
```

### 4.3 `voice-assistant/gatewaychatclient.cpp`

#### 4.3.1 新增 #include

```cpp
#include <QNetworkAccessManager>
#include <QNetworkReply>
```

#### 4.3.2 构造函数修改

```cpp
GatewayChatClient::GatewayChatClient(QObject *parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_networkManager(new QNetworkAccessManager(this))   // 新增
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_gatewayToken = loadGatewayToken();   // 新增：启动时加载已保存的 token
    updateGatewaySettings();
    // ... 信号连接不变
}
```

#### 4.3.3 `ensureConnected()` 修改

在原有"已连接则 flush、正在连接则等待"的判断之后，新增 token 检查：

```cpp
void GatewayChatClient::ensureConnected()
{
    if (m_connectedToGateway) {
        flushPendingOutbound();
        return;
    }
    const QAbstractSocket::SocketState state = m_webSocket->state();
    if (state == QAbstractSocket::ConnectingState) {
        return;
    }

    // 新增：如果没有 token，先走 pairing 流程获取 token
    if (m_gatewayToken.isEmpty()) {
        qInfo() << "voice-assistant no gateway token, requesting new pairing code";
        requestNewPairingCode();
        return;
    }

    updateGatewaySettings();
    QNetworkRequest request(m_gatewayUrl);
    request.setRawHeader("Origin", m_gatewayOrigin.toUtf8());
    request.setRawHeader("Sec-WebSocket-Protocol", kChatProtocol.toUtf8());
    qInfo() << "voice-assistant gateway connecting with token" << m_gatewayUrl;
    m_webSocket->open(request);
}
```

#### 4.3.4 `updateGatewaySettings()` 修改

URL query 中新增 `token` 参数：

```cpp
QUrlQuery query;
query.addQueryItem(QStringLiteral("session_id"), m_sessionId);
query.addQueryItem(QStringLiteral("name"), QStringLiteral("voice-assistant"));
if (!m_gatewayToken.isEmpty()) {
    query.addQueryItem(QStringLiteral("token"), m_gatewayToken);
}
url.setQuery(query);
```

#### 4.3.5 `onSocketError()` 修改

新增 401 检测与 token 清除逻辑：

```cpp
void GatewayChatClient::onSocketError()
{
    const QString socketError = m_webSocket->errorString();
    qWarning() << "voice-assistant gateway websocket error:" << socketError;

    // 新增：如果是 401 Unauthorized，清除旧 token 以便下次重新走 pairing 流程
    if (socketError.contains(QStringLiteral("401"))) {
        qInfo() << "voice-assistant got 401, clearing stale token and will re-pair";
        m_gatewayToken.clear();
        QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
        ini.remove(QStringLiteral("gateway-token"));
    }

    Q_EMIT errorOccurred(mappedSocketErrorMessage());
}
```

#### 4.3.6 新增方法实现

**`loadGatewayToken()`**：从 settings.ini 读取已保存的 gateway token。

```cpp
QString GatewayChatClient::loadGatewayToken() const
{
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    return ini.value(QStringLiteral("gateway-token")).toString();
}
```

**`saveToken()`**：将 token 同时保存到内存和 settings.ini。

```cpp
void GatewayChatClient::saveToken(const QString &token)
{
    m_gatewayToken = token;
    QSettings ini(assistantSettingsIniPath(), QSettings::IniFormat);
    ini.setValue(QStringLiteral("gateway-token"), token);
}
```

**`requestNewPairingCode()`**：POST 请求 `/admin/paircode/new`，根据响应判断是否需要配对。

```cpp
void GatewayChatClient::requestNewPairingCode()
{
    const QString toml = readKylinBotToml();
    const QString host = loadHostFromConfigs(toml);
    const int port = loadPortFromConfigs(toml);
    QUrl url(QStringLiteral("http://%1:%2/admin/paircode/new").arg(host).arg(port));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *r = m_networkManager->post(request, QByteArray());
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            qWarning() << "voice-assistant failed to generate new pairing code:" << r->errorString();
            m_pendingMessage.clear();
            Q_EMIT errorOccurred(tr("Failed to connect to gateway: ") + r->errorString());
            return;
        }
        QByteArray data = r->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "voice-assistant invalid new pairing code response";
            m_pendingMessage.clear();
            Q_EMIT errorOccurred(tr("Invalid gateway response, please restart service"));
            return;
        }
        QJsonObject obj = doc.object();
        bool pairingRequired = obj.value(QStringLiteral("pairing_required")).toBool();
        QString code = obj.value(QStringLiteral("pairing_code")).toString();
        if (!pairingRequired) {
            qInfo() << "voice-assistant pairing not required, proceeding directly";
            ensureConnected();
        } else if (code.isEmpty()) {
            qWarning() << "voice-assistant pairing required but no code available";
            m_pendingMessage.clear();
            Q_EMIT errorOccurred(tr("Pairing required but no code available, please check gateway service terminal"));
        } else {
            qInfo() << "voice-assistant new pairing code generated:" << code;
            doPairWithCode(code);
        }
    });
}
```

**`doPairWithCode()`**：使用 pairing code 完成配对，获取 token。

```cpp
void GatewayChatClient::doPairWithCode(const QString &pairingCode)
{
    const QString toml = readKylinBotToml();
    const QString host = loadHostFromConfigs(toml);
    const int port = loadPortFromConfigs(toml);
    QUrl url(QStringLiteral("http://%1:%2/pair").arg(host).arg(port));
    QNetworkRequest request(url);
    request.setRawHeader("X-Pairing-Code", pairingCode.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *r = m_networkManager->post(request, QByteArray());
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            qWarning() << "voice-assistant pairing failed:" << r->errorString();
            m_pendingMessage.clear();
            Q_EMIT errorOccurred(tr("Pairing failed: ") + r->errorString());
            return;
        }
        QByteArray data = r->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "voice-assistant invalid pairing response";
            m_pendingMessage.clear();
            Q_EMIT errorOccurred(tr("Invalid pairing response"));
            return;
        }
        QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("token"))) {
            QString token = obj[QStringLiteral("token")].toString();
            saveToken(token);
            qInfo() << "voice-assistant pairing succeeded, token saved";
            ensureConnected();
        }
    });
}
```

## 五、关键流程图

### 5.1 首次连接流程（无 token）

```
voice-assistant 启动
  │
  ├─ 构造函数: m_gatewayToken = loadGatewayToken() → 空
  │
  ├─ sendMessage() → ensureConnected()
  │   │
  │   ├─ m_gatewayToken.isEmpty() == true
  │   │   └─ requestNewPairingCode()
  │   │       │
  │   │       └─ POST http://127.0.0.1:42617/admin/paircode/new
  │   │           │
  │   │           ├─ pairing_required=true, code="ABCD1234"
  │   │           │   └─ doPairWithCode("ABCD1234")
  │   │           │       │
  │   │           │       └─ POST http://127.0.0.1:42617/pair
  │   │           │           Header: X-Pairing-Code: ABCD1234
  │   │           │           │
  │   │           │           ├─ Response: {"token": "enc2:xxx"}
  │   │           │           │   ├─ saveToken("enc2:xxx")
  │   │           │           │   └─ ensureConnected() → 有 token → 连接成功
  │   │           │           │
  │   │           │           └─ 错误 → errorOccurred 信号
  │   │           │
  │   │           ├─ pairing_required=false → ensureConnected() 直接连接
  │   │           │
  │   │           └─ code 为空 → errorOccurred 信号
  │   │
  │   └─ m_gatewayToken 非空 → 正常连接流程
  │
  └─ WebSocket 连接: ws://127.0.0.1:42617/ws/chat?session_id=xxx&name=voice-assistant&token=enc2:xxx
```

### 5.2 Token 失效重试流程

```
ensureConnected() → 带旧 token 连接
  │
  └─ WebSocket 握手 → 401 Unauthorized
      │
      ├─ onSocketError() 检测到 "401"
      │   ├─ m_gatewayToken.clear()
      │   └─ settings.ini 删除 gateway-token
      │
      └─ 下次 ensureConnected() → m_gatewayToken.isEmpty()
          └─ requestNewPairingCode() → 重新走 pairing 流程
```

## 六、与主工程 `GatewayWebSocketManager` 的对比

| 特性 | `GatewayWebSocketManager`（主工程） | `GatewayChatClient`（voice-assistant） |
|------|--------------------------------------|----------------------------------------|
| Token 存储 | `ConfigManager::getInstance()->getConfigValue("gateway-token")` | `QSettings(assistantSettingsIniPath())` 直接读写 |
| 存储位置 | 同一个 `settings.ini` | 同一个 `settings.ini` |
| HTTP 客户端 | `QNetworkAccessManager m_networkManager` | `QNetworkAccessManager m_networkManager` |
| Pairing 请求 | `POST /admin/paircode/new` | `POST /admin/paircode/new` |
| Pair 请求 | `POST /pair` + `X-Pairing-Code` header | `POST /pair` + `X-Pairing-Code` header |
| Token 传递方式 | URL query `&token=xxx` | URL query `&token=xxx` |
| 401 处理 | 无（主工程暂未实现） | 清除 token + 下次重试 |
| 错误报告 | `InnerMessage` 对象 | `errorOccurred(QString)` 信号 |

## 七、风险与缓解

| 风险 | 严重程度 | 缓解措施 |
|------|---------|---------|
| Gateway 重启后 token 失效 | 低 | `onSocketError()` 检测 401 自动清除 token，下次连接自动重走 pairing |
| Pairing code 获取失败 | 中 | 错误信息通过 `errorOccurred` 信号上报 UI，提示用户检查 Gateway 服务 |
| 主工程和 voice-assistant token 冲突 | 低 | 两者读写同一 `settings.ini` 中的 `gateway-token`，后写入者覆盖，Gateway 支持多个 paired_token |
| `QNetworkAccessManager` 生命周期 | 低 | 作为类成员在构造函数中创建，跟随 `GatewayChatClient` 生命周期 |
| voice-assistant 先于主工程启动 | 中 | 若 `settings.ini` 中尚无 token，voice-assistant 可独立完成 pairing 流程获取 token；主工程后续启动时也能读取 |
