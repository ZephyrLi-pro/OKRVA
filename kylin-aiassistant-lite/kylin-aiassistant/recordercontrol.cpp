#include "recordercontrol.h"
#include <cmark.h>
#include "global/voiceusingmanager.h"
#include <QDBusConnection>
#include <QDateTime>

/**
 * @brief runtimeSpeechCallback
 *          语音合成返回回调函数，数据流式返回
 * @param result
 * @param usr_data
 */
static void finishedCallback(SpeechRecognitionResult *result, void *user_data)
{
    if(user_data == nullptr) {
            KyInfo() << "Invalid callback user data pointer.";
      }
      int errorCode = speech_recognition_result_get_error_code(result);
      KyInfo() << "ccccccccccc errorCode :" << errorCode;
      const char *result_text = speech_recognition_result_get_text(result);
      KyInfo() << "ccccccccccc result_text :" << result_text;
      int speaker_id = speech_recognition_result_get_speaker_id(result);
      KyInfo() << "ccccccccccc speaker_id :" << speaker_id;
      SpeechResultReason reason = speech_recognition_result_get_reason(result);
      KyInfo() << "ccccccccccc reason :" << reason;

      RecorderControl *recorderControl = (RecorderControl *)user_data;
      //错误码处理
      if(errorCode != 0 && errorCode != -1)
      {
          //百度模型，五秒钟未收到音频会自动断开连接，导致语音通话暂停记录后无法继续使用
          KyInfo() << "voiceChat speech callback error, errorcode :" << errorCode;
          recorderControl->parseAndSendErrorMsg(errorCode);
          //QString errorStr = Globals::getErrorMsg(static_cast<SpeechErrorCode>(errorCode));
          //Q_EMIT recorderControl->reply(errorStr,true,0,errorCode);
          //Q_EMIT recorderControl->sigCancelSendVoice();

          return;
      }


      //发送合成消息显示到前端
      QJsonObject msg;
      msg["msgType"] = "recvChatMsg";
      QJsonObject msgBody;
      msgBody["isEnd"] =  false;
      msgBody["errorCode"] = errorCode;

      if (reason == SPEECH_RECOGNITION_STARTED) {
          // 通知前端识别已就绪，触发 stateSynthesizing → stateRecording 切换（不含识别文本）
          msg["msgBody"] = msgBody;
          Q_EMIT recorderControl->sigReceiveSpeechRecognitionMessage(
              QString::fromUtf8(QJsonDocument(msg).toJson()));
          return;
      }
      if (reason == SPEECH_RECOGNITION_COMPLETED) {
          recorderControl->finishRecognitionStop();
          return;
      }

      // 判断识别状态，未修正语句也显示，未修正的语句在最终进行替换
      if (reason == SPEECH_RECOGNIZING) {
          msgBody["recvMsg"] = result_text;
          msgBody["recognized"] = false;
      } else if (reason == SPEECH_RECOGNIZED) {
          msgBody["recvMsg"] = result_text;
          msgBody["recognized"] = true;
      } else {
          return;
      }
      msg["msgBody"] = msgBody;

      Q_EMIT recorderControl->sigReceiveSpeechRecognitionMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));
      QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
      KyInfo() << "收到识别完成的语音文字时间：" << timeStamp;

      recorderControl->finishRecognitionStopIfRequested(reason);

      return;
}
RecorderControl::RecorderControl(QObject *parent)
    : QObject{parent}
{
    initConnects();
}

RecorderControl::~RecorderControl()
{
    if(m_speechconfig != nullptr){
        AudioConfig **cConfig = &m_speechconfig;
        audio_config_destroy(cConfig);
    }

    closeSpeechSession();
}

void RecorderControl::moveToWorkerThread(QThread *thread)
{
    if (thread == nullptr) {
        KyInfo() << "Invalid thread pointer, cannot move RecorderControl to thread.";
        return;
    }

    m_workerThread = thread;
    
    // 启动线程
    if (!m_workerThread->isRunning()) {
        m_workerThread->start();
    }

    // 将对象移动到工作线程
    this->moveToThread(m_workerThread);

    KyInfo() << "RecorderControl moved to worker thread:" << m_workerThread;
}

void RecorderControl::initConnects()
{
    connect(VoiceUsingManager::getInstance(), &VoiceUsingManager::sigInterruptUsing, this,[&](){
        if(isRecording == false) {
            return;
        }
        KyInfo() << "当前正在执行打断原有录音的流程" << this;
        stopSpeechRecognition();
        closeSpeechSession();
    });

    //插拔输入设备的信号3.1的聲音
    QDBusConnection::sessionBus().connect(QString(), QString("/"), "org.ukui.media", "sourcePortChanged", this,
                                          SLOT(handleInputDeviceChanged()));
}

void RecorderControl::handleInputDeviceChanged()
{
    emit sigCancelSendVoice();

    return;
}

