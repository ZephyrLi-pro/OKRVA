#include "screensharecapture.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

#include <ukui-log4qt.h>

#ifdef Q_OS_LINUX
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QDBusPendingReply>
#include <QVariantMap>
#include <QGuiApplication>
#include <QScreen>
#include <QScopedPointer> // for QScopedArrayPointer
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/select.h>
#endif

namespace {

QString envUtf8Upper(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed().toUpper();
}

#ifdef Q_OS_LINUX

static bool readImageMetaDataFromPipe(int pipeFd, int &width, int &height,
                                      QImage::Format &format)
{
    int metadataSize = sizeof(width) + sizeof(height) + sizeof(format);
    QScopedArrayPointer<uchar> metadata(new uchar[metadataSize]);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(pipeFd, &readfds);
    struct timeval timeout = {5, 0};
    int ready = select(pipeFd + 1, &readfds, nullptr, nullptr, &timeout);
    if (ready == -1) {
        qWarning() << "select() failed: " << strerror(errno);
        return false;
    }
    if (ready == 0) {
        qWarning() << "select() timeout, no data available";
        return false;
    }

    size_t totalBytesRead = 0;
    while (totalBytesRead < metadataSize) {
        ssize_t bytesRead = read(pipeFd, metadata.data() + totalBytesRead,
                                 metadataSize - totalBytesRead);
        if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                qWarning() << "Error reading metadata: " << strerror(errno);
                return false;
            }
        } else if (bytesRead == 0) {
            qWarning() << "Reached EOF before reading all metadata.";
            return false;
        }
        totalBytesRead += bytesRead;
    }

    uchar *ptr = metadata.data();
    memcpy(&width, ptr, sizeof(width));
    ptr += sizeof(width);
    memcpy(&height, ptr, sizeof(height));
    ptr += sizeof(height);
    memcpy(&format, ptr, sizeof(format));
    return true;
}

static QImage readImageFromPipe(int pipeFd)
{
    int width, height;
    QImage::Format format;
    bool isMetaDataLoaded =
        readImageMetaDataFromPipe(pipeFd, width, height, format);
    if (isMetaDataLoaded == false) {
        qWarning() << "Failed to load image metadata.";
        return QImage();
    }

    size_t dataSize = QImage(width, height, format).sizeInBytes();
    QScopedArrayPointer<uchar> data(new uchar[dataSize]);

    size_t totalBytesRead = 0;
    while (totalBytesRead < dataSize) {
        ssize_t bytesRead = read(pipeFd, data.data() + totalBytesRead,
                                 dataSize - totalBytesRead);
        if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                qWarning() << "Error reading image data: " << strerror(errno);
                return QImage();
            }
        } else if (bytesRead == 0) {
            qWarning() << "Reached EOF before reading all image data.";
            return QImage();
        }
        totalBytesRead += bytesRead;
    }

    return QImage(data.data(), width, height, format).copy();
}

/**
 * 截图实现（统一路径）：org.ukui.kwin.Screenshot / screenshotFullscreen
 * 通过 pipe 直接返回图像数据（metadata + pixels），参考 kylin-ai-memorymap 实现。
 * 适用于所有基于 UKUI KWin 的环境。
 */
