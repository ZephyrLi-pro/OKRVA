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
#include "cmainwnd.h"

#include <usermanual.h>
#include <QWidget>
#include <QScreen>
#include <QPainter>

#include "windowhelper.h"
#include "global/filedialoghelper.h"
#include "global/kaglobal.h"
#include "readercontrol.h"
#include "recordercontrol.h"

CMainWnd *g_mainWnd = nullptr;

CMainWnd::CMainWnd(QWidget *parent)
    : KWidget(parent)
{
    createAllChildWnd();

    m_msgPane = new CMsgPane();

    // 初始化 X11WindowHelper
    m_x11WindowHelper = new X11WindowHelper(this);

    initSolts();
    initLayout();

    initScribeWords();
    initScribeWordsSetting();
    initShortcut();

    checkTheme(); //暂时不跟随主题，字体由qml自适应
}

CMainWnd::~CMainWnd()
{
}

CMainWnd *CMainWnd::getInstance()
{
    if (g_mainWnd == nullptr){
        g_mainWnd = new CMainWnd;
    }

    return g_mainWnd;
}

void CMainWnd::createAllChildWnd()
{
    return;
}

void CMainWnd::initSolts()
{
    connect(m_msgPane, &CMsgPane::sigEnterMeetingRecordPage, this, &CMainWnd::handleEnterMeetingRecordPage);

    connect(m_msgPane, &CMsgPane::sigShowDeleteMessageBox, this, &CMainWnd::onShowDeleteMessageBox);

    // 连接信号与槽
    connect(m_msgPane, &CMsgPane::activateMainWindowSignal, this, &CMainWnd::onActivateMainWindow);
    // sigSaveAs 信号连接保留（向后兼容），但 QML 端已直接使用 fileDialogHelper.saveImage()
    connect(m_msgPane, &CMsgPane::sigSaveAs, this, &CMainWnd::onSaveAs);

    connect(m_msgPane, &CMsgPane::chatInitCompleted, this, [&](){
        isChatInitCompleted = true;
    });

    connect(m_msgPane, &CMsgPane::copyImageToClipboard, [=](){
        m_msgPane->showTip(tr("Copy Successful"));
    });
    connect(m_msgPane, &CMsgPane::copySuccess, [=](){
        m_msgPane->showTip(tr("Copy Successful"));
    });
    connect(m_msgPane, &CMsgPane::sigLikeAction, [=](){
        m_msgPane->showTip(tr("Thanks for your feedback"));
    });

    return;
}

void CMainWnd::initLayout()
{

}

void CMainWnd::switchToSideMode()
{

    return;
}

void CMainWnd::switchToWindowMode()
{
    return;
}

void CMainWnd::setSideModePos()
{
    return;
}

void CMainWnd::setWindowModePos()
{
    return;
}


void CMainWnd::initScribeWords()
{
    return;
}

void CMainWnd::initScribeWordsSetting()
{
}

void CMainWnd::autoShowSelectWidget()
{

}

void CMainWnd::handleSelectionTextChanged()
{
}

void CMainWnd::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
}

void CMainWnd::showView()
{
}

void CMainWnd::onFocusChanged(QWidget *oldWidget, QWidget *newWidget) {

}

void CMainWnd::onPageChanged(const int pageNumber)
{
    return;
}

void CMainWnd::handleEnterMeetingRecordPage(bool entered)
{
    PageType currPage;
    int curWinMode = m_msgPane->getCurWinMode();
    KyInfo() << "=======handleEnterMeetingRecordPage:" << "curWinMode" << curWinMode << "EnterMeetingRecordPage" << entered;
    if(curWinMode == 2)
    {
        // 侧边栏模式
        if (entered == true)
        {
            // true记录过程中，记录中已经支持侧边栏按钮显示
            //m_btnSideBar->setVisible(true);
            currPage = PageType::MEETING_RECORD_SIDEBAR_MODE;
        } else {
            // false记录结束，已返回会议主页，显示侧边栏按钮
            //m_btnSideBar->setVisible(true);
            currPage = PageType::MEETING_HOME_SIDEBAR_MODE;
        }
    }
    else if (curWinMode == 3)
    {
        // 窗口模式
        if (entered == true)
        {
            // true记录过程中
            currPage = PageType::MEETING_RECORD_WINDOW_MODE;
        } else {
            // false记录结束，已返回会议主页
            currPage = PageType::MEETING_HOME_WINDOW_MODE;
        }
    }
}

