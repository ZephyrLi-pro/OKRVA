#ifndef CTEXTEDITEX_H
#define CTEXTEDITEX_H

#include <QObject>
#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QHBoxLayout>

class CTextEditEx : public QTextEdit
{
public:
    CTextEditEx(QWidget* parent = nullptr);
};

#endif // CTEXTEDITEX_H
