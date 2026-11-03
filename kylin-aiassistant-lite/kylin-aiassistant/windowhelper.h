#ifndef WINDOWHELPER_H
#define WINDOWHELPER_H

#ifdef UKUI_WINDOWHELPER_SUPPORT
    #include <ukuiwindowhelper/ukuiwindowhelper.h>
#else
    #include <ukuistylehelper/ukuistylehelper.h>
    using namespace kdk;
#endif

#endif // WINDOWHELPER_H
