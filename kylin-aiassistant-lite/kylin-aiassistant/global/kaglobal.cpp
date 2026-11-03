/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "kaglobal.h"
#include "configmanager.h"
#include "textselect/selectcontrol.h"

#include <QStyleOption>
#include <QStyle>
#include <QBitmap>
#include <QPalette>
#include <QPainter>
#include <QDebug>
#include <QMap>
#include <QJsonObject>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDir>
#include <QProcess>
#include <QBuffer>
#include <QFileIconProvider>
#include <kylin-ai/coreai/speech/error.h>

namespace {
constexpr float kFallbackFontSize = 11.0f;

float loadSystemFontSize()
{
    if (!QGSettings::isSchemaInstalled("org.ukui.style")) {
        return kFallbackFontSize;
    }

    QGSettings settings("org.ukui.style");
    bool ok = false;
    const float fontSize = settings.get("system-font-size").toFloat(&ok);
    return ok && fontSize > 0 ? fontSize : kFallbackFontSize;
}

QString loadSystemFontFamily()
{
    if (!QGSettings::isSchemaInstalled("org.ukui.style")) {
        return QString();
    }

    QGSettings settings("org.ukui.style");
    return settings.get("systemFont").toString();
}
}

bool isChatInitCompleted = false;
bool isSettingOpen = false;
bool isTextAreaFocus = false;           // 输入框是否激活
bool isSpeechSynthesizing = false;      // 是否正在合成语音
bool isWayland = false;
bool isMinimizeClicked = false;         // 记录最小化按钮是否点击


int Globals::themeColor = 0; // 0:为浅色 1:为深色 2:自由模式
float Globals::fontSize = loadSystemFontSize();
QString Globals::fontFamily = loadSystemFontFamily();
bool Globals::isKnowledgeBaseActive = false;  // 延迟初始化，在main.cpp中设置
QUrl Globals::openLink = QUrl(QString("https://docs.openkylin.top/zh/07_%E5%85%B3%E4%BA%8E%E7%A4%BE%E5%8C%BA/"
                                      "%E7%A4%BE%E5%8C%BA%E5%B9%B3%E5%8F%B0%E4%BB%8B%E7%BB%8D/AI%E6%A8%A1%E5%9E%8B%E8%B4%A6%E5%8F%B7%E8%8E%B7%E5%8F%96%E5%8F%8A%E9%85%8D%E7%BD%AE%E6%8C%87%E5%8D%97"));

QString Globals::uploadDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + KB_UPLOAD_DIR;
QString Globals::defaultPath = "";

// 根据架构定义支持的MIME类型集合
#ifdef PDF_SUPPORT_ENABLED
const QSet<QString> Globals::targetMimeTypes = {
    "text/plain",               // txt
    "application/msword",       // doc
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",  // docx
    "application/pdf",          // pdf
    "application/vnd.ms-works", // wps (注意：wps可能有多种MIME类型)
    "application/wps-office.doc", // WPS文档的另一种常见MIME类型
    "inode/x-empty", //空txt
    "application/zip",
    "application/vnd.ms-powerpoint",
    "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    "application/encrypted" //加密doc
};
#else
// 申威架构下不支持PDF
const QSet<QString> Globals::targetMimeTypes = {
    "text/plain",               // txt
    "application/msword",       // doc
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",  // docx
    "application/vnd.ms-works", // wps (注意：wps可能有多种MIME类型)
    "application/wps-office.doc", // WPS文档的另一种常见MIME类型
    "inode/x-empty", //空txt
    "application/zip",
    "application/vnd.ms-powerpoint",
    "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    "application/encrypted" //加密doc
};
#endif

// 目标文件后缀集合
#ifdef PDF_SUPPORT_ENABLED
const QSet<QString> Globals::targetSuffixes = {
    "txt",   // txt文件
    "doc",   // Word旧版文件
    "docx",  // Word新版文件
    "pdf",   // PDF文件
    "pptx",   // PPTX文件
    "ppt",   // PPT文件
    "ofd",   // OFD文件
    "wps"    // WPS文件
};
#else
// 申威架构下不支持PDF
const QSet<QString> Globals::targetSuffixes = {
    "txt",   // txt文件
    "doc",   // Word旧版文件
    "docx",  // Word新版文件
    "pptx",   // PPTX文件
    "ppt",   // PPT文件
    "ofd",   // OFD文件
    "wps"    // WPS文件
};
#endif

Globals * Globals::m_global = nullptr;
bool Globals::m_isSubsystemVersionTooLow = false;
void Globals::borderRound(QWidget *widget,int xradius,int yradius){
    if(widget){
        QStyleOption opt;
        opt.initFrom(widget);
        QPainter p(widget);
        widget->style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, widget);//绘制样式

        QBitmap bmp(widget->size());
        bmp.fill();
        QPainter painter(&bmp);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.setRenderHint(QPainter::Antialiasing);

        //此处可修改圆角矩形大小，也可以绘制为其他想要的形状，比如圆形
        painter.drawRoundedRect(bmp.rect(), xradius, yradius);
        //该函数便使得QBitmap上像素为1的区域才显示出来
        widget->setMask(bmp);
    }
}

Globals::Globals(QObject *parent)
{
    // 从配置中初始化追问模式状态
    mQuestionModeEnabled = ConfigManager::getInstance()->getConfigValue("question-mode-enabled", true).toBool();
    // 从配置中初始化长按空格语音输入状态
    mSpaceVoiceInputEnabled = ConfigManager::getInstance()->getConfigValue("space-voice-input-enabled", true).toBool();
    mVoiceCallbackEnabled = ConfigManager::getInstance()->getConfigValue("voice-callback-enabled", true).toBool();
    m_isSubsystemVersionTooLow = isAISubsystemVersionCompatLow();
}

Globals *Globals::getinstance()
{
    if (m_global == nullptr){
        m_global = new Globals;
    }

    return m_global;
}

void Globals::setIsKBActive(bool flag)
{
    Globals::isKnowledgeBaseActive = flag;
    Q_EMIT isKBActiveChanged();

    return;
}



bool Globals::isKnowledgeBaseSupported() const
{
#ifdef KB_SUPPORT_ENABLED
    return true;
#else
    return false;
#endif
}

bool Globals::isPdfSupported() const
{
#ifdef PDF_SUPPORT_ENABLED
    return true;
#else
    return false;
#endif
}

bool Globals::isLocalModelSupported() const
{
#ifdef LOCAL_MODEL_SUPPORT_ENABLED
    return true;
#else
    return false;
#endif
}

