
#ifndef CMSGPANE_H
#define CMSGPANE_H

#include "chatdatabase.h"
#include "documentreferencedatabase.h"
#include "systemchat.h"
#include "modelitemlistmodel.h"
#include "followupmanager.h"
#include "tasksessionmanager.h"
#include <kysdk/diagnosetest/libkydatacollect.h>
#include <unistd.h>
#include <string>
#include <cmark.h>
#include <iostream>
#ifdef KB_SUPPORT_ENABLED
#include <kb/knowledgebasechat.h>
#endif

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QDBusUnixFileDescriptor>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include <QProcess>
#include <QPrinter>
#include <QImageWriter>
#include "window/windowcontroller.h"
#ifdef KB_SUPPORT_ENABLED
#include "kb/knowledgebasedatabase.h"
#endif

class Reader;
class Recorder;
class PhotoViewerPlugin;
class FollowUpManager;

#define EMPTYMEETINGFLAG "EmptyMeetingFlag"
#define MAXIMAGESIZE 50 * 1024 * 1024

using namespace kyai::config::model;


class SystemChat;
enum WindowMode
{
    WINDOW_MODE_NORMAL = 0,
    WINDOW_MODE_OCCUPIED =1,
    WINDOW_MODE_SIDEMODE =2,
    WINDOW_MODE_WINDOWMODE=3,
};

enum ActiveModule{
    MODULE_PLUGIN = 0,
    MODULE_CHAT = 1,
    MODULE_MEETING = 2
};

// 定义文档信息结构体
struct DocumentInfo {
    QString documentName = "";
    QString documentSize = "";
    QString documentIcon = "";
    QString documentPath = "";
    QString documentUuid = "";
    bool isDir = false;
    int childCount = 0;
};

class CMsgPane : public QObject
{
    Q_OBJECT
public:
    enum ChatType{
        NORMAL = 0,
        KNOWLEDGEBASE = 1,
        TASK_MODE = 2
    };

    struct InformationSource {
        QString name;
        QString url;
        int id;
    };

    QString getAppNameFromSourceId(int id) const;
    void callSourceUrlTool(int toolId);
    const QString userGuidePathPrefix = "/usr/share/kylin-user-guide/data/guide/";
    const QString kaimingUserGuidePathPrefix = "/opt/system/resource/kylin/kylin-user-guide/data/guide/";
    explicit CMsgPane();
    //报错消息头处理
    void errorMessageToJs(QString msgHead, QString modelMsg, QString colorMsg, QString errorMsg);
    void errorMessageToJsNoModel(QString msgHead, QString modelMsg, QString colorMsg, QString errorMsg);
    void systemMessageToJs(QString msgHead, QString modelMsg, QString colorMsg, QString systemName, QString systemIcon, QString msg, int systemIndex, ChatType chatType);
    void errorMessageToKbJs(QString msgHead, QString modelMsg, QString colorMsg, QString errorMsg, ChatType chatType, QString isModelError = "");// 知识库异常信息发送前端
    void clearJsonMessage();