voiceassistant::VoiceScreenGrabResult captureViaUkuiKwinScreenshot()
{
    using voiceassistant::VoiceScreenGrabResult;

    VoiceScreenGrabResult out;

    // 使用 org.ukui.kwin.Screenshot 接口的 screenshotFullscreen 方法（通过 pipe 返回像素数据）
    QDBusInterface iface(
        QLatin1String("org.ukui.KWin"),
        QLatin1String("/Screenshot"),
        QLatin1String("org.ukui.kwin.Screenshot"),
        QDBusConnection::sessionBus());

    if (!iface.isValid()) {
        out.errorMessage = QStringLiteral("org.ukui.kwin.Screenshot 接口不可用");
        KyInfo() << "屏幕共享截图：" << out.errorMessage << iface.lastError().message();
        return out;
    }

    // 创建 pipe 用于接收截图数据
    int fd[2];
    if (pipe2(fd, O_CLOEXEC) != 0) {
        out.errorMessage = QStringLiteral("创建 pipe 失败");
        KyInfo() << "屏幕共享截图：" << out.errorMessage;
        return out;
    }

    // 获取主屏索引
    QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    int primaryScreenNum = screens.indexOf(primaryScreen);

    // 调用 screenshotFullscreen(fd, includeCursor, screenNumber)
    // 截图数据将写入 pipe
    QDBusPendingReply<void> reply = iface.call(
        QLatin1String("screenshotFullscreen"),
        QVariant::fromValue(QDBusUnixFileDescriptor(fd[1])),
        false,  // includeCursor = false
        primaryScreenNum);

    ::close(fd[1]); // 关闭写端

    // 读取图像数据（先 metadata: width/height/format，再 pixel data）
    QImage image = readImageFromPipe(fd[0]);
    ::close(fd[0]);

    if (image.isNull()) {
        // 回退到 Qt 原生抓屏（确保至少能发送一个截图，避免因 D-Bus 失败导致卡住）
        KyInfo() << "屏幕共享截图：D-Bus pipe 读取失败，回退到 QScreen::grabWindow";
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            image = screen->grabWindow(0).toImage();
        }
    }

    if (image.isNull()) {
        out.errorMessage = QStringLiteral("screenshotFullscreen 返回空图像且回退抓屏也失败");
        KyInfo() << "屏幕共享截图：" << out.errorMessage;
        return out;
    }

    QPixmap pm = QPixmap::fromImage(image);

    const int maxW = 1280;
    if (pm.width() > maxW) {
        const int h = qMax(1, (pm.height() * maxW) / qMax(1, pm.width()));
        pm = pm.scaled(maxW, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    if (!pm.save(&buffer, "PNG", 9)) {
        out.errorMessage = QStringLiteral("PNG 编码失败");
        KyInfo() << "屏幕共享截图：" << out.errorMessage;
        return out;
    }

    out.pngBase64 = QString::fromLatin1(pngBytes.toBase64()).remove(QLatin1Char('\r'))
                            .remove(QLatin1Char('\n'));
    out.success = true;
    KyInfo() << "屏幕共享截图：已编码为 Base64（宽" << pm.width() << "高" << pm.height() << "）";

    return out;
}

/**
 * 统一入口：使用 screenshotFullscreen pipe 方式获取截图数据（适用于所有 UKUI KWin 环境）。
 */
voiceassistant::VoiceScreenGrabResult captureViaUkuiKwinDBus()
{
    return captureViaUkuiKwinScreenshot();
}
#else
voiceassistant::VoiceScreenGrabResult captureViaUkuiKwinDBus()
{
    voiceassistant::VoiceScreenGrabResult out;
    out.errorMessage = QStringLiteral("非 Linux 平台未集成 UKUI D-Bus 截图");
    return out;
}
#endif

}

namespace voiceassistant {

VoiceAssistantDesktopKind detectDesktopKindForCapture()
{
    const QString current = envUtf8Upper("XDG_CURRENT_DESKTOP");
    const QString session = envUtf8Upper("XDG_SESSION_DESKTOP");

    /* UKUI（麒麟）会话走 KWin 截图：
     * 使用 org.ukui.kwin.Screenshot / screenshotFullscreen（通过 pipe 返回像素数据）。
     */
    if (current.contains(QLatin1String("UKUI")) || session.contains(QLatin1String("UKUI"))) {
        return VoiceAssistantDesktopKind::UkuiKwinDBus;
    }

    /* TODO(X11/Wayland 通用)：根据实际环境检测 KDE/GNOME 等会话并返回对应枚举值 */
    /* TODO(GNOME / Mutter)：Portal 或 dbus org.gnome.Shell — 按需接入 */

    return VoiceAssistantDesktopKind::Unknown;
}

VoiceScreenGrabResult capturePrimaryScreenPngBase64()
{
    VoiceScreenGrabResult result;

    const VoiceAssistantDesktopKind kind = detectDesktopKindForCapture();
    switch (kind) {
    case VoiceAssistantDesktopKind::UkuiKwinDBus:
        return captureViaUkuiKwinDBus();

    default:
        result.success = false;
        result.errorMessage = QStringLiteral("当前会话类型未实现的截图后端");
        KyInfo() << "屏幕共享截图："
                    "当前为非 UKUI 或未识别桌面会话，暂未实现截图（TODO：KDE / GNOME / Qt 兜底等）";
        return result;
    }
}

}