void Globals::borderColor(QWidget *widget, int xradius, int yradius, QColor borderColor) {
    if (widget) {
        QStyleOption opt;
        opt.initFrom(widget);
        QPainter p(widget);
        widget->style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, widget);//绘制样式

        QBitmap bmp(widget->size());
        bmp.fill();
        QPainter painter(&bmp);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.setRenderHint(QPainter::Antialiasing);

        //此处可修改圆角矩形大小，也可以绘制为其他想要的形状，比如圆形
        painter.drawRoundedRect(bmp.rect(), xradius, yradius);
        //该函数便使得QBitmap上像素为1的区域才显示出来
        widget->setMask(bmp);
        //绘制外边框
        QPainter borderPainter(widget);
        borderPainter.setRenderHint(QPainter::Antialiasing);
        borderPainter.setPen(QPen(borderColor, 1));
        borderPainter.drawRoundedRect(widget->rect().adjusted(1, 1, -1, -1), xradius, yradius);
    }
}

QString Globals::getErrorMsg(int errorCode)
{
    QString errorMsg;
    kyai::assistant::ErrorCode error = static_cast<kyai::assistant::ErrorCode>(errorCode);
    switch(error)
    {
    case kyai::assistant::ErrorCode::Ok:
        break;
    case kyai::assistant::ErrorCode::NetError:
        KyInfo() << "网络设置错误。";
        errorMsg = QObject::tr("Network connection is unavailable. Please check your network settings and try again. If using a custom model, ensure the request address is correct.");
        break;
    case kyai::assistant::ErrorCode::ServerError:
        KyInfo() << "AI服务错误。";
        errorMsg = QObject::tr("The current service is unavailable. Please restart the system and try again.");
        break;
    case kyai::assistant::ErrorCode::UnAuthorized:
        KyInfo() << "权限限制，鉴权失败，无权访问。";
        errorMsg = QObject::tr("Authentication error. Check whether the model account configuration information is incorrect.");
        break;
    case kyai::assistant::ErrorCode::RequestTooMany:
        KyInfo() << "请求频率过高。";
        errorMsg = QObject::tr("Requests are too frequent, try again later, or reduce the request frequency to avoid this issue.");
        break;
    case kyai::assistant::ErrorCode::RequestFailed:
        KyInfo() << "请求失败。";
        errorMsg = QObject::tr("The request failed, please resend it.");
        break;
    case kyai::assistant::ErrorCode::InputTextLengthInvaild:
        KyInfo() << "输入文本长度超限。";
        errorMsg = QObject::tr("If the length of the input text is exceeded, please re-edit the text before sending.");
        break;
    case kyai::assistant::ErrorCode::TaskTimeout:
        KyInfo() << "任务超时错误。";
        errorMsg = QObject::tr("The task times out, please resend it.");
        break;
    case kyai::assistant::ErrorCode::GenerateImageFailed:
        KyInfo() << "生成图片失败";
        errorMsg = QObject::tr("Failed to generate image, please resend it.");
        break;
    case kyai::assistant::ErrorCode::GenerateImageBlocked:
        KyInfo() << "生成图片阻塞";
        errorMsg = QObject::tr("The image was not approved due to content. Please adjust the prompt and try again.");
        break;
    case kyai::assistant::ErrorCode::ParamError:
        KyInfo() << "参数错误";
        errorMsg = QObject::tr("The current model configuration is incorrect and cannot be used normally. Please check whether the model name and related configuration information are filled in correctly.");
        break;
    case kyai::assistant::ErrorCode::InputTextInvaild:
        KyInfo() << "输入文本无效";
        errorMsg = QObject::tr("The input text is invalid, please modify it and resend it.");
        break;
    case kyai::assistant::ErrorCode::FileNotFound:
        KyInfo() << "文件未找到";
        errorMsg = QObject::tr("The input file is invalid, please confirm the location of the file and resend it.");
        break;
    case kyai::assistant::ErrorCode::IntentServerError:
        KyInfo() << "初始化意图模型失败";
        errorMsg = QObject::tr("Initializing the intent model failed, check the model configuration.");
        break;
    case kyai::assistant::ErrorCode::NlpServerError:
        KyInfo() << "NLP服务错误";
        errorMsg = QObject::tr("Text content is too long, please send in segments or shorten the text and try again.");
        break;
    case kyai::assistant::ErrorCode::VisionServerError:
        KyInfo() << "VISION服务错误";
        errorMsg = QObject::tr("Initializing the vision model failed, please check the model configuration.");
        break;
    case kyai::assistant::ErrorCode::InitNotCompleted:
        KyInfo() << "初始化未完成";
        errorMsg = QObject::tr("Initialization is not completed. Please wait for the initialization to complete and try again.");
        break;
    case kyai::assistant::ErrorCode::InputTextBlocked:
        KyInfo() << "输入错误";
        errorMsg = QObject::tr("If the input content has not passed the review and is suspected of violating the rules, please adjust it and try again.");
        break;
    case kyai::assistant::ErrorCode::OutputTextBlocked:
        KyInfo() << "输出错误";
        errorMsg = QObject::tr("The output content has not been approved, contains sensitive information, and the results cannot be displayed.");
        break;
    case kyai::assistant::ErrorCode::UnknownError:
        KyInfo() << "未知错误";
        errorMsg = QObject::tr("Unknown error, please restart the service or application.");
        break;
    case kyai::assistant::ErrorCode::SystemOperationFailed:
        KyInfo() << "系统操作失败！";
        errorMsg = QObject::tr("The system operation failed to complete. The problem may be due to insufficient model recognition ability or incorrect configuration of the end side model environment. We have listed model performance as an urgent optimization task. Meanwhile, please assist in checking the local configuration environment. Thank you for your cooperation.");
        break;
    case kyai::assistant::ErrorCode::IntentionRecognitionFailed:
        KyInfo() << "意图识别失败";
        errorMsg = QObject::tr("Your request is not recognized, please make sure your entry is clear and accurate.");
        break;
    case kyai::assistant::ErrorCode::NlpModelNotFound:
        KyInfo() << "NLP 模型未配置";
        break;
    case kyai::assistant::ErrorCode::VisionModelNotFound:
        KyInfo() << "视图模型未配置";
        break;
    case kyai::assistant::ErrorCode::FileExceedSizeLimit:
        KyInfo() << "文档过大";
        errorMsg = QObject::tr("The current file exceeds the processing limit (around 20000 words), please reduce the content appropriately and try again.");
        break;
    case kyai::assistant::ErrorCode::FileParseFailed:
        KyInfo() << "文档内容错误，空文档、纯图片文档或加密文档";
        errorMsg = QObject::tr("Unable to process the file. The file type is not supported or the content cannot be parsed.");
        break;
    case kyai::assistant::ErrorCode::CreateScheduleFailed:
        KyInfo() << "新建日程失败";
        errorMsg = QObject::tr("Sorry, I am unable to add a schedule for you as you have not provided information such as a start time, or the information can not be recognized. To successfully add a schedule, please ensure that you provide clear and accurate dates and times.");
        break;
    case kyai::assistant::ErrorCode::CreateToDoListFailed:
        KyInfo() << "生成待办失败";
        errorMsg = QObject::tr("Sorry, I am unable to create a to-do for you as you have not provided information such as a start time, or the information can not be recognized. To successfully create a to-do, please ensure that you provide clear and accurate dates and times.");
        break;
    case kyai::assistant::ErrorCode::FreeAllocatedQuotaExceeded:
        KyInfo() << "免费额度已使用完,对话次数已达上限";
        errorMsg = QObject::tr("Your free trial quota has been exhausted.");
        break;
    case kyai::assistant::ErrorCode::LocalNlpModelNotLoaded:
        KyInfo() << "当前本地模型不可用";
        errorMsg = QObject::tr("The current local model is not available.");
        break;
    case kyai::assistant::ErrorCode::SslCertificateError:
        KyInfo() << "请求时间验证失败，请检查系统时间设置或尝试更换模型后重试。";
        errorMsg = errorMsg = QObject::tr("Network abnormalities may be caused by the following reasons:")
                + "<br>"
                + tr("1.Request time validation failed, please check the system time settings or try changing the model and retry.")
                + "<br>"
                + tr("2.The network is not authenticated, please try network authentication or contact IT.");
        break;
    case kyai::assistant::ErrorCode::ExtractImageContentsFailed:
        KyInfo() << "图片处理提取文字失败。";
        errorMsg = QObject::tr("Extract image contents failed.");
        break;
    case kyai::assistant::ErrorCode::VisionImageGenerationInsufficientLicense:
        KyInfo() << "文生图服务额度不足，请检查当前模型的额度或配置信息后重试。";
        errorMsg = QObject::tr("The text-to-image service quota is insufficient. Please check the current model's quota or configuration information and try again.");
        break;
    default:
        errorMsg = QObject::tr("Undefined error, returned.");
        break;
    }
    return errorMsg;
}

