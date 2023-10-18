#include "view-backend-private.h"

#include <algorithm>
#include <android/hardware_buffer.h>
#include <cstdint>
#include <errno.h>

#include "ipc-messages.h"
#include "logging.h"
#include "renderer-host-private.h"

namespace WPEAndroid {

AndroidViewBackend* toAndroidViewBackend(WPEAndroidViewBackend* backend)
{
    return reinterpret_cast<AndroidViewBackend*>(backend);
}

Buffer* toAndroidBuffer(WPEAndroidBuffer* buffer)
{
    return reinterpret_cast<Buffer*>(buffer);
}


AndroidViewBackend::AndroidViewBackend(uint32_t initialWidth, uint32_t initialHeight)
    : m_initialWidth(initialWidth), m_initialHeight(initialHeight) { }

void AndroidViewBackend::setCommitBufferCallback(void* context, WPEAndroidViewBackend_CommitBuffer func)
{
    m_commitBufferCallback = [context, func](Buffer *buffer, int fenceID){
        WPEAndroidBuffer* b = reinterpret_cast<WPEAndroidBuffer*>(buffer);
        func(context, b, fenceID);
    };
}

void AndroidViewBackend::commitBuffer(Buffer* buffer, int fenceID)
{
    m_commitBufferCallback(buffer, fenceID);
}

ViewBackend::ViewBackend(AndroidViewBackend *androidViewBackend, WPEViewBackend* wpeViewBackend)
    : m_androidViewBackend(androidViewBackend), m_wpeViewBackend(wpeViewBackend) { }

ViewBackend::~ViewBackend()
{
    while (!m_poolIds.empty())
        unregisterPool(m_poolIds.front());

    m_ipcHost.deinitialize();
    m_androidViewBackend = nullptr;
    m_wpeViewBackend = nullptr;
}

void ViewBackend::initialize()
{
    m_ipcHost.initialize(*this);
    wpe_view_backend_dispatch_set_size(wpeBackend(),
        m_androidViewBackend->initialWidth(), m_androidViewBackend->initialHeight());
}

void ViewBackend::frameComplete()
{
    // Assume that last released buffer is from the completed frame
    RendererHost::instance().frameComplete();

    wpe_view_backend_dispatch_frame_displayed(wpeBackend());
}

void ViewBackend::releaseBuffer(Buffer* buffer)
{
    RendererHost::instance().releaseBuffer(buffer);
}

void ViewBackend::registerPool(uint32_t poolId)
{
    m_poolIds.push_back(poolId);
    RendererHost::instance().registerViewBackend(poolId, this);
}

void ViewBackend::unregisterPool(uint32_t poolId)
{
    ALOGV("ViewBackend::unregisterPool() %d", poolId);
     auto it = std::find(m_poolIds.begin(), m_poolIds.end(), poolId);
    if (it == m_poolIds.end())
        return;

    m_poolIds.erase(it);
    RendererHost::instance().unregisterViewBackend(poolId);
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    ALOGV("ViewBackend::handleMessage() %p[%zu]", data, size);
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::RegisterPool::code:
    {
        auto registerPoolMsg = IPC::RegisterPool::from(message);
        registerPool(registerPoolMsg.poolID);
        break;
    }
    case IPC::UnregisterPool::code:
    {
        auto unregisterPoolMsg = IPC::UnregisterPool::from(message);
        unregisterPool(unregisterPoolMsg.poolID);
        break;
    }
    default:
        ALOGE("ViewBackend: invalid message");
        break;
    }
}

} // namespace WPEAndroid

extern "C" {

struct wpe_view_backend_interface android_view_backend_impl = {
    // create
    [] (void* data, WPEViewBackend* wpeBackend) -> void*
    {
        ALOGD("android_view_backend_impl::create()");
        auto* androidBackend = static_cast<WPEAndroid::AndroidViewBackend*>(data);
        auto* impl = new WPEAndroid::ViewBackend(androidBackend, wpeBackend);
        androidBackend->setImpl(impl);
        return impl;
    },
    // destroy
    [] (void* data)
    {
        ALOGD("android_view_backend_impl::destroy()");
        auto* impl = static_cast<WPEAndroid::ViewBackend*>(data);
        delete impl;
    },
    // initialize
    [] (void* data)
    {
        ALOGD("android_view_backend_impl::initialize()");
        auto& impl = *static_cast<WPEAndroid::ViewBackend*>(data);
        impl.initialize();
    },
    // get_renderer_host_fd
    [] (void* data) -> int
    {
        auto& impl = *static_cast<WPEAndroid::ViewBackend*>(data);
        return impl.ipcHost().releaseClientFD();
    },
};

__attribute__((visibility("default")))
WPEAndroidViewBackend* WPEAndroidViewBackend_create(uint32_t width, uint32_t height)
{
    auto* backend = new WPEAndroid::AndroidViewBackend(width, height);
    wpe_view_backend_create_with_backend_interface(&android_view_backend_impl, backend);
    return reinterpret_cast<WPEAndroidViewBackend*>(backend);
}

__attribute__((visibility("default")))
void WPEAndroidViewBackend_destroy(WPEAndroidViewBackend* backend)
{
    auto* androidViewBackend = WPEAndroid::toAndroidViewBackend(backend);
    wpe_view_backend_destroy(androidViewBackend->impl()->wpeBackend());
    delete androidViewBackend;
}

__attribute__((visibility("default")))
WPEViewBackend* WPEAndroidViewBackend_getWPEViewBackend(WPEAndroidViewBackend* backend)
{
    auto* androidViewBackend = WPEAndroid::toAndroidViewBackend(backend);
    return androidViewBackend->impl()->wpeBackend();
}

__attribute__((visibility("default")))
void WPEAndroidViewBackend_dispatchReleaseBuffer(WPEAndroidViewBackend* backend, WPEAndroidBuffer* buffer)
{
    auto* androidViewBackend = WPEAndroid::toAndroidViewBackend(backend);
    auto* androidBuffer = WPEAndroid::toAndroidBuffer(buffer);
    androidViewBackend->impl()->releaseBuffer(androidBuffer);
}

__attribute__((visibility("default")))
void WPEAndroidViewBackend_dispatchFrameComplete(WPEAndroidViewBackend* backend)
{
    auto* androidViewBackend = WPEAndroid::toAndroidViewBackend(backend);
    androidViewBackend->impl()->frameComplete();
}

__attribute__((visibility("default")))
void WPEAndroidViewBackend_setCommitBufferHandler(WPEAndroidViewBackend* backend, void* context, WPEAndroidViewBackend_CommitBuffer func)
{
    auto* androidViewBackend = WPEAndroid::toAndroidViewBackend(backend);
    androidViewBackend->setCommitBufferCallback(context, func);
}

__attribute__((visibility("default")))
AHardwareBuffer* WPEAndroidBuffer_getAHardwareBuffer(WPEAndroidBuffer* buffer)
{
    auto* androidBuffer = WPEAndroid::toAndroidBuffer(buffer);
    return androidBuffer->hardwareBuffer();
}

} // extern "C"
