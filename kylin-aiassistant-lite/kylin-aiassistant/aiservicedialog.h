#ifndef AISERVICEDIALOG_H
#define AISERVICEDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>

class AiServiceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AiServiceDialog(QWidget *parent = nullptr);
    ~AiServiceDialog();

    void setLowVersionText();


private:
    void initLayout();
    void initConnect();
    void openUkuiControlCenter();


private:
    QPushButton *closeButton;
    QPushButton *cancelButton;
    QPushButton *goButton;
    QLabel *iconLabel;
    QLabel *titleLabel;
    QLabel *detailLabel;

signals:

};

#endif // AISERVICEDIALOG_H
