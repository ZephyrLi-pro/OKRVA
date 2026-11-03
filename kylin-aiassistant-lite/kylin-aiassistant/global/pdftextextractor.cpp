#include "pdftextextractor.h"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace {
constexpr int kPdfTextTimeoutMs = 120000;

QString processFailure(const QString &program, QProcess &process)
{
    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (!stderrText.isEmpty()) {
        return QStringLiteral("%1: %2").arg(program, stderrText);
    }
    return QStringLiteral("%1 exited with code %2").arg(program).arg(process.exitCode());
}
}

DocumentContent PdfTextExtractor::extract(const QString &filePath, bool addPageMarkers)
{
    DocumentContent content;
    const QString program = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (program.isEmpty()) {
        content.error = QStringLiteral("pdftotext is not installed");
        return content;
    }

    QProcess process;
    process.start(program, {QStringLiteral("-layout"), filePath, QStringLiteral("-")});
    if (!process.waitForStarted(5000)) {
        content.error = QStringLiteral("Failed to start pdftotext: %1").arg(process.errorString());
        return content;
    }
    if (!process.waitForFinished(kPdfTextTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        content.error = QStringLiteral("pdftotext timed out");
        return content;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        content.error = processFailure(program, process);
        return content;
    }

    QString text = QString::fromUtf8(process.readAllStandardOutput());
    QStringList pages = text.split(QChar('\f'), Qt::KeepEmptyParts);
    while (!pages.isEmpty() && pages.constLast().trimmed().isEmpty()) {
        pages.removeLast();
    }
    content.pageCount = pages.size();

    QStringList normalizedPages;
    normalizedPages.reserve(pages.size());
    bool hasNativeText = false;
    for (int index = 0; index < pages.size(); ++index) {
        const QString pageText = pages.at(index).trimmed();
        hasNativeText = hasNativeText || !pageText.isEmpty();
        if (addPageMarkers) {
            normalizedPages.append(QStringLiteral("===== Slide %1 =====\n%2")
                                       .arg(index + 1)
                                       .arg(pageText));
        } else if (!pageText.isEmpty()) {
            normalizedPages.append(pageText);
        }
    }
    content.text = normalizedPages.join(QStringLiteral("\n\n"));
    if (!hasNativeText) {
        content.error = QStringLiteral("No native text was found in the PDF");
    }
    return content;
}