    WindowMode m_windowMode;
    ActiveModule m_activeModule = ActiveModule::MODULE_CHAT;
    void pushMsg(QString func);
    void pushMsg(QString func, QVariant arg1);
    void pushMsg(QString func, QString jsonMsgHeadToWeb, QString msg, bool convertMarkdown = true);
    bool getIsEnd();
    void showTip(QString tipMessage);

public slots:
    bool getIsReplying();
    // 切换模型
    void onSetCloudModel(AiCapability capability, QString modelName);
    bool isCurrentModelAvailable();
    void onEnterMeetingRecordPage(bool entered);
    void onSwitchToPlugin(QString page);
    //消息
    void onClearContext();
    void onClearKBContext(); // 调用知识库清除知识库上下文
    void setDeepThinking(bool checked); // 设置深度思考
    void setWebSearch(bool checked); // 设置联网搜索
    QString onSendMsg(const QString &jsonMsgHead, const QString &msg);
    void onRecvMsg(const InnerMessage message);
    void onRecvTaskMsg(const QString& sessionUuid, const InnerMessage message);
    void saveBackendMessageToDatabase(const QString& sessionUuid, const QString& message, const InnerMessage& msgInfo);
    void clearBackendRecvCache(const QString& sessionUuid);
    void onAddQuestion(const QStringList &questions);
    Q_INVOKABLE void onAudioRecvMsg(QString msg, bool is_end, int index, int errorCode, QString model);         //处理语音类问题报错
    void onRecvTool(QString msg, QMap<int, QString> imageToolMap, QString imagePath, int errorCode, QString model, QString feedbackURL);
    void onSaveMessageList();
    void onLoadMessageList();
    void onChangedIsReplyingStatus(bool isReplying);
    //知识库文档问答
#ifdef KB_SUPPORT_ENABLED
    void onKBRecvMsg(KbChatMessage * msg, int errorCode);
#else
    void onKBRecvMsg(void * msg, int errorCode);
#endif
    //图像
    void onSelectFile(int typeNum);
    void onShowImage(const QString &imagePath, int counts);
    QString onPictureOperateClick(int toolId, const QString &imagePath);
    void onImageDragIn(bool dragin);
    void onShowErrorMoreImage();
    void onShowErrorSizeImage();
    void onShowErrorFormatImage();
    void updateToShowPic(QString imageData, QString errorMessage); // 更新图片处理返回的图片
    void updateToShowOperatePic(QString imageData, QString errorMessage); // 更新图像处理返回的图片
    //文档
    void onShowDocument(const QString &filePath);
    void onShowErrorFormatFile();
    void onShowErrorFormat();
    //文生图
    void onSaveHTML2PDF(const QString &base64Image);
    //系统助手-信息来源
    void updateToShowSource(QMap<int, QString> infoSourceMap); // 更新系统助手信息来源
    void updateToShowWebSearchSource(QJsonArray searchSourcesArray); // 更新联网搜索信息来源
    void onSelectInfoSource(int toolId); // 跳转到信息来源
    //埋点数据
    void onSendDataCollect(const QString &eventCode, const QString &pageName);
    Q_INVOKABLE void onSendDataCollectWithProps(const QString &eventCode, const QString &pageName, const QVariantMap &customProps);
    //会话
    void saveDialog();
    void clearDialog();
    void onShowText(const QString text);
    void onShowSpeech(const QString text);
    void onAppendSpeech(const int speakerID, const QString text);
    void loadPlugin(QString pluginHtmlFile);
    Q_INVOKABLE void openSchedule(QString todoListJson);  //通过D-Bus接口打开日历，todoListJson为待办列表JSON字符串，日期从待办列表中解析
    Q_INVOKABLE void addTodoToSchedule(QString todoListJson);  //通过D-Bus接口将待办添加到日程，todoListJson为待办列表JSON字符串
    Q_INVOKABLE QString getTodoList(QString todoMessage);

    //语音听写
    void onClearDictation();
    //复制成功
    void onCopySuccess();
    void onCopyText(const QString &text);
    void onCopyImage(const QString &base64Image);
    // 过滤角标并复制文本（供 QML 调用）
    Q_INVOKABLE void copyTextFiltered(const QString &text);
    //点赞点踩
    void onLikeAction();
    //设置会议助手重新生成功能正在运行中
    void onsigReloadIsRunning();
    void onReloadIsRunning();
    //重新初始化会议语音类模型和会议nlp
    void onReInitMeetingNlpSpeech();
    //发送会议录音
    void onSendMeetingAudio();
    void onPauseMeetingAudio();
    void onContinueMeetingAudio();
    void onStopMeetingAudio();
    //刷新会议文件列表
    void flushMeetingFileList();
    //处理会议原文
    void processMessageList(QString jsonStr);
    void onGenerateSummary(QString msg, bool is_end, int index, int errorCode, QString model, const QString &reasoningContentMsg);
    void saveMeetingMsgToDataBase(int msgIndex, QString message);
    void saveMeetingInfoToDataBase(int msgIndex,QString summary, QString fileName, QString filePath);
    void deleteMeetingHistoryFromDataBase(const int meetingID);
    void deleteMeetingHistoryFromDataBase(const QString meetingName);
    QString  getActiveMeetingName();
    void onMeetingRunningPrevent(bool isShutDown);//会议助手运行限制
    void clearMeetingUsingStatus();

