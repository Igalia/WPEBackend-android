#include <wpe-android/view-backend-exportable.h>

#include "ipc.h"
#include "ipc-android.h"
#include "logging.h"

#include <android/hardware_buffer.h>
#include <array>
#include <errno.h>

namespace Exportable {

class ViewBackend;

struct ClientBundle {
    const struct wpe_android_view_backend_exportable_client* client;
    void* data;

    ViewBackend* viewBackend;
    uint32_t width;
    uint32_t height;
};

class ViewBackend : public IPC::Host::Handler {
public:
    ViewBackend(ClientBundle*, struct wpe_view_backend*);
    virtual ~ViewBackend();

    IPC::Host& ipcHost() { return m_ipcHost; }

    void initialize();

    void constructPool(uint32_t);
    void bufferAllocation(AHardwareBuffer* buffer, uint32_t, uint32_t);
    void bufferCommit(uint32_t, uint32_t);

    void frameComplete();
    void releaseBuffer(AHardwareBuffer*, uint32_t, uint32_t);

private:
    // IPC::Host::Handler
    void handleMessage(char*, size_t) override;

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    IPC::Host m_ipcHost;

    struct {
        uint32_t poolID { 0 };
        std::array<AHardwareBuffer*, 4> buffers;
    } m_bufferPool;
};

ViewBackend::ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
    : m_clientBundle(clientBundle)
    , m_backend(backend)
{
    m_clientBundle->viewBackend = this;
    m_ipcHost.initialize(*this);

    m_bufferPool.buffers = { nullptr, nullptr, nullptr, nullptr };
}

ViewBackend::~ViewBackend()
{
    for (auto* buffer : m_bufferPool.buffers) {
        if (buffer)
            AHardwareBuffer_release(buffer);
    }
    m_bufferPool.buffers = { nullptr, nullptr, nullptr, nullptr };

    m_ipcHost.deinitialize();
    m_clientBundle->viewBackend = nullptr;
    m_backend = nullptr;
    m_clientBundle = nullptr;
}

void ViewBackend::initialize()
{
    wpe_view_backend_dispatch_set_size(m_backend, m_clientBundle->width, m_clientBundle->height);
}

void ViewBackend::constructPool(uint32_t poolID)
{
    for (auto* buffer : m_bufferPool.buffers) {
        if (buffer)
            AHardwareBuffer_release(buffer);
    }

    m_bufferPool.poolID = poolID;
    m_bufferPool.buffers = { nullptr, nullptr, nullptr, nullptr };
}

void ViewBackend::bufferAllocation(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID)
{
    if (poolID != m_bufferPool.poolID) {
        if (buffer)
            AHardwareBuffer_release(buffer);
        return;
    }

    if (bufferID >= m_bufferPool.buffers.size()) {
        if (buffer)
            AHardwareBuffer_release(buffer);
        return;
    }

    if (m_bufferPool.buffers[bufferID])
        AHardwareBuffer_release(m_bufferPool.buffers[bufferID]);
    m_bufferPool.buffers[bufferID] = buffer;
}

void ViewBackend::bufferCommit(uint32_t poolID, uint32_t bufferID)
{
    if (poolID != m_bufferPool.poolID)
        return;
    if (bufferID >= m_bufferPool.buffers.size())
        return;

    auto* buffer = m_bufferPool.buffers[bufferID];
    ALOGV("  BufferCommit: committing pool buffer %p\n", buffer);

    if (m_clientBundle->client && m_clientBundle->client->export_buffer)
        m_clientBundle->client->export_buffer(m_clientBundle->data, buffer, poolID, bufferID);
}

void ViewBackend::frameComplete()
{
    IPC::Message message;
    IPC::FrameComplete::construct(message);
    m_ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void ViewBackend::releaseBuffer(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID)
{
    IPC::ReleaseBuffer release;
    release.poolID = poolID;
    release.bufferID = bufferID;

    IPC::Message message;
    IPC::ReleaseBuffer::construct(message, release);
    m_ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    ALOGV("ViewBackend::handleMessage() %p[%zu]", data, size);
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::PoolConstruction::code:
    {
        auto poolConstruction = IPC::PoolConstruction::from(message);
        ALOGV("  PoolConstruction: poolID %u", poolConstruction.poolID);
        constructPool(poolConstruction.poolID);
        break;
    }
    case IPC::BufferAllocation::code:
    {
        auto allocation = IPC::BufferAllocation::from(message);
        ALOGV("  BufferAllocation: poolID %u, bufferID %u, current pool ID %u",
            allocation.poolID, allocation.bufferID, m_bufferPool.poolID);

        AHardwareBuffer* buffer = nullptr;
        int ret = 0;
        while (true) {
            ret = AHardwareBuffer_recvHandleFromUnixSocket(m_ipcHost.socketFd(), &buffer);
            if (!ret || ret != -EAGAIN)
                break;
        }
        ALOGV("  BufferAllocation: ret %d, buffer %p\n", ret, buffer);

        bufferAllocation(buffer, allocation.poolID, allocation.bufferID);
        break;
    }
    case IPC::BufferCommit::code:
    {
        auto commit = IPC::BufferCommit::from(message);
        ALOGV("  BufferCommit: poolID %u, bufferID %u", commit.poolID, commit.bufferID);
        bufferCommit(commit.poolID, commit.bufferID);
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
