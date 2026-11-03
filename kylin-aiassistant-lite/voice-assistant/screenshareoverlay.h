#ifndef SCREENSHAREOVERLAY_H
#define SCREENSHAREOVERLAY_H

#include <QWidget>

class QScreen;
class QShowEvent;

/**
 * Fullscreen translucent overlay showing corner brackets (recording-style cue).
 * Mouse events pass through to windows below via WA_TransparentForMouseEvents.
 */
class ScreenShareOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenShareOverlay(QScreen *screen, QWidget *parent = nullptr);

    void reposition();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void applyScreenBinding();

    QScreen *m_screen = nullptr;
};

#endif // SCREENSHAREOVERLAY_H