    void onStopReply();
    void updateStateWithChat();
    void getMeetingList();
    void openMeetingFile(QString path);
    void openMeetingFolder(QString path);
    int xdgOpen(QString path);
    void xdgOpenForImageOperate(QString base64Image,int operatorId, QString imagePath);
    void reloadMeetingSummary(QString name);
    void saveToDatabase(int msgInex, QString message);  //保存消息进数据库
    void updateLastMessage(int msgInex, QString message); // 更新最后一条数据的message到数据库
    void updateFeedbackDatabase(int msgInex, QString message);

    void updateIsCollectBySessionId(QString sessionId, int isCollect);
    void updateIsCollectBySessionIdList(const QVariantList &sessionIds, int isCollect); // 根据sessionId列表更新收藏状态
    void loadHistory();
    void loadHistoryBySessionId(QString sessionId);
    void loadTaskSessionHistory(const QString& sessionId);

    void getPluginList();
    void getDialogueList();
    void modifyDialogueList(const QVariantList &strings);
    void closeSession();
    Q_INVOKABLE void deactivateTaskSession();
    Q_INVOKABLE int getSessionModeType(const QString& sessionId);
    int getNextHistoryId();
    int getCurrentMaxHistoryId();
    Q_INVOKABLE bool getTaskSessionIsReplying(const QString& sessionUuid);
    Q_INVOKABLE int getTaskSessionCount() const;
    void deleteSessionList(const QVariantList &strings);
    Q_INVOKABLE void deleteMessageByIndex(int msgIndex); // 删除指定索引的消息
    Q_INVOKABLE void requestDeleteMessage(int msgIndex);
    Q_INVOKABLE void clearCurrentSessionDocumentReference(); // 清除当前会话的文档引用
    Q_INVOKABLE QString extractDocumentReferenceFromHistory(const QString &sessionId); // 从数据库读取文件引用信息

    int getCurWinMode(){return m_windowMode;}
    QString convertMarkDownToHtml(const QString &markDown);
    ActiveModule getCurrentActiveModule(){return m_activeModule;}

    //文档处理
    void openDocument(const QString &documentPath); // 打开文档
    bool isSupportDocumentQA(); //返回是否支持文档问答
    bool isSupportHtml2Docx(); //返回是否支持周报html格式转docx格式

    //图片处理
    bool isSupportImageProcessing(); //返回是否支持图片问答

    int getCurrentSessionId();

    // 会议助手正在运行，语音对话不可用报错提示
    void onMeetRuningVoiceUnableError();
    void activateMainWindow();  // 触发信号
    void updateDialogueListSessionId();
    void onStopSpeak(); //停止朗读
    Q_INVOKABLE void stopVoiceInputIfActive(); // 停止语音输入，如果正在输入
    void openUkuiControlCenter();
    Q_INVOKABLE void openUkuiUpgrade();
    Q_INVOKABLE void openSoftwareCenter(); //打开软件商店
    Q_INVOKABLE void setWindowAlwaysOnTop(bool checked); //设置窗口置顶
    Q_INVOKABLE void showHelp(); //显示帮助手册
    Q_INVOKABLE void showFeedBack(); //显示意见反馈
    Q_INVOKABLE void showAbout(); //显示关于窗口
    Q_INVOKABLE void startVoiceChat(); //启动独立 voice-assistant 进程
    Q_INVOKABLE void stopVoiceChat();  //通知 voice-assistant 自行退出
    Q_INVOKABLE QString getConfigValue(const QString &key); //获取配置值
    Q_INVOKABLE void updateConfig(const QString &key, const QVariant &value); //更新配置值
    Q_INVOKABLE void syncGatewayPairing();
    // 验证 gateway-token 有效性；失效时自动重新配对并返回 true
    Q_INVOKABLE bool validateAndRefreshGatewayToken();
    void onStartReplyingToChat();
    void onChatInited();
    ///
    /// \brief 获取会议助手当前选择的Speech模型
    /// \return
    ///
    QString getMeetingSpeechCurModel();


