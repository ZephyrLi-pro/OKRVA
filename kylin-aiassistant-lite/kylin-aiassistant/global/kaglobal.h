#ifndef KAGLOBAL_H
#define KAGLOBAL_H

#include <QGSettings>
#include <QUrl>

#include <aiconfig/modelconfig.h>
#include <kylin-ai/genai/text/chat.h>
#include <kylin-ai/private/osassistant/osassistant.h>
#include <kylin-ai/coreai/speech/error.h>
#ifdef KB_SUPPORT_ENABLED
#include <kylin-ai/advai/knowledgebase/knowledgebaseinfo.h>
#include <kylin-ai/advai/knowledgebase/types.h>
#include <kylin-ai/advai/knowledgebase/error.h>
#endif
#include <QProcess>
#include <math.h>
#include <ukui-log4qt.h>
#include "configmanager.h"
#include <QDir>
#include <QMimeDatabase>
#include <QMimeType>
#include <QJsonObject>
#include <QJsonArray>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <ukui-log4qt.h>

using namespace kyai::assistant;
using namespace kyai::config::model;

#define TIMEOUT_TIMER_INTERVAL 15000
#define KB_UPDATE_PROGRESS_TIMER_INTERVAL 2000
#define KB_UPLOAD_DIR "/.config/kylin-aiassistant/upload"

