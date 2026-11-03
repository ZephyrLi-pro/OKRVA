#ifndef PDFTEXTEXTRACTOR_H
#define PDFTEXTEXTRACTOR_H

#include "documentcontent.h"

class PdfTextExtractor
{
public:
    static DocumentContent extract(const QString &filePath, bool addPageMarkers = false);
};

#endif // PDFTEXTEXTRACTOR_H