    ///
    /// \brief initNLPModelList  初始化NLP模型可用列表，也可用于刷新
    ///
    void initNLPModelList();


    ///
    /// \brief getNLPCurModel 获取当前选择的NLP模型，以config为准，存在runtime和config不一致的情况
    /// \return 模型名
    ///
    QString getNLPCurModel();

    ///
    /// \brief setNLPModel 设置NLP模型，下次对话时生效。
    /// \param name 模型名
    ///
    void setNLPModel(QString name);

    void initNLPModel(); // 程序初始化设置模型

    ///
    /// \brief stopLoadTimer 30s加载计时停止
    ///
    void stopLoadTimer();

    bool getIsSidebarClose();
    void setIsSidebarClose(bool flag);

    ///
    /// \brief setWindowProperty 调用窗管接口，为qml页面设置阴影和圆角
    /// \param win 窗口handle
    ///
    void setWindowProperty(QObject *win);

    void setKnowledgeBaseStatus(bool isActive);


    Q_INVOKABLE void stopFollowUpRequest(); // 停止追问请求（供前端调用）
    int getKnowledgeFileCountByFilepath(const QString filePath);
    QStringList getAllFileIdsByFilepath(const QString filePath);
    void handleSendFile(const QString filePath);
    int getTextBit(const QString text);
    bool isContainDir(const QVariantList &strings);  // 判断传入的路径里面是否包含文件夹

    bool getIsIntroductionVisible();
    void setIsIntroductionVisible(bool flag);
    int getHistoryCount();
    bool getIsShowHistoryMaxTip();
    void setIsShowHistoryMaxTip(bool flag);
    void deleteTheEarliestHistory();

    // 供划词窗口调用：应用一个基于指令模板系统的指令（通过 uuid 标识），并通知前端更新选中指令状态
    Q_INVOKABLE void applyCommandFromSelection(const QString &uuid);
    // 划词进入助手时切换到快问快答主页
    Q_INVOKABLE void enterFromWordSelection();

public:
    int m_operatorId = -1; // 图像处理操作id
    // 录音/朗读时限制锁屏和睡眠（供 QML 或 main.cpp 信号槽调用）
    Q_INVOKABLE void recordingInhibit();
    Q_INVOKABLE void recordingUnInhibit();
    void readingInhibit();
    void readingUnInhibit();

private:
    void createAllChildWnd();
    void initSolts();
    void initChatOperator();
    void initPluginList();
    void initCommandList();
    void initLoadingTimer();
    void initFollowUpManager();
    void watchMeetingRecordDir();

    void saveDialogToFile(const QString &dialogJsonString);
    void saveMessageToFile(const QString &msgJsonString);
    void saveMeetingToFile(QString model);
    void saveEmptyMeetingToFile();//保存空的记录到文件中
    bool meetingInhibit();
    void meetingUnInhibit();
    QDBusInterface *m_meetingInhibitLockScreen = nullptr;
    QDBusInterface *m_meetingInhibitShutdown = nullptr;
    QDBusMessage meetingShutdownReply;
    QDBusMessage meetingLockScreenReply;
    QDBusMessage recordingLockScreenReply;  // 录音抑制 cookie
    QDBusMessage readingLockScreenReply;   // 朗读抑制 cookie
    QDBusUnixFileDescriptor m_inhibitFileDescriptor;  // 用于解除抑制的文件描述符
    void buildFollowUpRequest();
    QString getBase64FromImagePath(const QString &imagePath); // 从图片路径获取到Base64编码
    bool isCommunity();
    QString markdownToHtml(const std::string& markdown);
    bool checkChatModel(int errorCode);
    static QMap<QString, InformationSource> getInformationSources(); // 声明获取映射表的静态函数（区分编译宏）
    InformationSource getInformationSourceFromUserGuidePath(const QString &path); // 声明路径解析函数

