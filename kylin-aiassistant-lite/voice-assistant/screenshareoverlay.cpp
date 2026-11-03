#include "screenshareoverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
#include <ukui-log4qt.h>

namespace {
constexpr int kBracketLength = 42;
constexpr int kBracketThickness = 4;

const QColor kBracketGreen(0x00, 0xB4, 0x2A, 255);

void paintTopLeft(QPainter &p, int x, int y)
{
    p.fillRect(x, y, kBracketLength, kBracketThickness, kBracketGreen);
    p.fillRect(x, y, kBracketThickness, kBracketLength, kBracketGreen);
}

void paintTopRight(QPainter &p, int outerRight, int y)
{
    const int x0 = outerRight - kBracketThickness;
    const int left = outerRight - kBracketLength;
    p.fillRect(left, y, kBracketLength, kBracketThickness, kBracketGreen);
    p.fillRect(x0, y, kBracketThickness, kBracketLength, kBracketGreen);
}

void paintBottomLeft(QPainter &p, int x, int outerBottom)
{
    const int y0 = outerBottom - kBracketThickness;
    const int top = outerBottom - kBracketLength;
    p.fillRect(x, top, kBracketThickness, kBracketLength, kBracketGreen);
    p.fillRect(x, y0, kBracketLength, kBracketThickness, kBracketGreen);
}

void paintBottomRight(QPainter &p, int outerRight, int outerBottom)
{
    const int xl = outerRight - kBracketLength;
    const int xt = outerRight - kBracketThickness;
    const int yt = outerBottom - kBracketThickness;
    const int ytop = outerBottom - kBracketLength;
    p.fillRect(xl, yt, kBracketLength, kBracketThickness, kBracketGreen);
    p.fillRect(xt, ytop, kBracketThickness, kBracketLength, kBracketGreen);
}
}

ScreenShareOverlay::ScreenShareOverlay(QScreen *screen, QWidget *parent)
    : QWidget(parent)
    , m_screen(screen)
{
    Q_UNUSED(parent)
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_NativeWindow, true);

    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool;
#ifdef Q_OS_LINUX
    flags |= Qt::X11BypassWindowManagerHint;
#endif
    setWindowFlags(flags);

    reposition();
}

void ScreenShareOverlay::applyScreenBinding()
{
    if (!m_screen) {
        KyInfo() << "屏幕共享覆盖层：未关联有效屏幕指针，跳过 setScreen 绑定";
        return;
    }

    QWindow *wnd = windowHandle();
    if (!wnd) {
        if (isVisible()) {
            KyInfo() << "屏幕共享覆盖层：窗口已显示但未获得原生 windowHandle，无法执行 setScreen";
        }
        return;
    }

    QScreen *cur = wnd->screen();
    if (cur != m_screen) {
        KyInfo() << "屏幕共享覆盖层：当前窗口逻辑屏与目标屏不一致，执行 setScreen 绑定";
        wnd->setScreen(m_screen);
    }
}

void ScreenShareOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyScreenBinding();
}

void ScreenShareOverlay::reposition()
{
    if (!m_screen)
        m_screen = QGuiApplication::primaryScreen();
    if (!m_screen) {
        KyInfo() << "屏幕共享覆盖层：无法获取主屏幕，跳过几何与屏幕绑定更新";
        return;
    }

    QRect g = m_screen->geometry();
    setGeometry(g);
    applyScreenBinding();
}

void ScreenShareOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const int inset = kBracketThickness / 2;
    const int iw = width() - inset;
    const int ih = height() - inset;

    paintTopLeft(p, inset, inset);
    paintTopRight(p, iw, inset);
    paintBottomLeft(p, inset, ih);
    paintBottomRight(p, iw, ih);
}
