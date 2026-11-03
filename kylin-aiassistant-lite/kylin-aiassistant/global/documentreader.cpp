#include "documentreader.h"

#include "officetextextractor.h"
#include "pdftextextractor.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <ukui-log4qt.h>

DocumentContent DocumentReader::read(const QString &filePath) const
{
    DocumentContent content;
    if (filePath.trimmed().isEmpty()) {
        content.error = QStringLiteral("File path is empty");
        return content;
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        content.error = QStringLiteral("File does not exist or is not readable");
        return content;
    }

    const QString suffix = fileInfo.suffix().toLower();
    const QString mimeType = QMimeDatabase().mimeTypeForFile(fileInfo).name();
    KyInfo() << "DocumentReader type:" << mimeType << "suffix:" << suffix;

    if (suffix == QStringLiteral("txt") || suffix == QStringLiteral("md")
        || mimeType == QStringLiteral("text/plain")
        || mimeType == QStringLiteral("text/markdown")) {
        return readPlainText(filePath);
    }

    if (suffix == QStringLiteral("pdf") || mimeType == QStringLiteral("application/pdf")) {
#ifdef PDF_SUPPORT_ENABLED
        return PdfTextExtractor::extract(filePath);
#else
        content.error = QStringLiteral("PDF text extraction is disabled for this architecture");
        return content;
#endif
    }

    if (suffix == QStringLiteral("doc") || suffix == QStringLiteral("docx")
        || suffix == QStringLiteral("wps") || suffix == QStringLiteral("ppt")
        || suffix == QStringLiteral("pptx")) {
        return OfficeTextExtractor::extract(filePath);
    }

    content.error = QStringLiteral("Unsupported document type: %1").arg(suffix);
    return content;
}

QString DocumentReader::getAllText(const QString &filePath) const
{
    const DocumentContent content = read(filePath);
    if (!content.error.isEmpty()) {
        KyInfo() << "DocumentReader failed for" << filePath << ":" << content.error;
    }
    return content.text;
}

DocumentContent DocumentReader::readPlainText(const QString &filePath) const
{
    DocumentContent content;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        content.error = QStringLiteral("Failed to open text file: %1").arg(file.errorString());
        return content;
    }

    content.text = QString::fromUtf8(file.readAll());
    if (!content.text.isEmpty() && content.text.front() == QChar::ByteOrderMark) {
        content.text.remove(0, 1);
    }
    if (content.text.trimmed().isEmpty()) {
        content.error = QStringLiteral("The text file is empty");
    }
    return content;
}
