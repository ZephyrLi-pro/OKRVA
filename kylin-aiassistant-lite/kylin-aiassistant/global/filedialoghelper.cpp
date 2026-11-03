#include "filedialoghelper.h"

#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFileInfoList>
#include <QUrl>
#include <QVariant>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QStyle>
#include <QWidget>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include "kaglobal.h"

#include "cmainwnd.h"

FileDialogHelper::FileDialogHelper(QObject *parent)
    : QObject(parent)
{
}

void FileDialogHelper::centerDialogOnDesktop(QWidget *dialog)
{
    if (!dialog)
        return;
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect avail = screen->availableGeometry();
    dialog->adjustSize();
    QSize sz = dialog->sizeHint();
    if (sz.isEmpty() || sz.width() < 200)
        sz = QSize(900, 600);
    dialog->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, sz, avail));
}

// 静态工具：为 QFileDialog 添加 U 盘等挂载点到侧边栏，并监听变化
void FileDialogHelper::setupRemovableSidebar(QFileDialog &dialog)
{
    QList<QUrl> baseSidebar = dialog.sidebarUrls();
    int sidebarNum = 8; // 最大添加 U 盘数，可以自己定义
    QString home = QDir::homePath().section("/", -1, -1);
    QString mnt = QStringLiteral("/media/") + home + QStringLiteral("/");

    QDir mntDir(mnt);
    mntDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList filist = mntDir.entryInfoList();
    QList<QUrl> removableUrls;
    for (int i = 0; i < sidebarNum && i < filist.size(); ++i) {
        QFileInfo fi = filist.at(i);
        // 华为990、9a0需要屏蔽最小系统挂载的目录
        if (fi.fileName() == QLatin1String("2691-6AB8"))
            continue;
        removableUrls << QUrl(QStringLiteral("file://") + fi.filePath());
    }

    auto *fsw = new QFileSystemWatcher(&dialog);
    fsw->addPath(mnt);

    QObject::connect(fsw, &QFileSystemWatcher::directoryChanged, &dialog,
                     [sidebarNum, &baseSidebar, &dialog](const QString &path) {
        QDir dir(path);
        dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        QFileInfoList list = dir.entryInfoList();
        QList<QUrl> urls;
        for (int i = 0; i < sidebarNum && i < list.size(); ++i) {
            QFileInfo fi = list.at(i);
            if (fi.fileName() == QLatin1String("2691-6AB8"))
                continue;
            urls << QUrl(QStringLiteral("file://") + fi.filePath());
        }
        dialog.setSidebarUrls(baseSidebar + urls);
        dialog.update();
    });

    QObject::connect(&dialog, &QFileDialog::finished, &dialog, [&dialog, baseSidebar]() {
        dialog.setSidebarUrls(baseSidebar);
    });

    dialog.setSidebarUrls(baseSidebar + removableUrls);
}

QString FileDialogHelper::openSupportedFile(int typeNum, QWidget *parent)
{
    FileType type = FileType(typeNum);

    QFileDialog dialog(parent);
    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);  // 确保窗口模态
    dialog.setWindowTitle(QObject::tr("Open File"));
    dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::HideNameFilterDetails);

    setupRemovableSidebar(dialog);

    QString format;
    const QStringList supportDoc = g_supportDocFormats;
    const QStringList supportPic = g_supportPicFormats;

    if (type == IMAGE) {
        // 图片
        format = "(";
        for (const QString &str : supportPic)
            format += "*." + str + " ";
        format += ");;";

        const QString supportFormatsClassify =
                "(*.bmp);;"
                "(*.jpe *.jpeg *.jpg);;"
                "(*.pbm);;"
                "(*.pgm);;"
                "(*.png);;"
                "(*.ppm);;"
                "(*.tiff);;"
                "(*.webp)";
        format += supportFormatsClassify;
    } else if (type == DOCUMENT) {
        // 文档
        format = "(";
        for (const QString &str : supportDoc)
            format += "*." + str + " ";
        format += ");;";

        const QString supportFormatsClassify =
                "(*.txt);;"
                "(*.doc *.docx);;"
                "(*.pdf);;"
                "(*.wps);;"
                "(*.pptx *.ppt);;"
                "(*.ofd)";
        format += supportFormatsClassify;
    } else {
        // 0 或其它值：全部支持类型
        format = "(";
        QString imageFormat = "(";
        QString docFormat = "(";

        for (const QString &str : supportDoc) {
            docFormat += "*." + str + " ";
            format += "*." + str + " ";
        }
        docFormat += ");;";

        for (const QString &str : supportPic) {
            imageFormat += "*." + str + " ";
            format += "*." + str + " ";
        }
        imageFormat += ");;";
        format += ");;";

        const QString supportFormatsClassify =
                "(*.txt);;"
                "(*.doc *.docx);;"
                "(*.pdf);;"
                "(*.wps);;"
                "(*.pptx *.ppt);;"
                "(*.ofd);;"
                "(*.bmp);;"
                "(*.jpe *.jpeg *.jpg);;"
                "(*.pbm);;"
                "(*.pgm);;"
                "(*.png);;"
                "(*.ppm);;"
                "(*.tiff);;"
                "(*.webp)";

        format = format + imageFormat + docFormat + supportFormatsClassify;
    }

    dialog.setNameFilter(format);

    // 使用 exec() 而不是静态方法，确保正确的父子窗口关系
    if (dialog.exec() == QDialog::Accepted) {
        QStringList files = dialog.selectedFiles();
        if (!files.isEmpty()) {
            return files.first();
        }
    }

    return QString();
}

