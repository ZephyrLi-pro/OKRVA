#ifndef SCRIBEWORDPAGE_H
#define SCRIBEWORDPAGE_H

#include <memory>
#include <QWidget>

using namespace std;

class ScribeWordPage : public QWidget
{
    Q_OBJECT

public:
    explicit ScribeWordPage(QWidget *parent = nullptr);
    ~ScribeWordPage();
    void disableScribeWords();
    void toggleScribeSetting();

signals:
    void sigEnableScribeWordsSwitch();
    void sigDisableScribeWordsSwitch();

private slots:

private:
    void initLayout();
    void initConnection();

private:
    class Private;
    shared_ptr<Private> d_ptr;
};

#endif // SCRIBEWORDPAGE_H