QString Globals::getErrorMsg(SpeechErrorCode errorCode)
{
    QString errorMsg;
    switch (errorCode) {
    case AISDK_NO_ERROR://0
        errorMsg = "";
        break;
    case AISDK_NET_ERROR://1
        errorMsg = QObject::tr("The network connection is not available, please check the network settings and try again.");
        break;
    case AISDK_AUTHENTICATION_FAILURE://2
        errorMsg = QObject::tr("Authentication error. Check whether the model account configuration information is incorrect.");
        KyInfo() << errorMsg;
        break;
    case AISDK_RUNTIME_ERROR://3
        errorMsg = QObject::tr("Runtime error, please restart the service or system and try again.");
        break;
    case AISDK_TOO_MANY_REQUESTS://4
        errorMsg = QObject::tr("Requests are too frequent, try again later, or reduce the request frequency to avoid this issue.");
        break;
    case AISDK_SERVICE_ERROR://5
        errorMsg = QObject::tr("Cloud service error. The current request cannot be completed, check whether the service status is normal and try again.");
        break;
    case AISDK_SERVICE_TIMEOUT://6
        errorMsg = QObject::tr("The service response timed out. The request was not completed in time, please try again.");
        break;
    case AISDK_BAD_REQUEST://7
        errorMsg = QObject::tr("The request parameter or format is incorrect. Check that the parameters are formatted correctly and resubmit the request.");
        break;
    case AISDK_MODEL_RUN_FAILED://8
        errorMsg = QObject::tr("If the local model fails to run and the request cannot be completed, check the model configuration and try again.");
        break;
    case AISDK_MODEL_RUN_TIME_OUT://9
        errorMsg = QObject::tr("The local model ran timed out, please try again later.");
        break;
    case AISDK_MODEL_NOT_FOUND://10
        errorMsg = QObject::tr("There is currently no available voice model. Please add or enable a model under System Settings > AI Module Management > Model Configuration.");
        KyInfo() << "语音类模型未配置";
        break;
    case AISDK_SSL_CERTIFICATE_ERROR://12
        errorMsg = QObject::tr("Network abnormalities may be caused by the following reasons:")
                + "<br>"
                + tr("1.Request time validation failed, please check the system time settings or try changing the model and retry.")
                + "<br>"
                + tr("2.The network is not authenticated, please try network authentication or contact IT.");
        KyInfo() << "SSL证书错误，请求时间验证失败";
        break;
    case SPEECH_RECOGNITION_AUDIO_DATA_SIZE_INVALID://100
        errorMsg = QObject::tr("If the length of the speech recognition audio is too short or too long, please extend or shorten the audio length and submit it again.");
        break;
    case SPEECH_SYNTHESIS_TEXT_LENGTH_INVALID://101
        errorMsg = QObject::tr("If the text length of the speech synthesis text is too long, please send or shorten the text in parts and try again.");
        break;
    case SPEECH_PARAM_INVALID://102
        errorMsg = QObject::tr("The voice model configuration is invalid, check whether the model configuration information is incorrect.");
        break;
    case SPEECH_DEFAULT_MICROPHONE_INVALID://103
        errorMsg = QObject::tr("The microphone was not detected, please connect the device and try again.");
        break;
    case SPEECH_UNKNOWN_ERROR: //104
        errorMsg = QObject::tr("An unknown error has occurred, please try again later.");
        break;
    case SPEECH_UNSUPPORTED_LANGUAGE://105
        errorMsg = QObject::tr("This language is not supported at this time. Please try a different language.");
        break;
    default:
        // 处理不在 SpeechErrorCode 枚举中的错误码
        if (static_cast<int>(errorCode) == static_cast<int>(AI_COMMON_SPEECH_DEFAULT_MICROPHONE_MUTE)) {
            // 未检测到声音输入，设备麦克风可能已静音。请检查后重试。
            errorMsg = QObject::tr("No sound input detected. The device microphone may be muted. Please check and try again.");
        } else {
            errorMsg = QObject::tr("Undefined error, returned");
        }
        break;
    }
    KyInfo() << errorMsg;
    return errorMsg;
}

QString Globals::getMeetingSummaryErrorMsg(int errorCode)
{
    QString errorMsg;
    kyai::assistant::ErrorCode error = static_cast<kyai::assistant::ErrorCode>(errorCode);
    switch(error)
    {
    case kyai::assistant::ErrorCode::Ok:
        break;
    case kyai::assistant::ErrorCode::NetError:
        // 网络错误
        errorMsg = QObject::tr("There is currently no internet connection and it is not possible to generate a summary of the meeting. The original text has been saved to the local computer, you can return to the home page to view, and you can try again after the network is restored.");
        break;
    case kyai::assistant::ErrorCode::UnAuthorized:
        // 鉴权失败
        errorMsg = QObject::tr("Authentication failed, and the meeting summary could not be generated. The original text has been saved to the local computer, you can return to the home page to view, please check the relevant configuration of the large language model, and try again later.");
        break;
    case kyai::assistant::ErrorCode::NlpModelNotFound:
        // 模型未配置错误
        errorMsg = QObject::tr("There is currently no available text model, so the meeting summary cannot be generated. The original text has been saved locally, and you can return to the homepage to view it. Please check the text model configuration and try again.");
        break;
    case kyai::assistant::ErrorCode::FreeAllocatedQuotaExceeded:
        // 免费试用账号用尽提示
        errorMsg = "<span style=\"color:red\">" + QObject::tr("Your free trial quota has been exhausted and we are unable to generate a meeting summary. The original text has been saved locally and can be viewed on the homepage.") + "</span><br>" + QObject::tr("To ensure the normal trial of the meeting summary, please go to System Settings &gt; AI Module Management &gt; Model Configuration to configure the model.");
        break;
    default:
        // 其他类型错误，目前采用通用文案
        errorMsg = QObject::tr("The build failed. The original article has been saved to the local computer, you can return to the home page to view it, and try again later.");
        break;
    }
    return errorMsg;
}