bool RecorderControl::initSpeechSession()
{
    QString error;
    //初始化会话时，对创建成功的会话进行判断并进行指针传递，如果创建失败则返回错误码
    if(this->m_speechSession != nullptr)
        return true;

    SpeechRecognitionSession *speechSession = nullptr;
    speechSession = speech_recognizer_create_session();
    if(speechSession != nullptr)
    {
        KyInfo() << "voice speech_recognizer_create_session successful.";
    }
    else
    {
        KyInfo() << "speech_recognizer_create_session error.";
        Q_EMIT sigCancelSendVoice();
        return false;
    }

    int errorCode = speech_recognizer_init_session(speechSession);
      if(!errorCode)
      {
          KyInfo() << "speech_init_session successful";
      }
      else
      {
          KyInfo() << "speech_init_session error:" << errorCode;
          //error = Globals::getErrorMsg(static_cast<SpeechErrorCode>(errorCode));
          //Q_EMIT reply(error, true, 0, errorCode);
          //Q_EMIT sigCancelSendVoice();
          parseAndSendErrorMsg(errorCode);

          return false;
      }

      if(!errorCode)
      {
          this->m_speechSession = speechSession;
          //设置回调函数
          speech_recognizer_result_set_callback(this->m_speechSession, finishedCallback, this);
      }
      return true;
}

void RecorderControl::closeSpeechSession()
{
    if(this->m_speechSession != nullptr){
        SpeechRecognitionSession **sSession = &m_speechSession;
        speech_recognizer_destroy_session(sSession);
    }
    this->m_speechSession = nullptr;
    isRecording = false;
    m_stopping = false;

    return;
}

void RecorderControl::finishRecognitionStop()
{
    m_stopping = false;
    closeSpeechSession();
    emit sigCancelSendVoice();
}

void RecorderControl::finishRecognitionStopIfRequested(SpeechResultReason reason)
{
    if (m_stopping && reason == SPEECH_RECOGNIZED) {
        finishRecognitionStop();
    }
}

bool RecorderControl::startSpeechRecognition()
{
    if(!initSpeechSession())
    {
        KyInfo() << "startVoiceChat InitSession error!";
        return false;
    }

    //现在直接使用默认麦克风作为数据
    m_speechconfig =audio_config_create_continuous_audio_input_from_default_microphone();
    speech_recognizer_set_audio_config(m_speechSession, m_speechconfig);
    speech_recognizer_start_continuous_recognition_async(m_speechSession);

    m_stopping = false;
    isRecording  = true;
    return true;
}

void RecorderControl::stopSpeechRecognition()
{
    if(m_speechSession) {
        KyInfo() << "结束持续语音识别结果";
        m_stopping = true;
        speech_recognizer_stop_continuous_recognition_async(m_speechSession);
     }
}

void RecorderControl::parseAndSendErrorMsg(const int errorCode)
{
    QString errorMsg = "";
    QJsonObject msg;
    msg["msgType"] = "recvChatMsg";
    QJsonObject msgBody;
    // 检查错误信息是否等于模型未配置
    SpeechErrorCode error = static_cast<SpeechErrorCode>(errorCode);
    if (error == AISDK_MODEL_NOT_FOUND){
        errorMsg = "<span style=\"color:red\">" + tr("There is currently no available voice model available.") + "</span> \n\n" + convertMarkDownToHtml(tr("Configuration steps:\n\n 1. Go to the **System Settings &gt; AI Module Management &gt; Model Configuration** page to add a \"cloud model\", a \"local model\" or a \"optional model\".\n 2. If the model configuration is already completed, please confirm whether the model is enabled. \n\n Note: Running a local model may consume significant device resources. Please ensure your device meets the performance requirements."));
        msgBody["colorMsg"] = "black";
    }else {
        errorMsg = Globals::getErrorMsg(error);
        msgBody["colorMsg"] = "red";
        KyInfo() << "语音通话初始化错误信息："<< errorMsg;
    }

    msgBody["isEnd"] =  true;
    msgBody["sentByUser"]  = false;
    msgBody["reasonMessage"] = "";
    msgBody["modelMsg"] = "::ai_assistant";
    msgBody["model"] = "ai";
    msgBody["recvMsg"] = errorMsg;
    msgBody["recognized"] = true;
    msgBody["errorCode"] = errorCode;

    msg["msgBody"] = msgBody;

    Q_EMIT sigReceiveSpeechRecognitionMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));

    return;
}

QString RecorderControl::convertMarkDownToHtml(const QString &markDown)
{
    // 将markDown格式转换为html格式
    if(markDown.length() < 1){
        return "";
    }else{
        return markdownToHtml(markDown.toStdString());
    }
}

QString RecorderControl::markdownToHtml(const std::string &markdown)
{
    // Convert the markdown text to HTML
    const char* markdownText = markdown.c_str();
    char* html = cmark_markdown_to_html(markdownText, strlen(markdownText), CMARK_OPT_DEFAULT);

    // Handle the conversion result
    std::string htmlStr(html);
    //cmark_mem_free(html);
    QString qStr = QString::fromStdString(htmlStr);

    return qStr;
}

void RecorderControl::interruptAllVoiceUsing()
{
    emit  VoiceUsingManager::getInstance()->sigInterruptUsing(false);// 打断所有的音频占用
}