void CMainWnd::onShowDeleteMessageBox(int msgIndex)
{
    CustomMessageBox msgBox(this);
    msgBox.setMessage(tr("Are you sure you want to delete it?？"));
    msgBox.setInformativeText(tr("After deleting the conversation, it cannot be restored."));
    msgBox.setButtons(CustomMessageBox::OkButton | CustomMessageBox::CancelButton);
    msgBox.move(this->x() + (this->width() - msgBox.width()) / 2 , this->y() + (this->height() - msgBox.height()) / 2);
    if (msgBox.exec() == QDialog::Accepted) {
        if (msgBox.clickedButton() == CustomMessageBox::OkButton) {
            m_msgPane->deleteMessageByIndex(msgIndex);
        }
    }
}

void CMainWnd::onCreateDialog()
{
    // 先保存当前会话
    m_msgPane->saveDialog();

    // 然后清除当前会话
    m_msgPane->clearDialog();

    // 隐藏历史会话界面
//    m_sideBar->hide();

    return;
}

void CMainWnd::showOrHideWindow()
{
    KyInfo() << "----showorhidewindow----isMinimizeClicked" << isMinimizeClicked << "----isMinimized----" << isMinimized();

    bool isMinimizedState = this->isMinimized() || isMinimizeClicked;

    if (this->isVisible()) {
        if (isMinimizedState) {
            showView(); // 可见且最小化
        }
    } else {
        showView();
    }
}

void CMainWnd::onShowHelp()
{
    kdk::UserManual aiAssistantUserManual;
    if (!aiAssistantUserManual.callUserManual("kylin-aiassistant")) {
        qCritical() << "user manual call fail!";
    }
    return;
}

void CMainWnd::onShowAbout()
{
    m_aboutWindowOpen = true;
    Q_EMIT sigShowAbout();
    return;
}

bool CMainWnd::isAboutWindowOpen() const
{
    return m_aboutWindowOpen;
}

void CMainWnd::setAboutWindowOpen(bool open)
{
    m_aboutWindowOpen = open;
}

void CMainWnd::activateAboutWindow()
{
    Q_EMIT sigActivateAboutWindow();
}

void CMainWnd::onActivateMainWindow()
{

}