QString Globals::getMeetingReloadingErrorMsg(int errorCode)
{
    QString errorMsg;
    kyai::assistant::ErrorCode error = static_cast<kyai::assistant::ErrorCode>(errorCode);
    switch(error)
    {
    case kyai::assistant::ErrorCode::Ok:
        break;
    case kyai::assistant::ErrorCode::NetError:
        // 网络错误
        errorMsg = QObject::tr("The network connection is not available, please check the network settings and try again.");
        break;
    case kyai::assistant::ErrorCode::UnAuthorized:
        // 鉴权失败
        errorMsg = QObject::tr("Generation failed. Please check the configuration related to the large language model.");
        break;
    case kyai::assistant::ErrorCode::NlpModelNotFound:
        // 模型未配置
        errorMsg = QObject::tr("Generation failed. Please check the configuration related to the large language model.");
        break;
    case kyai::assistant::ErrorCode::FreeAllocatedQuotaExceeded:
        // 模型未配置
        errorMsg = QObject::tr("Generation failed. Please check the configuration related to the large language model.");
        break;
    default:
        // 其他类型错误，目前采用通用文案
        errorMsg = QObject::tr("The build failed, please try again later.");
        break;
    }
    return errorMsg;
}

QString Globals::getKnowledgeUploadErrorMsg(int errorCode)
{
    QString errorMsg = QObject::tr("unknown error");
    switch (errorCode) {
    case static_cast<int>(AI_COMMON_SUCCESS):
        break;
    //通用报错1-99
    case static_cast<int>(AI_COMMON_INVALID_PARAMETER):
        // 操作未完成
        errorMsg = QObject::tr("Operation not completed");
        break;
    case static_cast<int>(AI_COMMON_PERMISSION_DENIED):
        // 权限不足
        errorMsg = QObject::tr("Insufficient permissions");
        break;
    case static_cast<int>(AI_COMMON_SERVICE_ERROR):
        // 无法响应
        errorMsg = QObject::tr("Unable to respond");
        break;
    case static_cast<int>(AI_COMMON_SERVICE_TIMEOUT):
        // 操作超时
        errorMsg = QObject::tr("Operation timeout");
        break;
    case KB_ERROR_UPLOAD_DOC_INVALID_FORMAT:
        // 格式不支持
        errorMsg = QObject::tr("Format not supported");
        break;
    case KB_ERROR_UPLOAD_DOC_TOO_LARGE:
        // 文件过大
        errorMsg = QObject::tr("File too large");
        break;
    case KB_ERROR_UPLOAD_LOW_DISK_SPACE:
        // 磁盘空间不足
        errorMsg = QObject::tr("Not enough disk space");
        break;
    case KB_ERROR_UPLOAD_LOW_KNOWLEDGE_SPACE:
        // 容量空间不足
        errorMsg = QObject::tr("Insufficient space capacity");
        break;
    case KB_ERROR_UPLOAD_DOC_ALREADY_EXISTS:
        // 文件已存在
        errorMsg = QObject::tr("File already exists");
        break;
    case KB_ERROR_UPLOAD_DOC_NOT_EXISTS:
        // 文件不存在
        errorMsg = QObject::tr("File does not exist");
        break;
    case KB_ERROR_UPLOAD_DOC_CANNOT_OPEN:
        // 文件无法打开
        errorMsg = QObject::tr("The file cannot be opened");
        break;
    case KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE:
        // 拷贝文件无法创建
        errorMsg = QObject::tr("The copy file cannot be created");
        break;
    case static_cast<int>(AI_COMMON_SERVER_INITIALIZATION_FAILED):
        // 初始化失败
        errorMsg = QObject::tr("Initialization failed");
        break;
    case static_cast<int>(AI_COMMON_SERVER_CONNECTION_FAILED):
        // 连接失败
        errorMsg = QObject::tr("Connection failed");
        break;
#ifdef KB_SUPPORT_ENABLED
    // 文档错误 (400-499)
    case static_cast<int>(KB_ERROR_DOC_NOT_FOUND):
        // 文件未找到
        errorMsg = QObject::tr("File not found");
        break;
    case static_cast<int>(KB_ERROR_DOC_TOO_LARGE):
        // 文件过大
        errorMsg = QObject::tr("File too large");
        break;
    case static_cast<int>(KB_ERROR_DOC_INVALID_FORMAT):
        // 格式不支持
        errorMsg = QObject::tr("Format not supported");
        break;
    case static_cast<int>(KB_ERROR_DOC_ACCESS_DENIED):
        // 无法访问
        errorMsg = QObject::tr("Cannot access");
        break;
    case static_cast<int>(KB_ERROR_DOC_CORRUPTED):
        // 文件损坏
        errorMsg = QObject::tr("File corruption");
        break;
    case static_cast<int>(KB_ERROR_DOC_EMPTY):
        // 文件为空
        errorMsg = QObject::tr("The file is empty");
        break;
    case static_cast<int>(KB_ERROR_DOC_ID_INVALID):
        // 文件未找到
        errorMsg = QObject::tr("File not found");
        break;
    case static_cast<int>(KB_ERROR_DOC_ID_NOT_FOUND):
        // 文件未找到
        errorMsg = QObject::tr("File not found");
        break;
    case static_cast<int>(KB_ERROR_DOC_ID_ALREADY_EXISTS):
        // 文件已存在
        errorMsg = QObject::tr("File already exists");
        break;
    case static_cast<int>(KB_ERROR_DOC_BATCH_PARTIAL_FAILED):
        // 上传失败
        errorMsg = QObject::tr("Upload failed");
        break;
    case static_cast<int>(KB_ERROR_DOC_BATCH_ALL_FAILED):
        // 上传失败
        errorMsg = QObject::tr("Upload failed");
        break;
    case static_cast<int>(KB_ERROR_DOC_PARSE_FAILED):
        // 解析失败
        errorMsg = QObject::tr("Parse failed");
        break;
#endif
    // 特殊值（提示场景：所有场景都有可能）
    case static_cast<int>(AI_COMMON_UNKNOWN_ERROR):
        //出现未知问题，请重启应用或稍后重试。
        errorMsg = QObject::tr("An unknown issue has occurred. Please restart the application or try again later.");
        break;
    default:
        KyInfo() << "current error code is default parse.";
        break;
    }

    return errorMsg;
}