// 根据架构定义支持的文件格式列表
#ifdef PDF_SUPPORT_ENABLED
const QStringList g_supportDocFormats = {"txt", "doc", "docx", "pdf", "wps", "pptx", "ppt", "ofd" };
#else
// 申威架构下不支持PDF
const QStringList g_supportDocFormats = {"txt", "doc", "docx", "wps", "pptx", "ppt", "ofd" };
#endif
const QStringList g_supportPicFormats = {"bmp", "jpe", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tiff", "webp"};

// 文件类型
enum FileType
{
    ERROR = 0,
    IMAGE,
    DOCUMENT,
    SOURCE,
    SCHEDULE
};
// 写作下载类型
enum DownloadType
{
    DOWNLOAD_TXT = 0,
    DOWNLOAD_PDF,
    DOWNLOAD_MARKDOWN
};
// 消息去向
enum TartgetType
{
    RECVMSG = 0,
    SUMMARY_TEXT,
    CALENDAR,
    TODOLIST
};
// 页面状态
enum PageType {
    MAIN_PAGE_SIDEBAR_MODE = 0,       // 侧边栏模式下主页面
    SHOW_SIDEBAR,                     // 显示侧边栏
    MEETING_HOME_SIDEBAR_MODE,        // 侧边栏模式下会议助手主页
    MEETING_RECORD_SIDEBAR_MODE,      // 侧边栏模式下会议记录页面
    MAIN_PAGE_WINDOW_MODE,            // 窗口模式下显示主界面
    MEETING_HOME_WINDOW_MODE,         // 窗口模式下显示会议主页
    MEETING_RECORD_WINDOW_MODE        // 窗口模式下显示会议记录页面
};

// 用于后端传递的结构体
struct InnerMessage
{
    QString type = "";                  // 消息类型：文本对话、文生图、工具调用
    QString reasoningContentMsg = "";   // 推理消息
    QString msg = "";                   // 正文消息
    bool is_reasoning_end = false;      // 推理消息结束标志
    bool is_end = false;                // 正文消息结束标志
    int index = 0;                      // 消息序号
    int errorCode = 0;                  // 错误码
    QString model = "";                 // 模型名
    QString feedbackURL;                // 链接地址，用于点赞点踩访问模型主页
    QString extraJson;                  // 扩展消息数据，用于透传智能体工具调用等结构化事件

    // 构造函数（带默认参数）
    InnerMessage(const QString& type_ = "",
                 const QString& reasoningContentMsg_ = "",
                 QString msg_ = "",
                 bool isReasoningEnd_ = false,
                 bool isEnd_ = false,
                 int index_ = 0,
                 int errorCode_ = 0,
                 const QString& model_ = "",
                 const QString& feedbackUrl_ = "",
                 const QString& extraJson_ = "")
        : type(type_),
          reasoningContentMsg(reasoningContentMsg_),
          msg(msg_),
          is_reasoning_end(isReasoningEnd_),
          is_end(isEnd_),
          index(index_),
          errorCode(errorCode_),
          model(model_),
          feedbackURL(feedbackUrl_),
          extraJson(extraJson_)
    {
    }
};

/** KylinBot Gateway WebSocket: InnerMessage.errorCode when no model is configured (task mode friendly UI). */
constexpr int kGatewayInnerErrorNoModelConfigured = -3001;

// 注册结构体
Q_DECLARE_METATYPE(InnerMessage)

/**
 * @brief 文件数据结构体
 */
struct DocumentItem {
    QString documentName = "";       // 文件名称
    QString documentPath = "";       // 文件路径
    qint64 documentSize = 0;         // 文件大小
    QString documentType = "";       // 文件类型
};
Q_DECLARE_METATYPE(DocumentItem)

/**
 * @brief 语气风格结构体
 */
struct ToneStyle {
    QString uuid = "";                              // 唯一标识符
    QString toneName = "";                          // 语气风格名称
    QString toneOriginal = "";                      // 原始示例文本
    QString toneTarget = "";                        // 目标生成文本
    QString toneInstruction = "以前述语气风格为准";    // 语气风格指令
    bool isCustom = false;                          // 是否是自定义项
    QString fileURL = "";                           // 语气风格文档路径

    // 转换为JSON对象
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["uuid"] = uuid;
        obj["toneName"] = toneName;
        obj["toneOriginal"] = toneOriginal;
        obj["toneTarget"] = toneTarget;
        obj["toneInstruction"] = toneInstruction;
        obj["isCustom"] = isCustom;
        obj["fileURL"] = fileURL;
        return obj;
    }

    // 从JSON对象加载
    void fromJson(const QJsonObject& json) {
        uuid = json["uuid"].toString();
        toneName = json["toneName"].toString();
        toneOriginal = json["toneOriginal"].toString();
        toneTarget = json["toneTarget"].toString();
        toneInstruction = json["toneInstruction"].toString();
        isCustom = json["isCustom"].toBool();
        fileURL = json["fileURL"].toString();
    }

    // 重载相等运算符，用于QList操作
    bool operator==(const ToneStyle& other) const {
        return uuid == other.uuid;
    }
};
Q_DECLARE_METATYPE(ToneStyle)

/**
 * @brief 文档模板结构体
 */
struct DocTemplate {
    QString uuid = "";               // 唯一标识符
    QString templateName = "";       // 名称
    QString templateOutline = "";    // 结构大纲（存储为txt文件路径）
    QString templateTone = "";       // 语气风格
    int templateCount = 0;           // 长度(页)
    QString templatePrompt = "";     // 提示词
    QString templateTip = "";        // 模板简介
    QString templateProperty = "";   // 模板属性（一气呵成/步骤写作/逐个填空）
    bool isCustom = false;           // 是否是自定义项
    bool isReference = true;         // 是否启用内容参考
    bool isOutline = true;           // 是否启用结构大纲

