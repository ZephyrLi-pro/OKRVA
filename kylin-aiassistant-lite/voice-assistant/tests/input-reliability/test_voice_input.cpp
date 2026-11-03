#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QDataStream>
#include <QToolButton>
#include "sdl2audioplayerthread.h"
#include <QDebug>
#include <QTimer>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QLabel>
#include <QPixmap>
#include <QFile>
#include "waylandwindowactivation.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <functional>
#include "chatwidget.h"
#include "mainwindow.h"
#include "gatewaychatclient.h"
#include "speechsynthesiscontroller.h"
#include "speechtextsegments.h"
#include "speechpcmedges.h"
#include "speechrecognitioncontroller.h"

class VoiceInputRegression {
    static void require(bool ok, const char *label) {
        if (!ok) qFatal("voice input regression: %s", label);
    }
    static void waitFor(const std::function<bool()> &ready) {
        QElapsedTimer timer;
        timer.start();
        while (!ready() && timer.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
            QThread::msleep(2);
        }
        require(ready(), "mock gateway event timeout");
    }
    static QByteArray serverFrame(const QString &text) {
        const QByteArray payload = text.toUtf8();
        require(payload.size() < 126, "short server fixture frame");
        QByteArray result;
        result.append(char(0x81));
        result.append(char(payload.size()));
        return result + payload;
    }
    static void snapshot(MainWindow &window, const QString &name) {
        const QString directory = qEnvironmentVariable("RVAI_UI_SNAPSHOT_DIR");
        if (directory.isEmpty()) return;
        QCoreApplication::processEvents();
        window.repaint();
        QCoreApplication::processEvents();
        QLabel *status = window.findChild<QLabel *>(QStringLiteral("conversationStatus"));
        QLabel *oldTip = window.findChild<QLabel *>(QStringLiteral("label_tip"));
        qInfo() << "SNAPSHOT" << name << "window" << window.size()
                << "status" << (status ? status->geometry() : QRect())
                << "old-animation-tip-visible" << (oldTip && oldTip->isVisible());
        require(window.grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")),
                "public fixture snapshot saved");
    }
    static void rollingPrefixBuffer() {
        const QByteArray shortPcm(96000,'\1'); // 3 seconds at 16 kHz mono PCM16.
        const QByteArray longPcm(416000,'\2'); // Another 13 seconds.
        {
            SpeechSynthesisController voice;
            voice.m_adapterUrl=QStringLiteral("http://127.0.0.1:9/not-contacted");
            int plays=0;QByteArray queued;
            QObject::connect(voice.m_player,&SDL2AudioPlayerThread::requestPlay,&voice,[&]() { ++plays; });
            QObject::connect(voice.m_player,&SDL2AudioPlayerThread::requestEnqueueData,&voice,
                             [&](const QByteArray &data) { queued+=data; });
            require(voice.beginStream(),"rolling prefix stream opens");
            voice.m_readingActive=true;
            require(voice.handleSynthesizedSegment(shortPcm,16000,1),"first rolling prefix packet accepted");
            QCoreApplication::processEvents();
            require(plays==0 && !voice.m_player->isInitialized(),"slow-input prefix is buffered before opening audio");
            require(voice.handleSynthesizedSegment(longPcm,16000,1),"rolling lead can fill while generation remains open");
            waitFor([&]() { return plays==1; });
            require(voice.m_acceptingStreamText && queued==shortPcm+longPcm,
                    "rolling audio starts before stream completion with an exact continuous prefix");
            voice.close();
        }
        {
            SpeechSynthesisController voice;
            voice.m_adapterUrl=QStringLiteral("http://127.0.0.1:9/not-contacted");
            int plays=0;
            QObject::connect(voice.m_player,&SDL2AudioPlayerThread::requestPlay,&voice,[&]() { ++plays; });
            require(voice.beginStream(),"short rolling stream opens");voice.m_readingActive=true;
            require(voice.handleSynthesizedSegment(shortPcm,16000,1),"short answer PCM prepared");
            require(plays==0,"short answer initially respects the rolling lead");
            voice.finishStream();
            waitFor([&]() { return plays==1; });
            require(!voice.m_acceptingStreamText,"end-of-text releases a short prefix without waiting for nonexistent future text");
            voice.close();
        }
        {
            SpeechSynthesisController voice;
            voice.m_adapterUrl=QStringLiteral("http://127.0.0.1:9/not-contacted");
            int plays=0,errors=0;
            QObject::connect(voice.m_player,&SDL2AudioPlayerThread::requestPlay,&voice,[&]() { ++plays; });
            QObject::connect(&voice,&SpeechSynthesisController::errorOccurred,&voice,[&](const QString &) { ++errors; });
            require(voice.beginStream(),"failed rolling stream opens");voice.m_readingActive=true;
            require(voice.handleSynthesizedSegment(shortPcm,16000,1),"failed stream has a buffered prefix");
            voice.handleSynthesisError(-10000);
            voice.finishStream();QCoreApplication::processEvents();
            require(plays==0 && errors==1 && voice.m_streamPrefetch.isEmpty(),
                    "synthesis error discards unplayed prefix and cannot resurrect it at end-of-stream");
        }
        qInfo()<<"PASS: rolling prefix before end-of-text, short-stream flush, exact PCM concatenation, failed-prefix discard";
    }