QString Globals::getKnowledgeChatErrorMsg(int errorCode)
{
    QString errorMsg;
    switch (errorCode) {
    case static_cast<int>(AI_COMMON_SUCCESS):
        break;
    // 通用报错1-99
    case static_cast<int>(AI_COMMON_INVALID_PARAMETER):
        // 系统遇到问题，请重启应用后重试。
        errorMsg = QObject::tr("The current model configuration is incorrect and cannot be used normally. Please check whether the model name and related configuration information are filled in correctly.");
        break;
    case static_cast<int>(AI_COMMON_PERMISSION_DENIED):
        // 系统权限不足，请稍后重试。
        errorMsg = QObject::tr("Insufficient system permissions, please try again later.");
        break;
    case static_cast<int>(AI_COMMON_SERVICE_ERROR):
        // 知识库无法响应，请重启应用或注销系统后重试。
        errorMsg = QObject::tr("The knowledge base is unable to respond. Please restart the application or log out of the system and try again.");
        break;
    case static_cast<int>(AI_COMMON_SERVICE_TIMEOUT):
        // 知识库无法响应，请重启应用或注销系统后重试。
        errorMsg = QObject::tr("The knowledge base is unable to respond. Please restart the application or log out of the system and try again.");
        break;
    case static_cast<int>(AI_COMMON_SERVER_INITIALIZATION_FAILED):
        // 知识库初始化失败，请重启应用后重试。
        errorMsg = QObject::tr("The initialization of the knowledge base failed. Please restart the application and try again.");
        break;
    case static_cast<int>(AI_COMMON_SERVER_CONNECTION_FAILED):
        // 知识库连接失败，请重启应用或注销系统后重试。
        errorMsg = QObject::tr("The knowledge base connection failed. Please restart the application or log out of the system and try again.");
        break;
    case static_cast<int>(AI_COMMON_SSL_CERTIFICATE_ERROR):
        // 请求时间验证失败，请检查系统时间设置或尝试更换模型后重试。
        errorMsg = QObject::tr("Network abnormalities may be caused by the following reasons:")
                + "<br>"
                + tr("1.Request time validation failed, please check the system time settings or try changing the model and retry.")
                + "<br>"
                + tr("2.The network is not authenticated, please try network authentication or contact IT.");
        break;
    // 网络错误 (100-199)
    case static_cast<int>(AI_COMMON_NET_ERROR):
        // 网络连接不可用，请检查网络设置后重试。
        errorMsg = QObject::tr("Network connection is unavailable. Please check your network settings and try again. If using a custom model, ensure the request address is correct.");
        break;
    case static_cast<int>(AI_COMMON_NETWORK_TIMEOUT):
        // 服务响应超时。请求未能及时完成，请重新尝试。
        errorMsg = QObject::tr("Service response timed out. The request was not completed in a timely manner, please try again.");
        break;
    case static_cast<int>(AI_COMMON_NETWORK_CONNECTION_FAILED):
        // 网络连接不可用，请检查网络设置后重试。
        errorMsg = QObject::tr("Network connection is unavailable. Please check your network settings and try again. If using a custom model, ensure the request address is correct.");
        break;
    case static_cast<int>(AI_COMMON_AUTHENTICATION_FAILURE):
        // 鉴权错误。请检查模型账号配置信息是否有误。
        errorMsg = QObject::tr("Authentication error. Please check if the model account configuration information is incorrect.");
        break;

    // 会话错误 (200-299)
    case static_cast<int>(AISDK_INVALID_SESSION):
        // 知识库创建会话失败，请重启应用后重试。
        errorMsg = QObject::tr("The knowledge base creation session failed. Please restart the application and try again.");
        break;
#ifdef KB_SUPPORT_ENABLED
    case static_cast<int>(KB_ERROR_APP_ID_INVALID):
        // 应用信息有误，请联系管理员或支持人员处理。
        errorMsg = QObject::tr("The application information is incorrect. Please contact the administrator or support personnel for assistance.");
        break;
    case static_cast<int>(KB_ERROR_PASSWORD_INVALID):
        // 应用信息有误，请联系管理员或支持人员处理。
        errorMsg = QObject::tr("The application information is incorrect. Please contact the administrator or support personnel for assistance.");
        break;

    // 知识库错误 (300-399)
    case static_cast<int>(KB_ERROR_KB_NOT_FOUND):
        // 未找到对应的知识库，请检查名称或确认是否已导入。
        errorMsg = QObject::tr("No corresponding knowledge base found, please check the name or confirm if it has been imported.");
        break;
    case static_cast<int>(KB_ERROR_KB_ACCESS_DENIED):
        // 无访问知识库权限，请联系管理员或检查相关设置。
        errorMsg = QObject::tr("No access to the knowledge base, please contact the administrator or check the relevant settings.");
        break;
    case static_cast<int>(KB_ERROR_KB_CORRUPTED):
        // 该知识库已损坏，无法正常使用，请尝试重新导入。
        errorMsg = QObject::tr("The knowledge base is damaged and cannot be used normally. Please try importing it again.");
        break;
    case static_cast<int>(KB_ERROR_KB_ALREADY_EXISTS):
        // 该知识库已存在，请更换名称后重试。
        errorMsg = QObject::tr("The knowledge base already exists, please change the name and try again.");
        break;
     //检索错误 (500-599)
    case static_cast<int>(KB_ERROR_RETRIEVAL_FAILED):
        // 知识库检索过程中出现问题，请重启应用或注销系统后重试。
        errorMsg = QObject::tr("There was a problem during the knowledge base retrieval process. Please restart the application or log out of the system and try again.");
        break;
    case static_cast<int>(KB_ERROR_RETRIEVAL_NO_RESULTS):
        // 知识库中未找到与您问题相关的内容，请尝试调整提问方式，或补充相关的文件后重试。
        errorMsg = QObject::tr("No content related to your question was found in the knowledge base. Please try adjusting the questioning method or adding relevant files and try again.");
        break;
    case static_cast<int>(KB_ERROR_KB_EMPTY):
        // 当前知识库中无可参考的文件，无法生成有效回答。请添加文件后重试。
        errorMsg = QObject::tr("There are no referenced files in the current knowledge base to generate valid answers. Please add the file and try again.");
        break;
    case static_cast<int>(KB_ERROR_RETRIEVAL_TIMEOUT):
        // 知识库检索超时，请检查网络或稍后重试。
        errorMsg = QObject::tr("Knowledge base retrieval timed out, please check your network or try again later.");
        break;
    case static_cast<int>(KB_ERROR_RETRIEVAL_FILEID_FILTER_INVALID):
        // 所选文件中存在已被删除的文件，请重新选择后再试。
        errorMsg = QObject::tr("There are deleted files in the selected file. Please select again and try again.");
        break;
#endif
    // 聊天错误 (600-699)
    case static_cast<int>(AI_COMMON_MODEL_NOT_FOUND):
        // 当前无可用文本类模型
        KyInfo() << "当前无可用文本类模型";
        break;
    case static_cast<int>(AI_COMMON_NLP_INPUT_INVALID):
        // 对话内容过长，请适当缩减内容后重试。
        errorMsg = QObject::tr("The conversation content is too long. Please shorten the content appropriately and try again.");
        break;
    case static_cast<int>(AI_COMMON_TOO_MANY_REQUESTS):
        // 请求过于频繁，请稍后再试，或减少请求频率以避免此问题。
        errorMsg = QObject::tr("Requests are too frequent. Please try again later or reduce the frequency of requests to avoid this issue.");
        break;
    case static_cast<int>(AI_COMMON_NLP_PARAM_ERROR):
        // 参数错误。
        errorMsg = QObject::tr("If the parameter is incorrect, check the configuration parameters.");
        break;
        // 特殊值（提示场景：所有场景都有可能）
    case static_cast<int>(AI_COMMON_UNKNOWN_ERROR):
        // 出现未知问题，请重启应用或稍后重试。
        errorMsg = QObject::tr("An unknown issue has occurred. Please restart the application or try again later.");
        break;
    case static_cast<int>(AI_COMMON_FREE_ALLOCATED_QUOTA_EXCEEDED):
        // 出现未知问题，请重启应用或稍后重试。
        errorMsg = QObject::tr("Your free trial quota has been exhausted.");
        break;
    default:
        KyInfo() << "current error code is default parse.";
        break;
    }

    return errorMsg;
}

