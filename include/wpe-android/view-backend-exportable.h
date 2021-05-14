#ifndef wpebackend_android_view_backend_exportable_h
#define wpebackend_android_view_backend_exportable_h

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/wpe.h>

typedef struct AHardwareBuffer AHardwareBuffer;

struct wpe_android_view_backend_exportable;

struct wpe_android_view_backend_exportable_client {
    void (*export_buffer)(void* data, AHardwareBuffer*, uint32_t poolID, uint32_t bufferID);

    void (*_wpe_reserved0)(void);
    void (*_wpe_reserved1)(void);
    void (*_wpe_reserved2)(void);
    void (*_wpe_reserved3)(void);
};

struct wpe_android_view_backend_exportable*
wpe_android_view_backend_exportable_create(const struct wpe_android_view_backend_exportable_client*, void*, uint32_t width, uint32_t height);

void
wpe_android_view_backend_exportable_destroy(struct wpe_android_view_backend_exportable*);

struct wpe_view_backend*
wpe_android_view_backend_exportable_get_view_backend(struct wpe_android_view_backend_exportable*);

void
wpe_android_view_backend_exportable_dispatch_frame_complete(struct wpe_android_view_backend_exportable*);

void
wpe_android_view_backend_exportable_dispatch_release_buffer(struct wpe_android_view_backend_exportable*, AHardwareBuffer*, uint32_t poolID, uint32_t bufferID);

#ifdef __cplusplus
}
#endif

#endif // wpebackend_android_view_backend_exportable_h
