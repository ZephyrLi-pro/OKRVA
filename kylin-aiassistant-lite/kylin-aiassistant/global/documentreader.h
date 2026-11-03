#ifndef DOCUMENTREADER_H
#define DOCUMENTREADER_H

#include "documentcontent.h"

class DocumentReader
{
public:
    DocumentContent read(const QString &filePath) const;
    QString getAllText(const QString &filePath) const;

private:
    DocumentContent readPlainText(const QString &filePath) const;
};

#endif // DOCUMENTREADER_H