    // 转换为JSON对象
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["uuid"] = uuid;
        obj["templateName"] = templateName;
        obj["templateOutline"] = templateOutline;
        obj["templateTone"] = templateTone;
        obj["templateCount"] = templateCount;
        obj["templatePrompt"] = templatePrompt;
        obj["templateProperty"] = templateProperty;
        obj["templateTip"] = templateTip;
        obj["templateProperty"] = templateProperty;
        obj["isCustom"] = isCustom;
        obj["isReference"] = isReference;
        obj["isOutline"] = isOutline;
        return obj;
    }

    // 从JSON对象加载
    void fromJson(const QJsonObject& json) {
        uuid = json["uuid"].toString();
        templateName = json["templateName"].toString();
        templateOutline = json["templateOutline"].toString();
        templateTone = json["templateTone"].toString();
        templateCount = json["templateCount"].toInt();
        templatePrompt = json["templatePrompt"].toString();
        templateProperty = json["templateProperty"].toString();
        templateTip = json["templateTip"].toString();
        templateProperty = json["templateProperty"].toString();
        isCustom = json["isCustom"].toBool();
        isReference = json["isReference"].toBool();
        isOutline = json["isOutline"].toBool();
    }

    // 重载相等运算符，用于QList操作
    bool operator==(const DocTemplate& other) const {
        return uuid == other.uuid;
    }
};
Q_DECLARE_METATYPE(DocTemplate)

/**
 * @brief 文档上传状态枚举
 */
enum DocumentStatus {
    PENDING = 0,     // 待上传
    COPYING,         // 拷贝中
    COPY_ERROR,      // 拷贝失败
    COPY_COMPLETE,   // 拷贝完成
    UPLOADING,       // 上传中
    UPLOAD_ERROR,    // 失败
    UPLOAD_COMPLETE, // 已上传
};

enum KnowledgeUploadError{
    /* 知识库错误码 */
    KB_UPLOAD_SUCCESS = 0,
    KB_ERROR_UPLOAD_DOC_INVALID_FORMAT = 101,             // 格式不支持
    KB_ERROR_UPLOAD_DOC_TOO_LARGE = 102,                  // 文件过大
    KB_ERROR_UPLOAD_LOW_DISK_SPACE = 103,                 // 磁盘空间不足
    KB_ERROR_UPLOAD_LOW_KNOWLEDGE_SPACE = 104,            // 空间容量不足
    KB_ERROR_UPLOAD_DOC_ALREADY_EXISTS = 105,             // 文件已存在
    KB_ERROR_UPLOAD_DOC_NOT_EXISTS = 106,                 // 文件不存在
    KB_ERROR_UPLOAD_DOC_CANNOT_OPEN = 107,                // 文件无法打开
    KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE = 108,     // 拷贝文件无法创建
};


/**
 * @brief 知识库文档数据结构体
 */
struct KbDocumentItem {
    QString uuid = "";               // 唯一标识
    QString documentName = "";       // 文件名称
    QString documentPath = "";       // 文件原路径
    QString documentTargetPath = ""; // 文件存储路径
    qint64 documentSize = 0;       // 文件大小
    QString documentType = "";       // 文件类型
    int status = PENDING; // 文件状态
    int percent = 0;                 // 上传进度（0-100）
    int errorCode = 0;               // 错误码
    QString createTime = "";
    QString documentSizeStr = "";
    bool isDir = false;
    bool checked = false;
    QString fileIcon = "";
    QString statusStr = "";
    int itemId = 0;
};
Q_DECLARE_METATYPE(KbDocumentItem)


struct FileItem {
    QString uuid;
    QString fileName;
    QString filePath;
    QUrl fileIcon;
    int fileProgressNum;
    QString createTime;
    QString fileSize;
    int fileStatus;
    bool isDir;
    bool checked;
    int errorCode;
};

// 页面编号
enum ShowPage {
    CHAT = 1,
    WRITING = 2,
    SPEECH = 3,
    METTING = 4,
    KNOWLEDGE = 5,
    HISTORY = 6,
    SETTING = 7
};

extern bool isChatInitCompleted;
extern bool isSettingOpen;
extern bool isTextAreaFocus;
extern bool isSpeechSynthesizing;
extern bool isWayland;
extern bool isMinimizeClicked;

extern QQmlApplicationEngine *g_qmlEngine;

