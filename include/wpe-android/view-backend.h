#ifndef WPE_ANDROID_VIEW_BACKEND_H
#define WPE_ANDROID_VIEW_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/wpe.h>

typedef struct AHardwareBuffer AHardwareBuffer;

struct WPEAndroidBuffer;
struct WPEAndroidViewBackend;

typedef struct WPEAndroidBuffer WPEAndroidBuffer;
typedef struct WPEAndroidViewBackend WPEAndroidViewBackend;
typedef struct wpe_view_backend WPEViewBackend;

WPEAndroidViewBackend* WPEAndroidViewBackend_create(uint32_t width, uint32_t height);

void WPEAndroidViewBackend_destroy(WPEAndroidViewBackend*);

WPEViewBackend* WPEAndroidViewBackend_getWPEViewBackend(WPEAndroidViewBackend*);

typedef void (*WPEAndroidViewBackend_CommitBuffer)(void* context, WPEAndroidBuffer*, int fenceID);
void WPEAndroidViewBackend_setCommitBufferHandler(WPEAndroidViewBackend*, void* context, WPEAndroidViewBackend_CommitBuffer func);

void WPEAndroidViewBackend_dispatchReleaseBuffer(WPEAndroidViewBackend*, WPEAndroidBuffer*);

void WPEAndroidViewBackend_dispatchFrameComplete(WPEAndroidViewBackend*);

AHardwareBuffer* WPEAndroidBuffer_getAHardwareBuffer(WPEAndroidBuffer*);

#ifdef __cplusplus
}
#endif

#endif // WPE_ANDROID_VIEW_BACKEND_H