QString Globals::getKnowledgeAssistantErrorMsg(int errorCode)
{
    QString errorMsg = QObject::tr("unknown error");
    KnowledgeUploadError error = static_cast<KnowledgeUploadError>(errorCode);
    switch (error) {
    case KB_UPLOAD_SUCCESS:
        break;
    case KB_ERROR_UPLOAD_DOC_INVALID_FORMAT:
        // 格式不支持
        errorMsg = QObject::tr("Format not supported");
        break;
    case KB_ERROR_UPLOAD_DOC_TOO_LARGE:
        // 文件过大
        errorMsg = QObject::tr("File too large");
        break;
    case KB_ERROR_UPLOAD_LOW_DISK_SPACE:
        // 磁盘空间不足
        errorMsg = QObject::tr("Not enough disk space");
        break;
    case KB_ERROR_UPLOAD_LOW_KNOWLEDGE_SPACE:
        // 容量空间不足
        errorMsg = QObject::tr("Insufficient space capacity");
        break;
    case KB_ERROR_UPLOAD_DOC_ALREADY_EXISTS:
        // 文件已存在
        errorMsg = QObject::tr("File already exists");
        break;
    case KB_ERROR_UPLOAD_DOC_NOT_EXISTS:
        // 文件不存在
        errorMsg = QObject::tr("File does not exist");
        break;
    case KB_ERROR_UPLOAD_DOC_CANNOT_OPEN:
        // 文件无法打开
        errorMsg = QObject::tr("The file cannot be opened");
        break;
    case KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE:
        // 拷贝文件无法创建
        errorMsg = QObject::tr("The copy file cannot be created");
        break;
    default:
        KyInfo() << "current error code is default parse.";
        break;
    }

    return errorMsg;
}

// 提取大版本号（去掉构建号部分，如 "1.2.0.1-0k0.2~8" -> "1.2.0.1"）
static QString extractMajorVersion(const QString &version) {
    // 找到 '-' 或 '~' 的位置，只取前面的主版本号部分
    int dashPos = version.indexOf('-');
    int tildePos = version.indexOf('~');

    QString majorVersion = version;
    if (dashPos > 0) {
        majorVersion = version.left(dashPos);
    } else if (tildePos > 0) {
        majorVersion = version.left(tildePos);
    }

    return majorVersion;
}

// 提取构建号后缀（如 "1.3.0.0-0k0.12" -> "0k0.12"）
static QString extractBuildSuffix(const QString &version) {
    int dashPos = version.indexOf('-');
    if (dashPos < 0 || dashPos >= version.length() - 1) {
        return QString();
    }

    QString suffix = version.mid(dashPos + 1);
    int tildePos = suffix.indexOf('~');
    if (tildePos > 0) {
        suffix = suffix.left(tildePos);
    }

    return suffix;
}

static int extractBuildRevisionNumber(const QString &suffix) {
    if (suffix.isEmpty()) {
        return 0;
    }

    int lastDot = suffix.lastIndexOf('.');
    if (lastDot >= 0 && lastDot < suffix.length() - 1) {
        bool ok = false;
        int num = suffix.mid(lastDot + 1).toInt(&ok);
        if (ok) {
            return num;
        }
    }

    bool ok = false;
    int num = suffix.toInt(&ok);
    return ok ? num : 0;
}

// 比较构建号后缀，如 "0k0.12" >= "0k0.12"
static bool compareBuildSuffix(const QString &curSuffix, const QString &targetSuffix) {
    if (curSuffix.isEmpty() && targetSuffix.isEmpty()) {
        return true;
    }
    if (curSuffix.isEmpty()) {
        return false;
    }
    if (targetSuffix.isEmpty()) {
        return true;
    }

    int curNum = extractBuildRevisionNumber(curSuffix);
    int targetNum = extractBuildRevisionNumber(targetSuffix);
    if (curNum > targetNum) {
        return true;
    }
    if (curNum < targetNum) {
        return false;
    }

    return curSuffix >= targetSuffix;
}

// 对比版本号，if A >= B return true else return false
static bool compareVersion(const QString &cur, const QString target){
    QString curMajor = extractMajorVersion(cur);
    QString targetMajor = extractMajorVersion(target);

    QStringList curParts = curMajor.split('.');
    QStringList targetParts = targetMajor.split('.');

    int maxParts = qMax(curParts.size(), targetParts.size());
    for (int i = 0; i < maxParts; ++i) {
        int curNum = (i < curParts.size()) ? curParts[i].toInt() : 0;
        int targetNum = (i < targetParts.size()) ? targetParts[i].toInt() : 0;

        if (curNum > targetNum) {
            return true;
        } else if (curNum < targetNum) {
            return false;
        }
    }

    return compareBuildSuffix(extractBuildSuffix(cur), extractBuildSuffix(target));
}