    static void prefetchedStreamingSpeech() {
        QTcpServer tts;
        require(tts.listen(QHostAddress::LocalHost, 0), "private streaming TTS endpoint listens");
        QList<QJsonObject> posts;
        QPointer<QTcpSocket> peer;
        MainWindow window(nullptr, false);
        window.m_synthesisController->m_adapterUrl = QStringLiteral("http://127.0.0.1:%1/speech").arg(tts.serverPort());
        QObject::connect(&tts, &QTcpServer::newConnection, &window, [&]() {
            QTcpSocket *socket = tts.nextPendingConnection(); peer = socket;
            auto buffer=std::make_shared<QByteArray>(); auto parsed=std::make_shared<bool>(false);
            QObject::connect(socket, &QTcpSocket::readyRead, &window, [&,socket,buffer,parsed]() {
                *buffer += socket->readAll();
                const int end=buffer->indexOf("\r\n\r\n");
                if (end<0 || *parsed) return;
                int length=-1;
                for (const QByteArray &line : buffer->left(end).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length=line.mid(15).trimmed().toInt();
                if (length<0 || buffer->size()<end+4+length) return;
                *parsed=true;
                posts.append(QJsonDocument::fromJson(buffer->mid(end+4,length)).object());
            });
        });
        auto wav=[](const QByteArray &pcm) {
            QByteArray output; QDataStream stream(&output,QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream.writeRawData("RIFF",4);stream<<quint32(36+pcm.size());
            stream.writeRawData("WAVEfmt ",8);stream<<quint32(16)<<quint16(1)<<quint16(1)
                <<quint32(16000)<<quint32(32000)<<quint16(2)<<quint16(16);
            stream.writeRawData("data",4);stream<<quint32(pcm.size());stream.writeRawData(pcm.data(),pcm.size());
            return output;
        };
        auto respond=[&](const QByteArray &pcm) {
            const QByteArray data=wav(pcm);
            peer->write("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: "
                +QByteArray::number(data.size())+"\r\nConnection: close\r\n\r\n"+data);
            peer->disconnectFromHost();
        };
        int plays=0,finished=0;
        QList<QByteArray> enqueuedAudio;
        QObject::connect(window.m_synthesisController->m_player,&SDL2AudioPlayerThread::requestPlay,
                         &window,[&]() { ++plays; });
        QObject::connect(window.m_synthesisController->m_player,&SDL2AudioPlayerThread::requestEnqueueData,
                         &window,[&](const QByteArray &data) { enqueuedAudio.append(data); });
        QObject::connect(window.m_synthesisController,&SpeechSynthesisController::readingFinished,
                         &window,[&]() { ++finished; });
        window.show();window.m_showChatPage=true;window.m_externalConversationEnabled=true;
        window.m_waitingGatewayReply=true;window.beginAiReply();
        const QString first=QStringLiteral("这是已经生成的第一句话，介绍本地助手的功能。");
        const QString second=QStringLiteral("支持语音。");
        const QString third=QStringLiteral("支持手势。");
        window.appendAiReply(first);
        waitFor([&]() { return posts.size()==1; });
        require(window.m_waitingGatewayReply && posts.first().value(QStringLiteral("input")).toString()==first,
                "first readable sentence starts synthesis before gateway completion");
        window.appendAiReply(second);window.appendAiReply(third);
        require(window.m_currentAiReplyText==first+second+third,"all three sentences are already displayed");
        const QByteArray firstPcm(64000,'\1'),secondPcm(8000,'\2');
        respond(firstPcm);
        waitFor([&]() { return plays==1 && enqueuedAudio.size()==1; });
        QElapsedTimer readyBacklog;readyBacklog.start();
        while (posts.size()<2 && readyBacklog.elapsed()<1500) {
            QCoreApplication::processEvents();QThread::msleep(2);
        }
        require(posts.size()==2,"known short sentences start synthesis without another model frame");
        require(posts.last().value(QStringLiteral("input")).toString()==second+third,
                "queued short sentences are coalesced instead of waiting for more model text");
        respond(secondPcm);
        waitFor([&]() { return enqueuedAudio.size()==2; });
        require(readyBacklog.elapsed()<1500 && plays==1 && finished==0,
                "look-ahead audio is queued during current playback without restart or false completion");
        require(enqueuedAudio[0]==firstPcm && enqueuedAudio[1]==secondPcm,
                "prepared segments preserve order and exact audio bytes");
        require(window.prepareFollowupListening()==QStringLiteral("waiting"),"follow-up stays closed while answer plays");
        const QString availableTail=QStringLiteral("还有本地模型");
        window.appendAiReply(availableTail);
        QElapsedTimer semanticWait;semanticWait.start();
        while (semanticWait.elapsed()<700) { QCoreApplication::processEvents();QThread::msleep(2); }
        require(posts.size()==2,"an unfinished phrase is not synthesized just because a timer elapsed");
        window.appendAiReply(QStringLiteral("。"));
        waitFor([&]() { return posts.size()==3; });
        require(posts.last().value(QStringLiteral("input")).toString()==availableTail+QStringLiteral("。"),
                "a completed short phrase prefetches at its real punctuation boundary");
        respond(secondPcm);
        waitFor([&]() { return enqueuedAudio.size()==3; });
        window.handleAiReplyFinished(first+second+third+availableTail+QStringLiteral("。"));
        waitFor([&]() { return finished==1; });
        require(posts.size()==3 && plays==1,"gateway completion cannot re-read the already spoken prefix");
        require(window.m_followupElapsed.isValid() && window.m_followupTimer->isActive(),
                "complete streamed playback opens the nine-second follow-up");
        window.cancelFollowup();
        window.m_waitingGatewayReply=true;window.setState(MainWindow::Processing);window.beginAiReply();
        const QString tail=QStringLiteral("这是已经生成但暂时没有句号的完整内容").repeated(4);
        window.appendAiReply(tail);
        semanticWait.restart();
        while (semanticWait.elapsed()<700) { QCoreApplication::processEvents();QThread::msleep(2); }
        require(posts.size()==3,"even a long phrase is not cut at an arbitrary character budget");
        window.appendAiReply(QStringLiteral("，"));
        waitFor([&]() { return posts.size()==4; });
        require(posts.last().value(QStringLiteral("input")).toString()==tail+QStringLiteral("，"),
                "synthesis preserves a complete long clause even when it exceeds the adaptive budget");
        auto *interrupt=window.findChild<QToolButton *>(QStringLiteral("interruptButton"));
        require(interrupt,"real interruption button exists");interrupt->click();window.endExternalConversation();
        if (peer) respond(secondPcm);
        window.handleAiReplyFinished(tail+QStringLiteral("，"));
        QElapsedTimer late;late.start();
        while (late.elapsed()<200) { QCoreApplication::processEvents();QThread::msleep(2); }
        require(plays==1 && !window.m_synthesisController->isReadingActive()
                && !window.m_followupElapsed.isValid(),"cancelled prefetch cannot play late audio or reopen listening");
        window.m_suppressCurrentReplySpeech=false;window.m_waitingGatewayReply=true;
        window.setState(MainWindow::Processing);window.beginAiReply();
        window.appendAiReply(QStringLiteral("未完成文字"));
        window.handleChatError(QStringLiteral("public mock timeout"));
        late.restart();
        while (late.elapsed()<700) { QCoreApplication::processEvents();QThread::msleep(2); }
        require(posts.size()==4 && plays==1,"late tail timer cannot synthesize after an error");
        window.m_suppressCurrentReplySpeech=false;window.m_externalConversationEnabled=true;
        window.m_waitingGatewayReply=true;window.setState(MainWindow::Processing);
        const QString finalOnly=(first+second+third+tail).repeated(3);
        window.handleAiReplyFinished(finalOnly);
        QString reconstructed;
        int finalOnlyRequests=0;
        while (reconstructed.size()<finalOnly.size()) {
            waitFor([&]() { return posts.size()==5+finalOnlyRequests; });
            const QString part=posts.last().value(QStringLiteral("input")).toString();
            if (!finalOnlyRequests) require(part.size()<=24 && part.size()<finalOnly.size(),
                "final-only automatic answers also start with a short prefetched request");
            reconstructed+=part;
            require(!part.isEmpty() && finalOnly.startsWith(reconstructed),
                    "final-only streaming preserves every known sentence in order");
            respond(finalOnlyRequests ? secondPcm : firstPcm);
            ++finalOnlyRequests;
        }
        waitFor([&]() { return finished==2; });
        require(finalOnlyRequests>1 && plays==2,"final-only automatic answer prefetches and plays without full-answer buffering");
        window.cancelFollowup();
        qInfo()<<"PASS: punctuation-only segmentation, three known sentences, intact long clauses, no timer fragmentation, final-only streaming, cancellation, follow-up";
    }

public:
    static void run() {
        QByteArray softSpeech;
        for (int i=0;i<16000;++i) { softSpeech.append(char(24));softSpeech.append(char(0)); }
        const QByteArray paddedSpeech=QByteArray(32000,'\0')+softSpeech+QByteArray(19200,'\0');
        const QByteArray trimmedSpeech=SpeechPcm::trimEdges(paddedSpeech,16000,1,120);
        require(trimmedSpeech==QByteArray(1920,'\0')+softSpeech+QByteArray(3840,'\0'),
                "edge trimming preserves quiet phonemes and only shortens silent guards");
        require(SpeechPcm::trimEdges(QByteArray(3200,'\0'),16000,1,120)==QByteArray(3200,'\0'),
                "all-silent PCM is preserved without negative lengths");
        QString punctuationBuffer=QStringLiteral("模型Qwen3.5使用llama.cpp处理一段没有任何标点的长文本").repeated(3);
        const QString originalPunctuationBuffer=punctuationBuffer;
        QString phrase;
        require(!SpeechText::takeChunk(punctuationBuffer,&phrase) && punctuationBuffer==originalPunctuationBuffer,
                "version and extension dots do not cut an unpunctuated phrase");
        punctuationBuffer+=QStringLiteral("，后文。");
        require(SpeechText::takeChunk(punctuationBuffer,&phrase) && phrase==originalPunctuationBuffer+QStringLiteral("，"),
                "natural comma ends a complete clause without losing content");
        require(SpeechText::takeChunk(punctuationBuffer,&phrase) && phrase==QStringLiteral("后文。"),
                "short complete sentences retain their own natural boundary");
        rollingPrefixBuffer();
        prefetchedStreamingSpeech();
        QTemporaryDir activation;
        require(activation.isValid(), "private activation fixture directory");
        QFile executable(activation.path() + QStringLiteral("/wlrctl"));
        require(executable.open(QIODevice::WriteOnly), "create mock compositor client");
        executable.write((QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"")
            + activation.path() + QStringLiteral("/args\"\n")).toUtf8());
        executable.close();
        executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const QByteArray originalPath = qgetenv("PATH");
        qputenv("PATH", activation.path().toUtf8() + ":" + originalPath);
        QObject activationOwner;
        requestWaylandWindowActivation(&activationOwner, QStringLiteral("offscreen"), QStringLiteral("voice-assistant"));
        QCoreApplication::processEvents();
        require(!QFile::exists(activation.path() + QStringLiteral("/args")), "other Qt platforms do not invoke compositor client");
        requestWaylandWindowActivation(&activationOwner, QStringLiteral("wayland"), QStringLiteral("voice-assistant"));
        waitFor([&]() { return QFile::exists(activation.path() + QStringLiteral("/args")); });
        QFile args(activation.path() + QStringLiteral("/args"));
        require(args.open(QIODevice::ReadOnly) && args.readAll() == "toplevel\nfocus\napp_id:voice-assistant\n",
                "activation is restricted to the voice window with separate arguments");
        waitFor([&]() { return activationOwner.findChildren<QProcess *>().isEmpty(); });
        qputenv("PATH", originalPath);
        MainWindow window(nullptr, false);
        require(!window.m_speechController->isRunning(), "external mode does not open SDK microphone");
        window.showMinimized();
        QCoreApplication::processEvents();
        require(window.isMinimized(), "minimized-window fixture");
        window.m_waitingGatewayReply = true;
        window.setState(MainWindow::Processing);
        require(window.prepareExternalListening() == QStringLiteral("ready"), "capture allowed before speech output");
        require(!window.isMinimized(), "explicit voice wake restores minimized window");
        QStackedWidget *pages = window.findChild<QStackedWidget *>(QStringLiteral("stackedWidget_middle"));
        require(pages && pages->currentWidget()->objectName() == QStringLiteral("page_listening"),
                "cold first wake retains the initial listening page");
        require(window.m_externalCaptureActive && window.m_suppressCurrentReplySpeech, "capture reserves quiet output");
        window.onSubmitExternalText(QStringLiteral("first question"));
        require(!window.m_externalCaptureActive && window.m_externalVoiceQueue.size() == 1, "busy input queued, not discarded");
        window.onSubmitExternalText(QStringLiteral("second question"));
        window.onSubmitExternalText(QStringLiteral("third question"));
        window.onSubmitExternalText(QStringLiteral("fourth question"));
        require(window.m_externalVoiceQueue.size() == 3, "full queue is not overwritten");
        require(window.externalInputStatus() == QStringLiteral("full"), "queue-full capture handshake");
        window.handleChatError(QStringLiteral("test failure"));
        require(window.m_externalVoiceQueue.isEmpty() && !window.m_waitingGatewayReply, "error cancels queued requests explicitly");
        window.m_muted = true;
        require(window.prepareExternalListening() == QStringLiteral("muted"), "muted input denied");
        window.onSubmitExternalText(QStringLiteral("muted question"));
        require(window.m_externalVoiceQueue.isEmpty(), "late ASR does not bypass mute");
        window.m_muted = false;
        window.m_externalCaptureExpired = true;
        window.onSubmitExternalText(QStringLiteral("expired capture question"));
        require(window.m_externalVoiceQueue.isEmpty(), "late timed-out ASR cannot execute a stale question");
        require(window.prepareExternalListening() == QStringLiteral("ready") && !window.m_externalCaptureExpired, "new wake clears expired capture");
        window.finishExternalCapture();
        window.m_synthesisController->m_readingActive = true;
        require(window.prepareExternalListening() == QStringLiteral("speaking"), "speech output blocks microphone capture");
        require(!window.m_externalCaptureActive, "half-duplex rejection does not reserve capture");
        window.m_synthesisController->m_readingActive = false;
        window.m_waitingGatewayReply = true;
        window.m_externalCaptureActive = true;
        window.onExternalRecognitionFailed(QStringLiteral("too_long"));
        require(!window.m_externalCaptureActive && window.m_waitingGatewayReply, "capture failure does not cancel old agent round");
        window.m_resumeRecognitionAfterPlayback = true;
        window.resumeRecognitionAfterPlayback();
        require(window.m_state == MainWindow::Answering && window.m_waitingGatewayReply, "early TTS finish cannot fake Idle");
        require(!window.setReadingContext(QStringLiteral("manual"), QString(2001, QLatin1Char('x')), 1), "oversize page rejected");
        require(!window.setReadingContext(QStringLiteral("manual"), QStringLiteral("page"), 0), "invalid page rejected");
        require(window.setReadingContext(QStringLiteral("manual"), QStringLiteral("page text"), 2), "explicit page accepted");
        require(window.m_readingContext.contains(QStringLiteral("page text")), "page text stays in memory");
        require(window.setReadingContext(QString(), QString(), 0) && window.m_readingContext.isEmpty(), "reader can clear closed page");
        window.m_externalVoiceQueue.clear();
        window.m_waitingGatewayReply = true;
        require(window.setReadingContext(QStringLiteral("manual"), QStringLiteral("current page source"), 2), "set isolated page fixture");
        window.onSubmitExternalText(QStringLiteral("这一页主要讲了什么？"));
        require(window.m_externalVoiceQueue.frontReady(), "page questions resolve only the explicit reader snapshot without querying a draft");
        ExternalVoiceInput pageInput = window.m_externalVoiceQueue.take();
        QJsonObject pageSource = QJsonDocument::fromJson(pageInput.context.toUtf8()).object();
        require(pageSource.value(QStringLiteral("text")).toString() == QStringLiteral("current page source")
                && pageSource.value(QStringLiteral("page")).toInt() == 2, "page reference remains complete and selected");
        window.setReadingContext(QString(), QString(), 0);
        window.onSubmitExternalText(QStringLiteral("这一页主要讲了什么？"));
        require(window.m_externalVoiceQueue.frontReady(), "closed page context resolves explicitly");
        require(window.m_externalVoiceQueue.take().context.isEmpty(), "closed reader cannot fall back to an unrelated draft");
        window.setReadingContext(QStringLiteral("manual"), QStringLiteral("expired page source"), 3);
        window.m_readingContextExpires = 1;
        window.onSubmitExternalText(QStringLiteral("当前页主要讲了什么？"));
        require(window.m_externalVoiceQueue.frontReady() && window.m_externalVoiceQueue.take().context.isEmpty(), "expired page cannot be reused");
        window.setReadingContext(QString(), QString(), 0);
        window.m_waitingGatewayReply = false;
        window.m_externalVoiceQueue.clear();
        window.clearInlineError(false);
        ChatWidget *history = window.findChild<ChatWidget *>(QStringLiteral("page_chat"));
        require(history, "conversation widget exists");
        const QString retainedAnswer = QStringLiteral("我是本地系统助手，可以帮你查看资料和执行系统操作。");
        for (int i = 0; i < 3; ++i) {
            history->addUserBubble();
            history->setUserBubbleText(QStringLiteral("你是谁？"));
            history->addAIBubble();
            history->setLatestAIBubbleText(retainedAnswer);
        }
        window.m_showChatPage = true;
        window.updateMiddlePage();
        window.setState(MainWindow::Idle);
        const QString sessionBeforeFollowup = window.m_gatewayClient->m_sessionId;
        window.m_followupRequested = true;
        window.m_resumeRecognitionAfterPlayback = false;
        window.setState(MainWindow::Reading);
        window.m_synthesisController->readingFinished();
        require(window.m_followupElapsed.isValid() && window.m_state == MainWindow::Idle,
                "final audio completion starts follow-up even after an earlier segment cleared resume flag");
        window.cancelFollowup();
        window.m_followupRequested = true;
        window.m_synthesisController->m_readingActive = true;
        require(window.prepareFollowupListening() == QStringLiteral("waiting"), "follow-up waits for all speech to drain");
        require(!window.m_followupElapsed.isValid(), "no window countdown during playback");
        window.m_synthesisController->m_readingActive = false;
        window.m_waitingGatewayReply = true;
        require(window.prepareFollowupListening() == QStringLiteral("waiting"), "follow-up waits for gateway done");
        window.m_waitingGatewayReply = false;
        window.maybeOfferFollowup();
        require(window.m_followupTimer->isActive() && window.m_followupTimer->interval() == 9000,
                "nine-second follow-up window starts after both completion gates");
        QScrollArea *historyScroll = history->findChild<QScrollArea *>();
        require(historyScroll, "conversation is scrollable");
        waitFor([&]() { return historyScroll->verticalScrollBar()->maximum() > 0; });
        historyScroll->verticalScrollBar()->setValue(0);
        const int previousScroll = historyScroll->verticalScrollBar()->value();
        const QString followup = window.prepareFollowupListening();
        const int remaining = followup.mid(6).toInt();
        require(followup.startsWith(QStringLiteral("ready:")) && remaining > 0 && remaining <= 9000,
                "one-shot follow-up claim includes remaining deadline");
        require(window.m_externalCaptureActive && !window.m_followupTimer->isActive(), "claim switches to external capture");
        require(window.m_showChatPage && pages->currentWidget() == history,
                "follow-up keeps conversation page instead of returning to first-wake animation");
        require(historyScroll->verticalScrollBar()->value() == previousScroll,
                "follow-up does not reset history scroll position");
        const QList<QLabel *> historyLabels = history->findChildren<QLabel *>();
        bool answerRetained = false;
        for (QLabel *label : historyLabels) answerRetained |= label->text() == retainedAnswer;
        require(answerRetained, "previous answer remains in the conversation");
        QLabel *status = window.findChild<QLabel *>(QStringLiteral("conversationStatus"));
        require(status && status->isVisible() && status->text().contains(QStringLiteral("正在聆听")),
                "listening status is visible below retained history");
        snapshot(window, QStringLiteral("followup-listening"));
        require(!window.m_speechController->isRunning(), "follow-up never opens second SDK microphone");
        require(window.prepareFollowupListening() == QStringLiteral("waiting"), "duplicate claim cannot start another recording");
        require(window.m_gatewayClient->m_sessionId == sessionBeforeFollowup, "follow-up keeps conversation session");
        window.onExternalRecognitionFailed(QStringLiteral("followup_timeout"));
        require(!window.m_externalCaptureActive && window.prepareFollowupListening() == QStringLiteral("off"),
                "silent follow-up ends without another automatic loop");
        require(window.m_showChatPage && pages->currentWidget() == history,
                "silent timeout retains conversation history");
        require(status->isVisible() && status->text().contains(QStringLiteral("等待唤醒"))
                && status->toolTip().contains(QStringLiteral("你好麒麟")),
                "silent timeout updates the footer to waiting-for-wake");
        snapshot(window, QStringLiteral("followup-idle"));
        window.showInlineError(QStringLiteral("识别暂时失败，请重新唤醒。"));
        require(status->isVisible() && status->text().contains(QStringLiteral("识别暂时失败")),
                "capture errors remain readable without switching away from history");
        window.clearInlineError(true);
        window.m_followupRequested = true;
        window.maybeOfferFollowup();
        window.m_followupTimer->start(1);
        waitFor([&]() { return !window.m_followupElapsed.isValid(); });
        require(window.prepareFollowupListening() == QStringLiteral("off"), "unclaimed window expires too");
        window.m_followupRequested = true;
        require(window.prepareFollowupListening().startsWith(QStringLiteral("ready:")), "follow-up admitted before mute");
        QObject *panel = window.findChild<QObject *>(QStringLiteral("bottomWidget"));
        require(panel && QMetaObject::invokeMethod(panel, "muteToggled", Qt::DirectConnection, Q_ARG(bool, true)),
                "real mute handler invoked");
        require(!window.m_externalCaptureActive && window.m_externalCaptureExpired,
                "mute expires the in-flight external recording");
        require(QMetaObject::invokeMethod(panel, "muteToggled", Qt::DirectConnection, Q_ARG(bool, false)),
                "real unmute handler invoked");
        window.onSubmitExternalText(QStringLiteral("late result from before mute"));
        require(window.m_externalVoiceQueue.isEmpty() && !window.m_speechController->isRunning(),
                "unmute cannot admit stale ASR or start a second microphone");
        require(window.prepareExternalListening() == QStringLiteral("ready") && !window.m_externalCaptureExpired,
                "explicit new wake starts a fresh recording after mute");
        window.finishExternalCapture();
        window.m_followupRequested = true;
        window.m_muted = true;
        require(window.prepareFollowupListening() == QStringLiteral("off"), "mute blocks follow-up");
        window.m_muted = false;
        window.m_followupRequested = true;
        window.endExternalConversation();
        require(!window.m_externalConversationEnabled && window.prepareFollowupListening() == QStringLiteral("off"),
                "explicit end disables future follow-up");
        require(window.m_showChatPage && pages->currentWidget() == history,
                "explicit conversation end does not hide history");
        require(window.prepareExternalListening() == QStringLiteral("ready"), "new wake can restart after ending");
        window.finishExternalCapture();
        window.m_followupRequested = true;
        require(window.prepareFollowupListening().startsWith(QStringLiteral("ready:")), "follow-up admitted before replay race");
        window.m_synthesisController->m_readingActive = true;
        window.onSubmitExternalText(QStringLiteral("speaker replay must not become a question"));
        require(window.m_externalVoiceQueue.isEmpty() && !window.m_externalConversationEnabled,
                "playback starting during capture cannot become an agent question");
        window.m_synthesisController->m_readingActive = false;
        require(window.prepareExternalListening() == QStringLiteral("ready"), "normal wake still works after replay rejection");
        window.finishExternalCapture();
        window.m_followupRequested = true;
        window.hide();
        require(window.prepareFollowupListening() == QStringLiteral("off"), "hidden window cannot re-open microphone");
        window.show();
        window.setState(MainWindow::Idle);
        QTemporaryDir temporary;
        QLocalServer server;
        require(server.listen(temporary.filePath(QStringLiteral("gateway.sock"))), "mock socket listens");
        QLocalSocket *peer = nullptr;
        QByteArray buffer;
        bool upgraded = false;
        QStringList messages;
        QObject::connect(&server, &QLocalServer::newConnection, &window, [&]() {
            peer = server.nextPendingConnection();
            QObject::connect(peer, &QLocalSocket::readyRead, &window, [&]() {
                buffer += peer->readAll();
                if (!upgraded) {
                    const int end = buffer.indexOf("\r\n\r\n");
                    if (end < 0) return;
                    QByteArray key;
                    for (QByteArray header : buffer.left(end).split('\n')) {
                        if (header.toLower().startsWith("sec-websocket-key:"))
                            key = header.mid(header.indexOf(':') + 1).trimmed();
                    }
                    const QByteArray accept = QCryptographicHash::hash(
                            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1).toBase64();
                    peer->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: kylinbot.v1\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n");
                    peer->flush();
                    buffer.remove(0, end + 4);
                    upgraded = true;
                }
                while (buffer.size() >= 2) {
                    const quint8 second = quint8(buffer.at(1));
                    int length = second & 127;
                    int offset = 2;
                    if (length == 126) {
                        if (buffer.size() < 4) return;
                        length = (quint8(buffer.at(2)) << 8) | quint8(buffer.at(3));
                        offset = 4;
                    }
                    require(length != 127, "bounded client frame");
                    const bool masked = (second & 128) != 0;
                    if (buffer.size() < offset + (masked ? 4 : 0) + length) return;
                    const QByteArray mask = masked ? buffer.mid(offset, 4) : QByteArray();
                    if (masked) offset += 4;
                    QByteArray payload = buffer.mid(offset, length);
                    if (masked) for (int i = 0; i < payload.size(); ++i) payload[i] = char(payload.at(i) ^ mask.at(i % 4));
                    const QJsonObject frame = QJsonDocument::fromJson(payload).object();
                    if (frame.value(QStringLiteral("type")).toString() == QStringLiteral("message"))
                        messages.append(frame.value(QStringLiteral("content")).toString());
                    buffer.remove(0, offset + length);
                }
            });
        });
        // Open our isolated socket directly with NO bearer token or production
        // config changes. Once connected, normal submit/drain uses that socket.
        window.m_gatewayClient->m_webSocket->open(server.fullServerName(), QStringLiteral("/ws/chat"), {}, QStringLiteral("kylinbot.v1"));
        waitFor([&]() { return window.m_gatewayClient->isConnected(); });
        ExternalVoiceInput input;
        quint64 first = 0, second = 0;
        input.text = QStringLiteral("queued first question");
        window.m_externalVoiceQueue.append(input, &first);
        window.m_externalVoiceQueue.resolve(first, QStringLiteral("first reference snapshot"), false);
        input.text = QStringLiteral("queued second question");
        window.m_externalVoiceQueue.append(input, &second);
        window.m_externalVoiceQueue.resolve(second, QStringLiteral("second reference snapshot"), false);
        window.submitNextExternalInput();
        waitFor([&]() { return messages.size() == 1; });
        require(messages.first().contains(QStringLiteral("queued first question"))
                && messages.first().contains(QStringLiteral("first reference snapshot"))
                && !messages.first().contains(QStringLiteral("second reference snapshot")), "first payload has its own snapshot");
        peer->write(serverFrame(QStringLiteral("{\"type\":\"done\",\"full_response\":\"first answer\"}")));
        peer->flush();
        waitFor([&]() { return messages.size() == 2; });
        require(messages.last().contains(QStringLiteral("queued second question"))
                && messages.last().contains(QStringLiteral("second reference snapshot")), "done drains FIFO into second gateway request");
        window.m_suppressCurrentReplySpeech = true;
        peer->write(serverFrame(QStringLiteral("{\"type\":\"done\",\"full_response\":\"second answer\"}")));
        peer->flush();
        waitFor([&]() { return !window.m_waitingGatewayReply; });
        require(window.m_externalVoiceQueue.isEmpty(), "all accepted questions submitted exactly once");
        input.text = QStringLiteral("这一页主要讲什么？");
        window.m_externalVoiceQueue.append(input, &first);
        window.m_externalVoiceQueue.resolve(first, QString(), false);
        window.submitNextExternalInput();
        require(!window.m_waitingGatewayReply && messages.size() == 2, "missing page context refuses ungrounded answer");
        window.m_gatewayClient->shutdown();
        peer->disconnectFromServer();
        QCoreApplication::processEvents();
        // All replies came from the local mock, never a real model or TTS.
        qInfo() << "PASS: real MainWindow admission, queue, mute, half-duplex, failure, early playback finish, page bounds";
        qInfo() << "PASS: mock UDS gateway FIFO, per-question context snapshot, no dropped accepted inputs, missing-page guard";
        qInfo() << "PASS: follow-up playback/gateway gates, nine-second deadline, single claim, same session, timeout, mute, end, hidden window";
        qInfo() << "PASS: initial listening page, retained history/scroll, visible listening/idle/error footer, no page reset on timeout/end";
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    VoiceInputRegression::run();
    // Deliver late D-Bus context callbacks after the test window was destroyed;
    // QObject disconnection must keep them from resurrecting queued requests.
    QTimer::singleShot(1800, &app, &QCoreApplication::quit);
    return app.exec();
}
