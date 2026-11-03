#include "officetextextractor.h"

#include "pdftextextractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

namespace {
constexpr int kLibreOfficeTimeoutMs = 120000;

QString findLibreOffice()
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    }
    return program;
}

QString findConvertedFile(const QString &directory, const QString &suffix)
{
    const QFileInfoList candidates = QDir(directory).entryInfoList(
        {QStringLiteral("*.%1").arg(suffix)}, QDir::Files, QDir::Time);
    return candidates.isEmpty() ? QString() : candidates.constFirst().absoluteFilePath();
}

bool convertWithLibreOffice(const QString &program,
                            const QString &filePath,
                            const QString &outputFilter,
                            const QString &workDirectory,
                            QString *error)
{
    const QString profileDirectory = QDir(workDirectory).filePath(QStringLiteral("profile"));
    if (!QDir().mkpath(profileDirectory)) {
        *error = QStringLiteral("Failed to create a temporary LibreOffice profile");
        return false;
    }

    const QString profileUrl = QUrl::fromLocalFile(profileDirectory).toString(QUrl::FullyEncoded);
    const QStringList arguments {
        QStringLiteral("--headless"),
        QStringLiteral("--nologo"),
        QStringLiteral("--nodefault"),
        QStringLiteral("--norestore"),
        QStringLiteral("--nolockcheck"),
        QStringLiteral("-env:UserInstallation=%1").arg(profileUrl),
        QStringLiteral("--convert-to"),
        outputFilter,
        QStringLiteral("--outdir"),
        workDirectory,
        filePath
    };

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SAL_USE_VCLPLUGIN"), QStringLiteral("svp"));
    process.setProcessEnvironment(environment);
    process.start(program, arguments);
    if (!process.waitForStarted(5000)) {
        *error = QStringLiteral("Failed to start LibreOffice: %1").arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(kLibreOfficeTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        *error = QStringLiteral("LibreOffice conversion timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        *error = stderrText.isEmpty()
            ? QStringLiteral("LibreOffice exited with code %1").arg(process.exitCode())
            : stderrText;
        return false;
    }
    return true;
}
}

DocumentContent OfficeTextExtractor::extract(const QString &filePath)
{
    DocumentContent content;
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    const bool isWriterDocument = suffix == QStringLiteral("doc")
        || suffix == QStringLiteral("docx")
        || suffix == QStringLiteral("wps");
    const bool isPresentation = suffix == QStringLiteral("ppt")
        || suffix == QStringLiteral("pptx");

    if (!isWriterDocument && !isPresentation) {
        content.error = QStringLiteral("Unsupported Office document type: %1").arg(suffix);
        return content;
    }

    const QString program = findLibreOffice();
    if (program.isEmpty()) {
        content.error = QStringLiteral("LibreOffice is not installed");
        return content;
    }

    QTemporaryDir temporaryDirectory(
        QDir::tempPath() + QStringLiteral("/kylin-aiassistant-office-XXXXXX"));
    if (!temporaryDirectory.isValid()) {
        content.error = QStringLiteral("Failed to create a temporary Office conversion directory");
        return content;
    }

    const QString outputFilter = isWriterDocument
        ? QStringLiteral("txt:Text (encoded):UTF8")
        : QStringLiteral("pdf:impress_pdf_Export");
    if (!convertWithLibreOffice(program, filePath, outputFilter,
                                temporaryDirectory.path(), &content.error)) {
        return content;
    }

    if (isWriterDocument) {
        const QString textFile = findConvertedFile(temporaryDirectory.path(), QStringLiteral("txt"));
        QFile file(textFile);
        if (textFile.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            content.error = QStringLiteral("LibreOffice did not produce a readable text file");
            return content;
        }
        content.text = QString::fromUtf8(file.readAll()).trimmed();
        if (content.text.isEmpty()) {
            content.error = QStringLiteral("No native text was found in the Office document");
        }
        return content;
    }

    const QString pdfFile = findConvertedFile(temporaryDirectory.path(), QStringLiteral("pdf"));
    if (pdfFile.isEmpty()) {
        content.error = QStringLiteral("LibreOffice did not produce a PDF file");
        return content;
    }
    return PdfTextExtractor::extract(pdfFile, true);
}