class Globals : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float fontsize READ fontsize NOTIFY fontsizeChanged FINAL)
    Q_PROPERTY(bool isKBActive READ isKBActive WRITE setIsKBActive NOTIFY isKBActiveChanged FINAL)
    Q_PROPERTY(bool questionModeEnabled READ questionModeEnabled NOTIFY questionModeEnabledChanged FINAL)
    Q_PROPERTY(bool isKnowledgeBaseSupported READ isKnowledgeBaseSupported NOTIFY isKnowledgeBaseSupportedChanged FINAL)
    Q_PROPERTY(bool isPdfSupported READ isPdfSupported NOTIFY isPdfSupportedChanged FINAL)
    Q_PROPERTY(bool isLocalModelSupported READ isLocalModelSupported NOTIFY isLocalModelSupportedChanged FINAL)
    Q_PROPERTY(bool isSubsystemVersionTooLow READ isSubsystemVersionTooLow CONSTANT FINAL)

public:
    float fontsize() const {return Globals::fontSize;}
    bool questionModeEnabled() const { return mQuestionModeEnabled; }
    bool isKnowledgeBaseSupported() const;
    bool isPdfSupported() const;
    bool isLocalModelSupported() const;

    Q_INVOKABLE void sendLog(const QString log);

    static Globals * getinstance();
    Q_INVOKABLE void setVoiceDraftContext(const QString &text) {
        m_voiceDraftTruncated = text.size() > 2000;
        m_voiceDraftContext = text.left(2000);
        if (!m_voiceDraftContext.isEmpty() && m_voiceDraftContext.at(m_voiceDraftContext.size() - 1).isHighSurrogate())
            m_voiceDraftContext.chop(1);
    }
    QString voiceDraftContext() const { return m_voiceDraftContext; }
    bool voiceDraftTruncated() const { return m_voiceDraftTruncated; }
    void setIsKBActive(bool flag);
    bool isKBActive() const {return Globals::isKnowledgeBaseActive;}

    Q_INVOKABLE bool getIsChatInitCompleted(){
        return isChatInitCompleted;
    }
    // 从 GeneralPage 迁移过来的接口
    Q_INVOKABLE void switchToSideMode();
    Q_INVOKABLE void switchToWindowMode();
    Q_INVOKABLE void setCloseWindowMode(bool isHideToTray);
    Q_INVOKABLE QString getCurrentWindowMode();
    Q_INVOKABLE bool getCloseWindowMode();
    Q_INVOKABLE QString selectDefaultSavePath();
    Q_INVOKABLE QString getDefaultSavePath();
    Q_INVOKABLE void setDefaultSavePath(const QString &path);
    Q_INVOKABLE bool checkPathWritable(const QString &path);
    Q_INVOKABLE bool getWindowAlwaysOnTop();
    Q_INVOKABLE bool getQuestionModeEnabled();
    Q_INVOKABLE void setQuestionModeEnabled(bool enabled);
    Q_INVOKABLE bool getScribeEnabled();
    Q_INVOKABLE void setScribeEnabled(bool enabled);
    /// true：选中文本后自动唤起；false：仅通过快捷键等手段唤起（见全局划词设置）
    Q_INVOKABLE bool getScribeAutoInvoke();
    Q_INVOKABLE void setScribeAutoInvoke(bool enabled);
    Q_INVOKABLE bool getSpaceVoiceInputEnabled();
    Q_INVOKABLE void setSpaceVoiceInputEnabled(bool enabled);
    Q_INVOKABLE bool getVoiceCallbackEnabled();
    Q_INVOKABLE void setVoiceCallbackEnabled(bool enabled);
    Q_INVOKABLE void openUkuiControlCenter();
    Q_INVOKABLE void updateWindowModeRadioStatus(const QString &mode);
    bool isSubsystemVersionTooLow() const { return m_isSubsystemVersionTooLow; }

