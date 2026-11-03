#ifndef MINIPANEL_H
#define MINIPANEL_H

#include <QMetaObject>
#include <QWidget>

namespace Ui {
class MiniPanel;
}

class QScreen;
class ScreenShareOverlay;
class MiniPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MiniPanel(QWidget *parent = nullptr);
    ~MiniPanel();

    void setSubtitleChecked(bool checked);
    void setMuteChecked(bool checked);
    void setScreenShareActive(bool active);
    bool screenShareActive() const { return m_screenShareActive; }
    bool muteChecked() const;
    void setCallbackMode(bool enabled);
    void setMiniMode(bool enabled);

signals:
    void subtitleToggled(bool checked);
    void screenShareToggled(bool enabled);
    void cutoffClicked();
    void muteToggled(bool muted);
    void answerClicked();
    void restoreClicked();

private slots:
    void onPrimaryMonitorChangedForScreenShare(QScreen *newPrimary);

    void on_pushButton_screenshare_clicked(bool checked);

    void on_pushButton_mute_clicked(bool checked);

    void on_pushButton_cutoff_clicked();

    void on_pushButton_answer_clicked();

    void on_pushButton_subtitle_clicked();
    void on_pushButton_subtitle_clicked(bool checked);

    void on_pushButton_restore_clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void destroyScreenShareOverlays();
    bool createScreenShareOverlays();
    void updateScreenShareButtonState(bool active);
    void updateSubtitleButtonState(bool checked);
    void updateMuteButtonState(bool checked);
    void updateMiniCutoffButtonIcon();
    void applyMiniModeStyles();
    void applyNormalModeStyles();

    Ui::MiniPanel *ui = nullptr;
    ScreenShareOverlay *m_screenShareOverlay = nullptr;
    QMetaObject::Connection m_primaryScreenGeometryConnection;
    QWidget *m_logoRightSpacer = nullptr;
    bool m_screenShareActive = false;
    bool m_miniMode = false;
    int m_normalLeftMargin = 24;
    int m_normalRightMargin = 24;
    int m_normalSpacing = -1;
};

#endif // MINIPANEL_H
