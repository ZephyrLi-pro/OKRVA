#ifndef REPLACECONFIRMDIALOG_H
#define REPLACECONFIRMDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QGSettings>
#include <QCheckBox>

class ReplaceConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReplaceConfirmDialog(QString fileName, QString uuid, QWidget *parent = nullptr);
    explicit ReplaceConfirmDialog(QString fileName, QString uuid, bool isSingle = false, QWidget *parent = nullptr);
    ~ReplaceConfirmDialog();
    bool getIsApplyToAll();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void changeTheme();
    void checkTheme();
    void adjustFileName();

private:
    // 布局
    void initLayout();
    // 信号槽
    void initConnect();

    void accept() override;

private:
    QString m_fileName = "";
    QString m_uuid = "";
    QLabel *m_iconLabel = nullptr;
    QLabel *m_messageLabel = nullptr;

    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_acceptBtn = nullptr;

    QCheckBox *m_selectAll = nullptr;

    QVBoxLayout *m_vLayout = nullptr;
    QGSettings *themeData = nullptr; // 监测主题
};


#endif // REPLACECONFIRMDIALOG_H