signals:
    void fontsizeChanged(float size);
    //void fontFamilyChanged(QString fontFamily);
    void questionModeEnabledChanged(bool enabled);
    void isKBActiveChanged();
    void sigWindowModeChanged(const QString &mode); // 窗口模式改变信号，用于QML同步状态
    void sigCloseWindowModeChanged(bool isHideToTray); // 关闭窗口时设置改变信号，用于QML同步状态
    void sigDefaultSavePathChanged(const QString &path); // 默认保存路径改变信号，用于QML同步状态
    void sigScribeEnabledChanged(bool enabled); // 划词悬浮条启用状态改变信号，用于QML同步状态
    void sigScribeAutoInvokeChanged(bool autoInvokeEnabled); // 划词唤起方式改变信号（自动 / 快捷键），用于 QML 同步
    void sigSpaceVoiceInputEnabledChanged(bool enabled); // 长按空格语音输入启用状态改变信号，用于QML同步状态
    void sigVoiceCallbackEnabledChanged(bool enabled); // 语音回拨启用状态改变信号，用于QML同步状态
    void isKnowledgeBaseSupportedChanged();
    void isPdfSupportedChanged();
    void isLocalModelSupportedChanged();

public:
    static int themeColor;      // 主題顏色
    static float fontSize;        // 字体大小
    static QString fontFamily;    // 字体名称
    static bool isKnowledgeBaseActive;     //知识库激活状态
    static QUrl openLink;       // 帮助文档链接
    static QString defaultPath; // 助手文件存储路径
    static QString uploadDirPath; //上传知识库路径
    static QString sysMajorVersion; // 系统主版本
    static const QSet<QString> targetMimeTypes;
    static const QSet<QString> targetSuffixes;
    static void borderColor(QWidget *widget,int xradius,int yradius, QColor borderColor);
    static void borderRound(QWidget *widget,int xradius,int yradius);
    static QString getErrorMsg(int errorCode);  //根据文本对话错误码，返回错误信息
    static QString getErrorMsg(SpeechErrorCode errorCode);//根据语音对话错误码，返回错误信息
    static QString getMeetingSummaryErrorMsg(int errorCode); //根据会议助手智能总结错误码，返回错误信息
    static QString getMeetingReloadingErrorMsg(int errorCode); //根据重新生成智能总结错误码，返回错误信息
    static QString getKnowledgeUploadErrorMsg(int errorCode); //根据知识库上传错误码，返回错误信息
    static QString getKnowledgeChatErrorMsg(int errorCode); //根据知识库对话错误码，返回错误信息
    static QString getKnowledgeAssistantErrorMsg(int errorCode); //根据知识库助手错误码，返回错误信息
    static bool getIsSupportWindowMode(); //根据主题版本号判断，是否开启大窗口模式、独占模式
    Q_INVOKABLE static QString getAISubsystemVersion(); //获取AI子系统版本
    Q_INVOKABLE static bool isAISubsystemVersionTooLow(const QString &minVersion = "1.3.0.0-0k0.4"); //检测AI子系统版本是否低于指定版本
    Q_INVOKABLE static bool isAISubsystemVersionCompatLow(); // >=1.3.0.0 且 <1.3.0.0-0k0.4，兼容提示档
    static QString getFileIconName(QString filePath);
    static QString getFileIconPath(QString filePath); // 根据文件路径获取图标路径字符串
    static QString formatFileSize(qint64 bytes, int precision = 2); // 转换文件大小数字格式，默认保留两位小数
    Q_INVOKABLE static QUrl getFileIcon(QString filePath, int size);
    static QString getFileSize(QString filePath);
    bool mQuestionModeEnabled = false;
    bool mSpaceVoiceInputEnabled = true;
    bool mVoiceCallbackEnabled = true;

private:
    explicit Globals(QObject *parent = nullptr);
    static Globals *m_global;
    static bool m_isSubsystemVersionTooLow;
    QString m_voiceDraftContext;
    bool m_voiceDraftTruncated = false;

};

#endif // KAGLOBAL_H
