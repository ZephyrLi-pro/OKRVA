#include "documentitemwidget.h"
#include <QFontMetrics>
#include <QPixmap>

DocumentItemWidget::DocumentItemWidget(QWidget *parent) : QPushButton(parent)
{
    this->setFixedHeight(36);
    this->setMinimumWidth(348);
    this->setMaximumWidth(360);
    initLayout();
    initConnect();
    this->setObjectName("documentItemWidget");
    this->setStyleSheet("#documentItemWidget {"
                        "background-color: transparent;"
                        "border-radius: 6px;"
                        "}"
                        "#documentItemWidget:hover {"
                        "background-color: rgba(0, 0, 0, 0.05);"
                        "}"
                        "#documentItemWidget:pressed {"
                        "background-color: rgba(0, 0, 0, 0.08);"
                        "}");
}

void DocumentItemWidget::initLayout()
{
    QFont minFont;
    minFont.setPointSize(Globals::fontSize - 2);

    m_iconLabel = new QLabel();
    m_iconLabel->setAccessibleName("kylin-aiassistant_QLabel_m_iconLabel");
    m_iconLabel->setPixmap(QPixmap(QIcon::fromTheme("application-pdf").pixmap(16, 16)));
    m_iconLabel->setStyleSheet("background:transparent;");

    m_nameLabel = new QLabel();
    m_nameLabel->setAccessibleName("kylin-aiassistant_QLabel_m_nameLabel");
    m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setStyleSheet("background:transparent;");

    m_sizeLabel = new QLabel();
    m_sizeLabel->setAccessibleName("kylin-aiassistant_QLabel_m_sizeLabel");
    m_sizeLabel->setText(tr("Ready"));
    m_sizeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sizeLabel->setFont(minFont);
    m_sizeLabel->setStyleSheet("background:transparent;");

    m_deleteButton = new QPushButton();
    m_deleteButton->setAccessibleName("kylin-aiassistant_QPushButton_m_deleteBtn");
    m_deleteButton->setFlat(true);
    m_deleteButton->setFixedSize(24, 24);
    m_deleteButton->setProperty("isWindowButton", 0x2);
    m_deleteButton->setProperty("useIconHighlightEffect", 0x8);
    m_deleteButton->setIcon(QIcon::fromTheme("edit-delete-symbolic"));
    m_deleteButton->setToolTip(tr("Delete"));

    QHBoxLayout *m_hLayout = new QHBoxLayout();
    m_hLayout->setContentsMargins(8, 0, 8, 0);
    m_hLayout->setSpacing(8);
    m_hLayout->setAlignment(Qt::AlignVCenter);
    m_hLayout->addWidget(m_iconLabel);
    m_hLayout->addWidget(m_nameLabel);
    m_hLayout->addStretch();
    m_hLayout->addWidget(m_sizeLabel);
    m_hLayout->addWidget(m_deleteButton);

    this->setLayout(m_hLayout);
    return;
}

void DocumentItemWidget::initConnect()
{
    connect(m_deleteButton, &QPushButton::clicked, this, &DocumentItemWidget::onDeleteClicked);
    connect(Globals::getinstance(), &Globals::fontsizeChanged, this, [this] (float fontSize){
        if(fontSize <=0) return;

        if(!m_nameLabel || !m_sizeLabel)
        {
            return;
        }

        QFont font;
        font.setPointSize(fontSize);

        QFont minFont;
        minFont.setPointSize(std::max(1.0f, fontSize - 2));

        m_nameLabel->setFont(font);
        m_sizeLabel->setFont(minFont);
    });
    return;
}

void DocumentItemWidget::onDeleteClicked()
{
    if (m_item) {
        emit sigDeleteBtnClicked(m_item->documentPath);
    }
    return;
}

bool DocumentItemWidget::event(QEvent* event)
{
    switch (event->type()) {
        case QEvent::Enter:
            m_isHovered = true;
            showHoverStatus();
            break;
        case QEvent::Leave:
            m_isHovered = false;
            showDefaultStatus();
            break;
        default:
            break;
    }

    return QWidget::event(event);
}

void DocumentItemWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    return;
}

void DocumentItemWidget::setData(DocumentItem* document)
{
    if (document == nullptr) {
        return;
    }

    m_item = document;
    QIcon icon = QIcon::fromTheme(document->documentType, QIcon::fromTheme("text-x-generic"));
    m_iconLabel->setPixmap(QPixmap(icon.pixmap(16, 16)));
    m_sizeLabel->setText(Globals::formatFileSize(document->documentSize));

    if (m_isHovered) {
        showHoverStatus();
    } else {
        showDefaultStatus();
    }
    return;
}

void DocumentItemWidget::showHoverStatus()
{
    m_deleteButton->setVisible(true);
    adjustLabelWidth(true);
    return;
}

void DocumentItemWidget::showDefaultStatus()
{
    m_deleteButton->setVisible(false);
    adjustLabelWidth(false);
    return;
}

void DocumentItemWidget::adjustLabelWidth(bool isShowDelete)
{
    if (!m_item) return;

    m_sizeLabel->setFixedWidth(72);

    int sizeWidth = m_sizeLabel->fontMetrics().horizontalAdvance(m_sizeLabel->text());
    if (sizeWidth > 72) {
        elidedText(m_sizeLabel, m_sizeLabel->text());
    }

    m_nameLabel->setFixedWidth(isShowDelete ? 152 : 184);
    elidedText(m_nameLabel, m_item->documentName);
    return;
}

void DocumentItemWidget::elidedText(QLabel *label, QString str)
{
    QFontMetrics metrics(label->font());
    if (metrics.horizontalAdvance(str) > label->width()) {
        label->setToolTip(str);
        str = QFontMetrics(label->font()).elidedText(str, Qt::ElideRight, label->width());
    }

    label->setText(str);
    return;
}