void CMainWnd::onSaveAs(const QString &base64Image)
{
    bool ok = false;
    QPixmap pixmap;

    // 去掉前缀
    QStringList parts = base64Image.split(",");
    if(parts.length() == 2)
    {
        QString imageData = parts.at(1);
        QByteArray byteArray = QByteArray::fromBase64(imageData.toLocal8Bit());
        pixmap.loadFromData(byteArray);
    }
    if(pixmap.isNull())
    {
        KyInfo() << "pixmap is null";
        return;
    }

    // 获取当前时间
    QDateTime currentTime = QDateTime::currentDateTime();
    QString formattedTime = currentTime.toString("yyyyMMdd_hhmmss");
    // 使用格式化后的时间作为文件名
    QString fileName = "aiassistant_" + formattedTime + ".png";
    // 使用 Globals::getDefaultSavePath() 获取默认保存路径，与QML设置界面保持一致
    QString baseDir = Globals::getinstance()->getDefaultSavePath();

    QString path;
    if(baseDir.endsWith("/"))
    {
        path = baseDir + fileName;
    }
    else
    {
        path = baseDir + "/" + fileName;
    }

    // 不以主窗口为父：侧边栏模式下主窗口较窄，否则保存对话框会相对助手窗口居中而非桌面中心
    QFileDialog *fileDialog = new QFileDialog(nullptr);
    fileDialog->setAccessibleName("kylin-aiassistant_QFileDialog_fileDialog");
    fileDialog->setModal(true);
    fileDialog->setWindowModality(Qt::ApplicationModal);
    fileDialog->setNameFilter(QLatin1String("PNG(*.png);;JPE(*.jpe);;JPEG(*.jpeg);;JPG(*.jpg);;PBM(*.pbm);;PGM(*.pgm);;BMP(*.bmp);;PPM(*.ppm);;TIFF(*.tiff);;WEBP(*.webp)"));
    fileDialog->setDirectory(baseDir);
    fileDialog->setAcceptMode(QFileDialog::AcceptSave);
    fileDialog->selectFile(path);

    FileDialogHelper::centerDialogOnDesktop(fileDialog);

    if(fileDialog->exec() == QFileDialog::Accepted)
    {
        QString savePath = fileDialog->selectedFiles().at(0);

        QFileInfo fileName = savePath;
        if(savePath.isNull())
        {
            return;
        }

        if(fileName.suffix().isNull())
        {
            savePath += QLatin1String(".png");
        }
        fileName = savePath;
        QString name = fileName.fileName();
        QString msg;
        QString message;
        if(!name.startsWith(QChar('.'), Qt::CaseInsensitive))
        {
            QFileInfo fileInfo(savePath);
            QStringList parts = base64Image.split(",");
            // 解码 Base64 字符串
            QByteArray decodedData;
            if(parts.length() == 2)
            {
                QString imageData = parts.at(1);
                decodedData = QByteArray::fromBase64(imageData.toLatin1());
            }
            if(fileInfo.suffix() == "jpeg"|| fileInfo.suffix() == "pgm"|| fileInfo.suffix() == "pbm" || fileInfo.suffix() == "ppm" || fileInfo.suffix() == "jpg"){
                // jpeg pgm pbm ppm jpg保存白色背景会填充黑色，额外处理，其中pgm pbm只支持黑白像素
                QImage originalImage;
                originalImage.loadFromData(decodedData);
                // 创建带有白色背景的新图像
                QImage newImage(originalImage.size(), QImage::Format_RGB32);
                newImage.fill(Qt::white);
                // 使用QPainter在新图像上绘制原图
                QPainter painter(&newImage);
                painter.drawImage(0, 0, originalImage);
                painter.end();
                // 保存图像
                if (newImage.save(savePath)) {
                    ok = true;
                } else {
                    ok = false;
                }
            }else if(fileInfo.suffix() == "jpe"){
                // 使用 QImage 加载解码后的数据
                // jpe保存会报错，额外处理
                QImage img;
                if (img.loadFromData(decodedData)) {
                    // 保存为 .jpe 格式（实际上是 JPEG 格式）
                    if (img.save(savePath, "JPEG")) {
                        ok = true;
                    } else {
                        ok = false;
                    }
                } else {
                    qDebug() << "Failed to load image from decoded data.";
                }
                //ok = img.save(savePath, "JPEG");
            }else if(fileInfo.suffix() == "bmp"){
                // bmp保存白色背景会填充黑色，额外处理
                QImage image;
                image = image.convertToFormat(QImage::Format_RGB32);
                    // 保存为 .jpe 格式（实际上是 JPEG 格式）
                if (image.loadFromData(decodedData)) {
                    // 保存为 .jpe 格式（实际上是 JPEG 格式）
                    if (image.save(savePath)) {
                        ok = true;
                    } else {
                        ok = false;
                    }
                } else {
                    qDebug() << "Failed to load image from decoded data.";
                }
            }else{
                ok = pixmap.save(savePath);
            }
        }
        if(ok)
        {
            QString pathNoFile = savePath.left(savePath.lastIndexOf(QLatin1String("/")));
            message = QObject::tr("Image saved as ") + savePath;
            msg = "<a href=\"" + savePath + "\">" + message + "</a>";
        }
        else
        {
            if(name.contains(QChar('/')))
            {
                msg = QObject::tr("file name can not contains '/'");
            }
            else if(name.startsWith(QChar('.'), Qt::CaseInsensitive))
            {
                msg = QObject::tr("can not save file as hide file");
            }
            else if(name.length() > 255)
            {
                msg = QObject::tr("can not save  because filename too long");
            }
            else
            {
//                message = QObject::tr("Error trying to save as ") + savePath;
//                msg = "<a href=\"" + savePath + "\">" + message + "</a>";
                msg = QObject::tr("Error trying to save as ") + savePath;
            }
            QMessageBox saveErrBox(QMessageBox::Warning, QObject::tr("Save Error"), msg);
            saveErrBox.exec();
        }
    }
    return;
}