QString Globals::getAISubsystemVersion()
{
    QProcess process;
    process.start("dpkg-query", QStringList() << "-W" << "-f=${Version}" << "kylin-ai-subsystem");
    process.waitForFinished(3000);

    if (process.exitCode() == 0) {
        QString version = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        KyInfo() << "AI subsystem version:" << version;
        return version;
    } else {
        KyInfo() << "Failed to get AI subsystem version, package may not be installed";
        return QString();
    }
}

bool Globals::isAISubsystemVersionTooLow(const QString &minVersion)
{
    QString currentVersion = getAISubsystemVersion();

    // 如果获取不到版本，说明未安装，返回 false（由其他逻辑处理未安装的情况）
    if (currentVersion.isEmpty()) {
        KyInfo() << "AI subsystem not installed";
        return false;
    }

    // 提取大版本号用于日志显示
    QString curMajor = extractMajorVersion(currentVersion);
    QString minMajor = extractMajorVersion(minVersion);

    // 使用版本比较函数进行比较（主版本号 + 构建号后缀）
    // compareVersion 返回 true 表示 cur >= target，返回 false 表示 cur < target
    bool isVersionOK = compareVersion(currentVersion, minVersion);

    if (!isVersionOK) {
        KyInfo() << "AI subsystem version too low:" << curMajor << "(full:" << currentVersion << ") < required:" << minMajor;
        return true;
    }

    KyInfo() << "AI subsystem version OK:" << curMajor << "(full:" << currentVersion << ") >= required:" << minMajor;
    return false;
}

bool Globals::isAISubsystemVersionCompatLow()
{
    static const QString kMinMajorVersion = QStringLiteral("1.3.0.0");
    static const QString kMinFullVersion = QStringLiteral("1.3.0.0-0k0.4");

    QString currentVersion = getAISubsystemVersion();
    if (currentVersion.isEmpty()) {
        return false;
    }

    // 主版本号 >= 1.3.0.0 且完整版本 < 1.3.0.0-0k0.4
    bool isMajorOk = compareVersion(currentVersion, kMinMajorVersion);
    bool isFullOk = compareVersion(currentVersion, kMinFullVersion);
    bool isCompatLow = isMajorOk && !isFullOk;

    if (isCompatLow) {
        KyInfo() << "AI subsystem version compat low:" << currentVersion
                 << ", major ok (>=" << kMinMajorVersion << "), full low (<" << kMinFullVersion << ")";
    }

    return isCompatLow;
}

QString Globals::getFileIconName(QString filePath)
{
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForFile(filePath);

    return mimeType.iconName();
}

QString Globals::getFileIconPath(QString filePath)
{
    QFileInfo fileInfo(filePath);
    
    // 对于文件夹，使用 insync-folder.svg
    if (fileInfo.isDir()) {
        return "qrc:/icon/icon/insync-folder.svg";
    }
    
    // 对于文件，根据扩展名选择对应的图标
    QString suffix = fileInfo.suffix().toLower();
    
    if (suffix == "txt") {
        return "qrc:/icon/icon/text-plain.svg";
    } else if (suffix == "doc" || suffix == "docx" || suffix == "wps") {
        return "qrc:/icon/icon/application-msword.svg";
    } else if (suffix == "pdf") {
        return "qrc:/icon/icon/application-x-pdf.svg";
    } else if (suffix == "ppt" || suffix == "pptx") {
        return "qrc:/icon/icon/application-mspowerpoint.svg";
    } else if (suffix == "ofd") {
        return "qrc:/icon/icon/application-ofd.svg";
    } else {
        // 其他文件类型使用默认图标
        return "qrc:/icon/icon/application-x-php.svg";
    }
}

QString Globals::formatFileSize(qint64 bytes, int precision)
{
    KyInfo() << "=======formatFileSize========" << bytes;
    if (bytes <= 0)
        return "0 B";

    static const QStringList units = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    // 初始单位转换
    while (unitIndex < units.size() - 1 && size >= 1024.0) {
        size /= 1024.0;
        unitIndex++;
    }

    // 计算指定精度的四舍五入值
    double roundedSize = std::round(size * std::pow(10, precision)) / std::pow(10, precision);

    // 四舍五入后等于1024则升级单位
    if (unitIndex < units.size() - 1 && qFuzzyCompare(roundedSize, 1024.0)) {
        roundedSize = 1.0;
        unitIndex++;
    }

    // 判断是否为整数
    bool isInteger = qFuzzyCompare(roundedSize, std::round(roundedSize));

    // 整数强制保留1位小数，非整数按指定精度显示
    int displayPrecision = isInteger ? 1 : precision;

    // 格式化数值字符串
    QString formattedSize = QString::number(roundedSize, 'f', displayPrecision);

    // 非整数时移除末尾多余的0
    if (!isInteger) {
        formattedSize.remove(QRegExp("0+$"));
    }

    return QString("%1 %2").arg(formattedSize).arg(units[unitIndex]);
}

QUrl Globals::getFileIcon(QString filePath, int size)
{
    QFileInfo fileInfo(filePath);
    QIcon fileIcon;
    // 对于文件夹，使用 QFileIconProvider 获取图标（原来可以正确显示）
    if (fileInfo.isDir()) {
        QFileIconProvider iconProvider;
        fileIcon = iconProvider.icon(fileInfo);
    } else {
        // 对于文件，使用主题图标获取方式，确保 WPS 等文件类型能正确显示图标
        QString iconPath = "file://" + filePath;
        QString iconName = Globals::getFileIconName(iconPath);
        fileIcon = QIcon::fromTheme(iconName);
    }

    // 如果主题图标不存在，使用默认图标
    if (fileIcon.isNull() || fileIcon.availableSizes().isEmpty()) {
        fileIcon = QIcon::fromTheme("text-x-generic");
    }
    if (fileIcon.isNull()) return QUrl();

    // 1. 将 QIcon 转为 QPixmap（指定尺寸，避免缩放模糊）
    QPixmap pixmap = fileIcon.pixmap(size, size);
    if (pixmap.isNull()) return QUrl();

    // 2. 将 QPixmap 写入 QByteArray（PNG 格式）
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG"); // 保存为 PNG 格式（支持透明，兼容性好）
    buffer.close();

    // 3. 封装为 data: 协议的 QUrl（QML Image 可直接识别）
    return QUrl(QString("data:image/png;base64,%1").arg(QString(byteArray.toBase64())));

}

QString Globals::getFileSize(QString filePath)
{
    QFileInfo fileInfo(filePath);
    qint64 size = fileInfo.size();
    const double KB = 1024.0;
    const double MB = KB * 1024.0;

    double displaySize = 0.0;
    QString unit;

    if (size < static_cast<qint64>(KB)) {
        displaySize = static_cast<double>(size);
        unit = "B";
    } else if (size < static_cast<qint64>(MB)) {
        displaySize = size / KB;
        unit = "KB";
    } else {
        displaySize = size / MB;
        unit = "MB";
    }

    // 与 FileListModel::getFileSizeFromBytes 一致：先保留两位小数，再去掉末尾无意义的 0
    QString sizeStr = QString::number(displaySize, 'f', 2);
    if (sizeStr.endsWith(".00")) {
        sizeStr.replace(".00", ".0");
    } else if (sizeStr.endsWith("0") && sizeStr.contains('.')) {
        sizeStr.chop(1);
    }
    return sizeStr + unit;
}

