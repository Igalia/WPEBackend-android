#include "logging.h"

#include <android/native_window.h>

extern "C" {

__attribute__((visibility("default")))
void libwpe_android_provideNativeWindow(ANativeWindow* nativeWindow)
{
    ALOGV("libwpe_android_provideNativeWindow() nativeWindow %p size (%d,%d) -- DOES NOTHING",
        nativeWindow,
        ANativeWindow_getWidth(nativeWindow), ANativeWindow_getHeight(nativeWindow));
}

}
