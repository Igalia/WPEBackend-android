#include "renderer-host.h"
#include "view-backend.h"

#include "ipc-android.h"
#include "logging.h"

#include <errno.h>

namespace Exportable {

ViewBackend::ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
    : m_clientBundle(clientBundle)
    , m_backend(backend)
{
    m_clientBundle->viewBackend = this;
    m_ipcHost.initialize(*this);
}

ViewBackend::~ViewBackend()
{
    m_ipcHost.deinitialize();
    m_clientBundle->viewBackend = nullptr;
    m_backend = nullptr;
    m_clientBundle = nullptr;
}

void ViewBackend::initialize()
{
    wpe_view_backend_dispatch_set_size(m_backend, m_clientBundle->width, m_clientBundle->height);
}

void ViewBackend::frameComplete()
{
    // Assume that last released buffer is from the completed frame
    RendererHost::instance().frameComplete();

    wpe_view_backend_dispatch_frame_displayed(m_backend);
}

void ViewBackend::releaseBuffer(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID)
{
    RendererHost::instance().releaseBuffer(buffer, poolID, bufferID);
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    ALOGV("RendererHostClientProxy::handleMessage() %p[%zu]", data, size);
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::RegisterPool::code:
    {
        auto registerPool = IPC::RegisterPool::from(message);
        RendererHost::instance().registerViewBackend(registerPool.poolID, this);
        break;
    }
    case IPC::UnregisterPool::code:
    {
        auto unregisterPool = IPC::UnregisterPool::from(message);
        RendererHost::instance().unregisterViewBackend(unregisterPool.poolID);
        break;
    }
    default:
        ALOGV("ViewBackend: invalid message");
        break;
    }
}

} // namespace Exportable

extern "C" {

struct wpe_android_view_backend_exportable {
    Exportable::ClientBundle* clientBundle;
    struct wpe_view_backend* backend;
};

struct wpe_view_backend_interface android_view_backend_exportable_impl = {
    // create
    [] (void* data, struct wpe_view_backend* backend) -> void*
    {
        auto* clientBundle = static_cast<Exportable::ClientBundle*>(data);
        return new Exportable::ViewBackend(clientBundle, backend);
    },
    // destroy
    [] (void* data)
    {
        auto* backend = static_cast<Exportable::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [] (void* data)
    {
        ALOGV("noop_view_backend_impl::initialize()");
        auto& backend = *static_cast<Exportable::ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [] (void* data) -> int
    {
        auto& backend = *static_cast<Exportable::ViewBackend*>(data);
        return backend.ipcHost().releaseClientFD();
    },
};

__attribute__((visibility("default")))
struct wpe_android_view_backend_exportable*
wpe_android_view_backend_exportable_create(const struct wpe_android_view_backend_exportable_client* client, void* data, uint32_t width, uint32_t height)
{
    auto* clientBundle = new Exportable::ClientBundle { client, data, nullptr, width, height };
    struct wpe_view_backend* backend = wpe_view_backend_create_with_backend_interface(&android_view_backend_exportable_impl, clientBundle);

    auto* exportable = new struct wpe_android_view_backend_exportable;
    exportable->clientBundle = clientBundle;
    exportable->backend = backend;

    return exportable;
}

__attribute__((visibility("default")))
void
wpe_android_view_backend_exportable_destroy(struct wpe_android_view_backend_exportable* exportable)
{
    wpe_view_backend_destroy(exportable->backend);
    delete exportable->clientBundle;
    delete exportable;
}

__attribute__((visibility("default")))
struct wpe_view_backend*
wpe_android_view_backend_exportable_get_view_backend(struct wpe_android_view_backend_exportable* exportable)
{
    return exportable->backend;
}

__attribute__((visibility("default")))
void
wpe_android_view_backend_exportable_dispatch_frame_complete(struct wpe_android_view_backend_exportable* exportable)
{
    exportable->clientBundle->viewBackend->frameComplete();
}

__attribute__((visibility("default")))
void
wpe_android_view_backend_exportable_dispatch_release_buffer(struct wpe_android_view_backend_exportable* exportable, AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID)
{
    exportable->clientBundle->viewBackend->releaseBuffer(buffer, poolID, bufferID);
}

}
