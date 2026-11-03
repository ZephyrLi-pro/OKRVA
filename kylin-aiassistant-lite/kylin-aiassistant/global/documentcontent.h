#ifndef DOCUMENTCONTENT_H
#define DOCUMENTCONTENT_H

#include <QString>

struct DocumentContent
{
    QString text;
    int pageCount = 0;
    QString error;

    bool isValid() const
    {
        return error.isEmpty() && !text.trimmed().isEmpty();
    }
};

#endif // DOCUMENTCONTENT_H