    QTimer *m_loadTimer = nullptr;
    FollowUpManager *m_followUpManager = nullptr;  // 追问管理器
    bool mIsIntroductionVisible = false;        // 是否在新手引导界面

private slots:
    // 停止所有对话和语音活动；destroyBackend=true 时同时删除/销毁后端会话（仅关机使用）
    void onCloseAllSession(bool destroyBackend = true);
    // 休眠：start=true 进入休眠（仅暂停、保留会话），start=false 唤醒
    void onPrepareForSleep(bool start);
    // 关机：start=true 时执行完整销毁
    void onPrepareForShutdown(bool start);
    void updateBubble();
    void scheduleStreamUiFlush();
    void flushStreamUi(bool finalFlush);
    void clearStreamUiState();

signals:
    void sigSetup();
    void sigSaveDialog(QString filePath);
    void sigEnterMeetingRecordPage(bool entered); // 是否在会议记录页面
    void sigLoadMyMessageBox(const QVariantList &strings, const int type);
    void sigShowDeleteMessageBox(int msgIndex);
    void sigDeleteMessageAt(int msgIndex);
    void addMsg(QString jsonMsgHead, QString msg);
    void addQuestion(const QStringList &questions);
    void addRecvMsg(QString jsonMsgHead, QString msg);
    void updateMsg(QString jsonMsgHead, QString msg, QString markdownMsg);
    void updateReasoningMsg(QString jsonMsgHead, QString msg);
    void addRecvSources(QString jsonMsgHead);
    void appendRecvTool(QString jsonMsgHead);
    void updateTaskToolCalls(QString jsonMsgHead);
    void replyEnd(bool isEnd);
    void replyingChanged(bool replying);
    void insertImage(QString base64EncodedImage, int operatorId, QString imagePath);    //向前端發送文生圖生成的圖片
    void sigCancelGenerateImage();
    void insertOperateImage(QString base64EncodedImage, int operatorId, QString imagePath); //图像处理向前端发送生成的图片
    void updateOperateRefresh(); //图像处理更新重新回答为不可用
    void setReadEnd_Chat(int index);
    void sendImage(QString imagePath, QString base64Image);
    void recvImage(const QString scheJsonMsgHeadToWeb, const QString blackAndWhiteImagePath, const QString base64Image);
    void pushMeetingMsg(QString jsonMsgHead, QString msg);//发送会议信息
    void getMeetingContent();//获取会议总结
    void flushMeetingActiveName(QString fileName);
    void sigAddMeetingError(int errorCode ,const QString errorMsg);//添加错误信息
    void sigFreeAllocatedNoModelError(int errorCode ,const QString errorMsg); // 添加免费试用用尽错误信息
    void loadMeetingList(QString str);//加载会议记录列表
    void completedSummary(const QString fileName);//完成会议总结
    void sigShowMeetingRunningToolTip();//显示会议助手正在运行提示
    void sigShowMeetingNoNlpSet();//显示会议助手未设置NLP提示
    void sigShowMeetingReloadingError(int errorCode);//显示会议助手重新生成错误
    void sigShowMeetingSummaryError(int errorCode, QString errorMsg);//显示会议助手总结生成错误
    void sigHasNoModel(const QString errorStr);//未配置模型处理信号
    void sigAddSummaryTips(bool isR1Model);
    void sigSaveMeeting(); //显示会议助手提示保存时弹窗信号
    void startMeetingTimer();//开始会议记录计时器
    void sigSetMeetingBackBar(const bool isShow);
    void sigSetMeetingBackBarRow(const int showRow, const bool isShow);//控制会议助手后台运行悬浮条显示内容
    void sigSetReloadStatus(bool isReloading);//设置正在生成状态
    void addMeetingSummaryAnimation(const int meetingID);
    void contextCleared();
    void loadConversation(QStringList list);
    void restoreDocumentReference(QString documentJson); // 恢复文档引用信息
    //侧边栏模式控制前端显示页面
    //0显示全部 1显示主页面 2显示侧边栏 3显示会议助手界面 4窗口模式下显示主界面 5窗口模式下显示会议界面
    //6侧边栏模式下显示会议助手详情页 7窗口模式下显示会议助手详情页
    void showPluginPage(int num, QString pluginName);//侧边栏模式切换插件页面
    void showWindowPage(int num);   //窗口模式切换页面
    void showWindowPluginPage(int num, QString pluginName); //窗口模式切换插件页面
    void sigShowSettingsPage(); //打开设置界面
    void changeToSideMode(int width, int height);
    void changeToWinMode(int height);
    void changeWinModeToChat(int num);
    void closeOrOpenSideBar(bool isSidebarClose);
    void loadDialogueList(QStringList list);
    void loadPluginList(QStringList list);
    void deleteDialogueList(QStringList deleteList);
    void copyToClipboard(QString text);     //添加文字到剪切板
    void copySuccess();
    void copyImageToClipboard();
    void addImagePreview(QString imagePath, QString base64Image);
    void addDocumentPreview(QString filePath, QString icon, QString fileName, QString fileSize);
    void saveToPDF();
    void addReportPath(QString path);
    void showSettingPage();
    void modifyDialogueListFlag(QString sessionID);
    void sigSaveAs(const QString &base64Image); //图片另存为打开filedialog
    void sigOpenFile(int type);
    void saveLastMsgToDatabase();       //将listmodel最后一条消息存入数据库
    //void cancelSendAudio(); //取消语音发送
    //void sigVoiceInputError(QString errorMsg, int errorCode); //语音输入错误信号
    void clearListModel();
    void moveScrollBar();  //将前端滚动条移动到最新位置
    void changeSessionId(int sessionId);  //发送前端当前的sessionId
    void changeTaskSessionId(const QString& sessionUuid);  //发送前端当前任务会话UUID
    void updateTaskSessionIsReplying(bool isReplying);  //发送任务会话的isReplying状态
    void taskSessionCountChanged(int count);
    void sessionReplyStatusChanged(const QString& sessionId, bool isReplying, bool hasUnread);
    void activateMainWindowSignal();  // 定义一个信号
    void keyReturnPressed();//发送按下回车键信号
    void updateDialogueList();
    void cancelVoiceStopReplying();//取消语音发送
    void sigSendTmpMsgToQML(const QString str); //向前端发送语音识别中间结果
    void sigSendMsgToQML(const QString str);    //向前端发送语音识别修正后结果
    void sigResetPreIndex();
    void sigLikeAction();
    void startReplyingToChat(bool isReplying);
    void sendFocusChanged();
    void chatInitCompleted();
    void sigNlpModelInitCompleted();
    void showTipMessage(QString tipMessage);
    void sigClearTextArea();     // 清空输入区域
    void newConversation(int page); // 发送前端新建指定对话
    void clearCurrentPage(int page);
    void sigStopVoiceInputIfActive(); // 通知停止语音输入，如果正在输入
    void sendOpenDocumentMessage(QString iconPath, QString message);

