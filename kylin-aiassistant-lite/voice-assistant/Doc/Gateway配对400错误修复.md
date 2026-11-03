# Gateway 配对 400 Bad Request 问题修复

## 问题描述

语音识别正常，但发送文字请求时报错：

```
voice-assistant no gateway token, requesting new pairing code
voice-assistant failed to generate new pairing code: "Error transferring http://127.0.0.1:42617/admin/paircode/new - server replied: Bad Request"
voice-assistant gateway chat error: "Failed to connect to gateway: Error transferring http://127.0.0.1:42617/admin/paircode/new - server replied: Bad Request"
```

## 根因分析

Gateway 服务端（kybot）在 `require_pairing = false` 时，`POST /admin/paircode/new` 会返回 **HTTP 400 Bad Request**，响应体为：

```json
{
  "success": false,
  "pairing_required": false,
  "pairing_code": null,
  "message": "Pairing is disabled for this gateway"
}
```

voice-assistant 原代码将所有非 `NoError` 的响应都当作网络错误处理，没有读取 400 响应体中的 `pairing_required` 字段，导致即使 Gateway 不要求配对也会报错退出。

**Gateway 服务端相关代码**（`kybot/crates/kylinbot-config/src/pairing.rs`）：

```rust
pub fn generate_new_pairing_code(&self) -> Option<String> {
    if !self.require_pairing {
        return None;  // require_pairing=false 时返回 None
    }
    // ...
}
```

对应 handler 返回 400（`kybot/crates/kylinbot-gateway/src/lib.rs`）：

```rust
None => {
    let body = serde_json::json!({
        "success": false,
        "pairing_required": false,
        "pairing_code": null,
        "message": "Pairing is disabled for this gateway"
    });
    Ok((StatusCode::BAD_REQUEST, Json(body)))
}
```

## 修复方案

修改 `gatewaychatclient.cpp` 的 `requestNewPairingCode()` 函数，调整响应处理顺序：

1. **先读取响应体和 HTTP 状态码**
2. **优先检查 400 状态码**：只有响应体中明确包含布尔值 `pairing_required=false` 时，才跳过配对流程并连接 WebSocket
3. **严格校验字段类型**：避免缺失 `pairing_required` 或字段类型异常时，被 `QJsonValue::toBool()` 默认值误判为无需配对
4. **最后才检查网络错误**

### 修改前

```cpp
if (r->error() != QNetworkReply::NoError) {
    qWarning() << "voice-assistant failed to generate new pairing code:" << r->errorString();
    // ... 直接报错退出
    return;
}

QByteArray data = r->readAll();
// 正常处理...
```

### 修改后

```cpp
QByteArray data = r->readAll();
const int httpStatus = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

// 优先检查 400 响应中的 pairing_required 字段
if (httpStatus == 400 && !data.isEmpty()) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        const QJsonValue pairingRequiredValue = obj.value(QStringLiteral("pairing_required"));
        if (pairingRequiredValue.isBool() && !pairingRequiredValue.toBool()) {
            qInfo() << "voice-assistant pairing not required (from 400 response), proceeding directly";
            m_pairingRequestInFlight = false;
            m_pairingNotRequired = true;
            openGatewayWebSocket();
            return;
        }
    }
}

// 再检查网络错误
if (r->error() != QNetworkReply::NoError) {
    qWarning() << "voice-assistant failed to generate new pairing code:" << r->errorString();
    // ... 报错
    return;
}

// 正常处理 200 响应...
QJsonObject obj = doc.object();
const QJsonValue pairingRequiredValue = obj.value(QStringLiteral("pairing_required"));
if (!pairingRequiredValue.isBool()) {
    qWarning() << "voice-assistant new pairing code response missing pairing_required";
    // ... 报错
    return;
}

const bool pairingRequired = pairingRequiredValue.toBool();
```

## 修改文件

- `voice-assistant/gatewaychatclient.cpp` — `requestNewPairingCode()` 函数
- 新增 `#include <QJsonValue>`，用于显式校验 `pairing_required` 字段类型

## 测试验证

- `git diff --check` 通过。
- `gatewaychatclient.cpp` 单文件编译通过。
- 在 `require_pairing=false` 的 Gateway 环境下测试，语音助手应能正常发送文字请求，不再报 `Bad Request` 错误。
- 对其他 400 JSON 错误响应，如果缺失 `pairing_required` 或该字段不是 bool，不会误判为无需配对。
