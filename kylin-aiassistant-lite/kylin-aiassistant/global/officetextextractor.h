#ifndef OFFICETEXTEXTRACTOR_H
#define OFFICETEXTEXTRACTOR_H

#include "documentcontent.h"

class OfficeTextExtractor
{
public:
    static DocumentContent extract(const QString &filePath);
};

#endif // OFFICETEXTEXTRACTOR_H