    // 通知 QML 端：需要根据 uuid 应用来自划词的指令
    void sigApplyCommandFromSelection(const QString &uuid);
    // 通知 QML 端：划词进入助手，需切换到快问快答主页
    void sigEnterFromWordSelection();

    // 通知 QML 端：系统即将休眠/睡眠，需要停止语音输入和朗读
    void sigStopVoiceInputAndReadingForSleep();

    // 通知 QML 端：即将发送消息，检查并更新 System Default 按钮状态
    void sigAboutToSendMessage();

    // 通知 QML 端：一个外部导入的定时任务执行结果已写入数据库，
    // QML 侧应刷新历史列表。sessionUuid 形如 "kybot-cron:{run_id}"。
    void scheduledSessionCreated(const QString& sessionUuid);

    void sigActivateAndSwitchToSession(const QString& sessionId);

public Q_SLOTS:
    // 把 kybot cron 的一次执行结果落入本地 RECORD 表作为一条 task mode 历史。
    // sessionID 形如 "kybot-cron:{runId}"；已存在时安静跳过。
    // startedAtSecs 为该 run 实际开始时间（秒级 epoch），用于历史列表排序。
    // 返回 true 表示新插入，false 表示已存在或失败。
    bool importKybotCronRun(const QString& jobId,
                            const QString& jobName,
                            const QString& jobPrompt,
                            const QString& runId,
                            const QString& runOutput,
                            const QString& runStatus,
                            qint64 startedAtSecs);