bool FileDialogHelper::saveImage(const QString &base64Image)
{
    // 获取主窗口作为父窗口（Wayland 环境下需要）
    QWidget *parent = nullptr;
    CMainWnd *mainWnd = CMainWnd::getInstance();
    if (mainWnd) {
        parent = mainWnd;
    } else {
        // 如果没有主窗口，尝试获取活动窗口
        parent = QApplication::activeWindow();
    }
    
    return FileDialogHelper::saveImage(base64Image, parent);
}

bool FileDialogHelper::saveImage(const QString &base64Image, QWidget *parent)
{
    bool ok = false;

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
            delete fileDialog;
            return false;
        }

        if(fileName.suffix().isNull())
        {
            savePath += QLatin1String(".png");
        }
        fileName = savePath;
        QString name = fileName.fileName();
        if(!name.startsWith(QChar('.'), Qt::CaseInsensitive) &&
           !name.contains(QChar('/')) &&
           name.length() <= 255)
        {
            QFileInfo fileInfo(savePath);
            QString suffix = fileInfo.suffix().toLower();

            // 去掉 data:image/*;base64, 前缀（避免主线程 split/解码耗时）
            // 解码+保存放到后台，避免阻塞 UI。前缀截断也尽量放到后台，减少主线程大块拷贝。
            auto *watcher = new QFutureWatcher<bool>(parent);
            connect(watcher, &QFutureWatcher<bool>::finished, watcher, [watcher, savePath, parent]() {
                bool saveOk = watcher->result();
                watcher->deleteLater();

                if (!saveOk) {
                    QMessageBox saveErrBox(QMessageBox::Warning,
                                            QObject::tr("Save Error"),
                                            QObject::tr("Error trying to save as ") + savePath);
                    saveErrBox.setParent(parent);
                    saveErrBox.exec();
                }
            });

            auto future = QtConcurrent::run([base64Image, savePath, suffix]() -> bool {
                int commaIndex = base64Image.indexOf(',');
                if (commaIndex == -1) {
                    return false;
                }

                QByteArray decodedData = QByteArray::fromBase64(base64Image.mid(commaIndex + 1).toLatin1());
                QImage originalImage;
                if (!originalImage.loadFromData(decodedData)) {
                    return false;
                }

                // 需要白底填充的格式（与原逻辑保持一致）
                if (suffix == "jpeg" || suffix == "pgm" || suffix == "pbm" || suffix == "ppm" ||
                    suffix == "jpg" || suffix == "bmp") {
                    QImage newImage(originalImage.size(), QImage::Format_RGB32);
                    newImage.fill(Qt::white);
                    QPainter painter(&newImage);
                    painter.drawImage(0, 0, originalImage);
                    painter.end();
                    return newImage.save(savePath);
                }

                // jpe 保存会报错，额外处理
                if (suffix == "jpe") {
                    return originalImage.save(savePath, "JPEG");
                }

                return originalImage.save(savePath);
            });
            watcher->setFuture(future);

            ok = true; // 用户已确认保存路径；实际写入结果在后台完成后反馈
        } else {
            QString msg;
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
                msg = QObject::tr("Error trying to save as ") + savePath;
            }
            QMessageBox saveErrBox(QMessageBox::Warning, QObject::tr("Save Error"), msg);
            saveErrBox.exec();
        }
    }
    
    delete fileDialog;
    return ok;
}