void CMainWnd::onShowScribeWord()
{

}

void CMainWnd::initPosition()
{
    return;
}

void CMainWnd::adjustForScreenChanged()
{
    return;
}

void CMainWnd::changeToOccupiedMode()
{
    return;
}

void CMainWnd::changeToNormalMode()
{
    return;
}

void CMainWnd::initShortcut()
{

}


QString CMainWnd::getAiAssistantVersion()
{
#ifdef BUILD_KAIMING
    return qgetenv("APP_VERSION");
#else
    FILE *pp = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *q = NULL;
    QString version = tr("none");

    pp = popen("dpkg -l kylin-aiassistant", "r");
    if(NULL == pp)
        return version;

    while((read = getline(&line, &len, pp)) != -1){
        q = strrchr(line, '\n');
        *q = '\0';

        QString content = line;
        QStringList list = content.split(" ");

        list.removeAll("");

        if (list.size() >= 3)
            version = list.at(2);
    }

    free(line);
    pclose(pp);
    return version;
#endif
}

void CMainWnd::closeOrHidden()
{
    return;
}

void CMainWnd::checkTheme()
{
    if(QGSettings::isSchemaInstalled("org.ukui.style"))
    {
        themeData = new QGSettings("org.ukui.style");

        if(themeData->get("style-name").toString() == "ukui-dark" || themeData->get("style-name").toString() == "ukui-black")
        {
            Globals::themeColor = 1;
            Q_EMIT sigThemeChanged();
            // changeDarkTheme();
        }
        else
        {
            Globals::themeColor = 0;
            Q_EMIT sigThemeChanged();
            // changeLightTheme();
        }

        Globals::fontSize = themeData->get(UKUI_FONT_SIZE).toString().toFloat();

        connect(themeData, &QGSettings::changed, this, [=](const QString &key) {
            if(key == "styleName")
            {
                if((themeData->get("style-name").toString() == "ukui-dark" || themeData->get("style-name").toString() == "ukui-black"))
                {
                    if(Globals::themeColor != 1)
                    {
                        Globals::themeColor = 1;
                        Q_EMIT sigThemeChanged();
                        // changeDarkTheme();
                    }
                }
                else
                {
                    if(Globals::themeColor != 0)
                    {
                        Globals::themeColor = 0;
                        Q_EMIT sigThemeChanged();
                        // changeLightTheme();
                    }
                }
            }
            else if(key == UKUI_FONT_SIZE)
            {
                //获取字号的值
                int fontSizeKey = themeData->get(UKUI_FONT_SIZE).toString().toFloat();
                //发送改变信号
                if(fontSizeKey > 0)
                {
                    Globals::fontSize = fontSizeKey;
                    Q_EMIT Globals::getinstance()->fontsizeChanged(fontSizeKey);
                }
            }
        });
    }
}

void CMainWnd::keepWindowAbove(bool needTokeepAbove)
{
    return;
}

void CMainWnd::setMovable(bool isMovable)
{
    return;
}

void CMainWnd::setWindowAlwaysOnTop(bool checked)
{
    qInfo() << "setWindowAlwaysOnTop " << checked;

    // 如果状态相同，不需要更新
    if (m_isTop == checked) {
        return;
    }

    m_isTop = checked;

    // 使用 X11WindowHelper 设置窗口置顶 (Linux X11 环境)
    if (m_x11WindowHelper && m_x11WindowHelper->isX11Environment()) {
        QWindow *window = this->window()->windowHandle();
        if (window) {
            m_x11WindowHelper->setWindowAlwaysOnTop(window, checked);
            qInfo() << "X11WindowHelper: Set always on top to" << checked;
        } else {
            qWarning() << "X11WindowHelper: Failed to get QWindow handle";
        }
    } else {
        // 非 X11 环境使用原有的 keepWindowAbove
        keepWindowAbove(checked);
    }

    ConfigManager::getInstance()->updateConfig("is-window-mode-topping", checked);


    // 发出信号通知QML更新状态
    Q_EMIT sigWindowAlwaysOnTopChanged(m_isTop);
}

void CMainWnd::onNewConversation()
{
//    Q_EMIT m_msgPane->newConversation(g_showPage);
    return;
}