    // 由 KybotCronSyncManager 调用：转发为 scheduledSessionCreated 信号给 QML。
    void onKybotCronRunImported(const QString& sessionUuid);

private:
    QString buildKybotCronUserMessageJson(const QString& jobId,
                                          const QString& jobName,
                                          const QString& jobPrompt,
                                          const QString& runId) const;
    QString buildKybotCronBotMessageJson(const QString& runOutput,
                                         const QString& runStatus) const;
    QString buildKybotCronBotMessageJson(const QString& runOutput,
                                         const QString& runStatus,
                                         const QString& reasonMessage,
                                         const QString& taskToolCalls) const;

private:
    Reader *m_reader = nullptr;
    Recorder *m_recorder = nullptr;

    CommandTemplateList *m_commandListModel = nullptr;
    QLabel *m_labSeparatorLine = nullptr;
    //NLP语言模型
    SystemChat *m_chat = nullptr;
    //是否应用深度思考和联网搜索
    bool m_enable_thinking = false;
    bool m_web_search = false;

#ifdef KB_SUPPORT_ENABLED
    KnowledgeBase *m_KBChat = nullptr;
#endif
    //文字发送的消息
    QString send_message;
    //语音发送消息到前端
    QString speech_send_message;
    int bubble_show_speech_count;
    uint loading_speech_dot_index;
    QTimer *speech_bubble_update_timer = nullptr;
    QTimer *m_streamUiTimer = nullptr;      // 流式气泡 UI 节流，避免每 chunk 全量重排
    bool m_streamUiReasoningPending = false;
    bool m_streamUiMessagePending = false;
    bool new_reply = true;                  //标记是否为新回答
    bool m_is_reasoning_end = true; // 标记思考过程是否结束
    QString recv_message;
    QString recv_reasoning_message; // 思考过程接收消息
    QMap<int, int> m_recv_chunkIndexMap;
    QString m_summary;                      //会议总结
    QString m_origin;                       //会议原文
    QString m_finishedSpeech;
    QString msgHead;                        //消息头
    int promptId = -1;                      //提示词ID
    QString promptIconPath;                        //提示词icon
    QString commandUuid;                    //指令UUID（新指令系统使用）
    bool speechFlag;
    QJsonObject jsonObjToWeb;
    ChatDatabase * m_database = nullptr;    //数据库
    QStringList m_pluginList;

    QString imagePath;
    QString imageSrc;

     QList<DocumentInfo> m_documents;
    //待办
    QString m_todolist;
    int toolId = -1;

    //会议助手正在运行
    bool isMeetingRunning = false;
    //会议助手正在被限制
    bool isMeetingInhibit = false;

    //更新isreplying
    bool  m_isReplying = false;


    ChatType m_chatType = NORMAL;   // 停止回答时，区分普通文本对话和知识库问答。
    
    bool m_isUserStopped = false;   // 标记是否是用户主动停止回答
    // 复制图片到剪贴板时，base64 解码/加载图片可能耗时；用它避免重复触发导致并发堆积
    bool m_copyImageToClipboardInProgress = false;
    // 图片预览/操作（解码+落盘+xdg-open）可能耗时；用它避免重复触发导致并发堆积
    bool m_xdgOpenForImageOperateInProgress = false;
    TaskSessionManager* m_taskSessionManager = nullptr;  // 智能体会话管理器
    QMap<QString, QString> m_backendRecvMessage;  // 后台会话消息累积缓存
    QMap<QString, QString> m_backendRecvMessageHeader;  // 后台会话消息头缓存
    QMap<QString, QString> m_backendRecvReasoningMsg;   // 后台会话思考消息缓存
    QMap<QString, QJsonArray> m_backendTaskToolCalls;    // 后台会话工具调用缓存

#ifdef KB_SUPPORT_ENABLED
    KnowledgeBaseDatabase * m_knowledgeBaseDateBase = nullptr;    //知识库数据库
#else
    void * m_knowledgeBaseDateBase = nullptr;    //知识库数据库（占位符，不支持知识库时为空）
#endif
};

#endif // CMSGPANE_H
