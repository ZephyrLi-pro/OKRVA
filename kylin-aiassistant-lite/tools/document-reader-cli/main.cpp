#include "documentreader.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 2) {
        QTextStream(stderr) << "Usage: kylin-document-reader FILE\n";
        return 2;
    }
    const DocumentContent document = DocumentReader().read(args.at(1));
    QJsonObject result;
    result["text"] = document.text;
    result["page_count"] = document.pageCount;
    result["error"] = document.error;
    result["ok"] = document.isValid();
    const QByteArray wire = QJsonDocument(result).toJson(QJsonDocument::Compact);
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly) || output.write(wire) != wire.size())
        return 3;
    output.write("\n");
    return document.isValid() ? 0 : 1;
}
