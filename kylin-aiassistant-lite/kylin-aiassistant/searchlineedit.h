#ifndef SEARCHLINEEDIT_H
#define SEARCHLINEEDIT_H

#include <QObject>
#include <QWidget>
#include <QLineEdit>

class SearchLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit SearchLineEdit(QWidget *parent = nullptr);
};

#endif // SEARCHLINEEDIT_H
