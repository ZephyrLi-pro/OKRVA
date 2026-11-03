#ifndef VOICE_ASSISTANT_SCREENSHARECAPTURE_H
#define VOICE_ASSISTANT_SCREENSHARECAPTURE_H

#include <QString>

namespace voiceassistant {

/**
 * 运行时桌面/会话归类，用于选择截图后端。
 * （当前仅接入 UKUI+KWin D-Bus，其它需在 screensharecapture.cpp 中补足。）
 */
enum class VoiceAssistantDesktopKind {
    UkuiKwinDBus,
    Unknown,
};

VoiceAssistantDesktopKind detectDesktopKindForCapture();

/** 主屏幕 PNG（经缩放压缩后）的 Base64 文本；success=false 时 pngBase64 为空。 */
struct VoiceScreenGrabResult {
    bool success = false;
    QString pngBase64;
    QString errorMessage;
};

VoiceScreenGrabResult capturePrimaryScreenPngBase64();

}

#endif
