#ifndef DOCUMENTITEMWIDGET_H
#define DOCUMENTITEMWIDGET_H

#include "documentlistmodel.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPaintEvent>
#include <QMouseEvent>
#include <global/kaglobal.h>

class DocumentItemWidget : public QPushButton
{
    Q_OBJECT
public:
    explicit DocumentItemWidget(QWidget *parent = nullptr);
    void setData(DocumentItem *document);

protected:
    bool event(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onDeleteClicked();

private:
    void initLayout();
    void initConnect();
    void adjustLabelWidth(bool isShowDelete);
    void showHoverStatus();
    void showDefaultStatus();
    void elidedText(QLabel *label, QString str);

signals:
    void sigDeleteBtnClicked(const QString &documentPath);

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QHBoxLayout *m_hLayout = nullptr;
    DocumentItem *m_item = nullptr;
    bool m_isHovered = false;
};

#endif // DOCUMENTITEMWIDGET_H