void Globals::sendLog(const QString log)
{
    KyInfo() << log;

    return;
}

// 从 GeneralPage 迁移过来的接口实现
void Globals::switchToSideMode()
{
    // 更新配置并发出信号通知QML更新状态
    ConfigManager::getInstance()->updateConfig("win-mode", 2);
    emit sigWindowModeChanged("sidebar");
}

void Globals::switchToWindowMode()
{
    // 更新配置并发出信号通知QML更新状态
    ConfigManager::getInstance()->updateConfig("win-mode", 3);
    emit sigWindowModeChanged("window");
}

void Globals::setCloseWindowMode(bool isHideToTray)
{
    ConfigManager::getInstance()->updateConfig("is-hide-tray", isHideToTray);
    emit sigCloseWindowModeChanged(isHideToTray);
}

QString Globals::getCurrentWindowMode()
{
    int winMode = ConfigManager::getInstance()->getConfigValue("win-mode", 3).toInt();
    QString windowMode = (winMode == 3) ? "window" : "sidebar";
    return windowMode;
}

bool Globals::getCloseWindowMode()
{
    bool isHideToTray = ConfigManager::getInstance()->getConfigValue("is-hide-tray", true).toBool();
    return isHideToTray;
}

QString Globals::selectDefaultSavePath()
{
    // 获取当前保存的路径，如果没有则使用下载目录
    QString currentPath = getDefaultSavePath();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).value(0, QDir::homePath());
    }

    // 打开文件夹选择对话框
    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::Directory);
    fileDialog.setOption(QFileDialog::ShowDirsOnly, true);
    fileDialog.setModal(true);
    fileDialog.setDirectory(currentPath);
    fileDialog.setWindowTitle(QObject::tr("Select save path"));

    QString selectedPath;
    if (fileDialog.exec() == QFileDialog::Accepted) {
        QStringList selectedPaths = fileDialog.selectedFiles();
        if (!selectedPaths.isEmpty()) {
            selectedPath = selectedPaths.at(0);
            // 保存选择的路径
//            setDefaultSavePath(selectedPath);
        }
    }

    return selectedPath;
}

bool Globals::checkPathWritable(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    QFileInfo dirInfo(path);
    return dirInfo.isWritable();
}

QString Globals::getDefaultSavePath()
{
    QString defaultPath = ConfigManager::getInstance()->getConfigValue("default-save-path", "").toString();
    if (defaultPath.isEmpty()) {
        // 如果没有配置，返回下载目录
        defaultPath = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).value(0, QDir::homePath());
    }
    return defaultPath;
}

void Globals::setDefaultSavePath(const QString &path)
{
    if (!path.isEmpty()) {
        ConfigManager::getInstance()->updateConfig("default-save-path", path);
        // 发出信号通知QML更新状态
        emit sigDefaultSavePathChanged(path);
    }
}

bool Globals::getWindowAlwaysOnTop()
{
    bool isTopping = ConfigManager::getInstance()->getConfigValue("is-window-mode-topping", false).toBool();
    return isTopping;
}

bool Globals::getQuestionModeEnabled()
{
    bool isEnabled = ConfigManager::getInstance()->getConfigValue("question-mode-enabled", true).toBool();
    return isEnabled;
}

bool Globals::getScribeEnabled()
{
    bool isEnabled = ConfigManager::getInstance()->getConfigValue("is-stroke-word", false).toBool();
    return isEnabled;
}

void Globals::setScribeEnabled(bool enabled)
{
    ConfigManager::getInstance()->updateConfig("is-stroke-word", enabled);
    // 发出信号通知QML更新状态
    emit sigScribeEnabledChanged(enabled);
    SelectControl::getInstance()->refreshGlobalScribeShortcut();
}

bool Globals::getScribeAutoInvoke()
{
    return ConfigManager::getInstance()->getConfigValue("scribe-auto-invoke", true).toBool();
}

void Globals::setScribeAutoInvoke(bool enabled)
{
    ConfigManager::getInstance()->updateConfig("scribe-auto-invoke", enabled);
    emit sigScribeAutoInvokeChanged(enabled);
    SelectControl::getInstance()->refreshGlobalScribeShortcut();
}

void Globals::setQuestionModeEnabled(bool enabled)
{
    // 更新配置
    ConfigManager::getInstance()->updateConfig("question-mode-enabled", enabled);

    // 更新Globals中的状态
    Globals::getinstance()->mQuestionModeEnabled = enabled;

    // 发出信号通知QML更新状态
    emit Globals::getinstance()->questionModeEnabledChanged(enabled);
}

bool Globals::getSpaceVoiceInputEnabled()
{
    bool isEnabled = ConfigManager::getInstance()->getConfigValue("space-voice-input-enabled", true).toBool();
    return isEnabled;
}

void Globals::setSpaceVoiceInputEnabled(bool enabled)
{
    // 更新配置
    ConfigManager::getInstance()->updateConfig("space-voice-input-enabled", enabled);

    // 更新Globals中的状态
    Globals::getinstance()->mSpaceVoiceInputEnabled = enabled;

    // 发出信号通知QML更新状态
    emit Globals::getinstance()->sigSpaceVoiceInputEnabledChanged(enabled);
}

bool Globals::getVoiceCallbackEnabled()
{
    bool isEnabled = ConfigManager::getInstance()->getConfigValue("voice-callback-enabled", true).toBool();
    return isEnabled;
}

void Globals::setVoiceCallbackEnabled(bool enabled)
{
    ConfigManager::getInstance()->updateConfig("voice-callback-enabled", enabled);

    Globals::getinstance()->mVoiceCallbackEnabled = enabled;

    emit Globals::getinstance()->sigVoiceCallbackEnabledChanged(enabled);
}

void Globals::openUkuiControlCenter()
{
    QProcess process;
    QString command = "ukui-control-center";
    QStringList arguments;
    arguments << "-m" << "aisubsystem";
    process.startDetached(command, arguments);
    process.waitForFinished(-1);
    // 检查进程是否正常退出
    if (process.exitStatus() == QProcess::NormalExit) {
        KyInfo() << "ukui-control-center successfully!";
    } else {
        KyInfo() << "Failed  to open ukui-control-center";
    }
}

void Globals::updateWindowModeRadioStatus(const QString &mode)
{
    int winMode = (mode == "window") ? 3 : 2;
    ConfigManager::getInstance()->updateConfig("win-mode", winMode);
    // 发出信号通知QML更新状态
    emit sigWindowModeChanged(mode);
}
