#ifndef SELECTIONPOPUPGUARD_H
#define SELECTIONPOPUPGUARD_H

#include <QString>
#include <Qt>

namespace SelectionPopupGuard {
inline bool shouldShow(const QString &text, const QString &initial,
                       const QString &handled, bool validSelection, bool assistantFocused)
{
    // Selection deduplication must not depend on a desktop SDK build flag.
    return validSelection && !assistantFocused && !text.isEmpty()
            && text != initial && text != handled;
}

inline Qt::WindowFlags popupFlags(Qt::WindowFlags flags, bool acceptFocus)
{
    // Qt::Tool contains the Window type bit: clearing Qt::Window alone corrupts
    // the type. Replace the entire type mask, leaving other hints unchanged.
    flags = (flags & ~Qt::WindowType_Mask) | Qt::Tool | Qt::WindowStaysOnTopHint;
    return acceptFocus ? flags & ~Qt::WindowDoesNotAcceptFocus
                       : flags | Qt::WindowDoesNotAcceptFocus;
}
}

#endif
