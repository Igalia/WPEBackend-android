#include "renderer-host.h"

#include "interfaces.h"

#include <cstdint>
#include <memory>
#include <unistd.h>

#include "ipc.h"
#include "ipc-android.h"
#include "logging.h"


uint32_t m_lastExportedPoolID = 0;

class RendererHostClientProxy final : public IPC::Host::Handler {
public:
    RendererHostClientProxy(RendererHost& host);
    ~RendererHostClientProxy();

    int releaseClientFD();

    IPC::Host& ipc() { return m_ipcHost; }

private:

    void constructPool();
    void purgePool(uint32_t poolId);
    void bufferAllocation(AHardwareBuffer* buffer, uint32_t, uint32_t);
    void bufferCommit(uint32_t, uint32_t);

    // IPC::Host::Handle
    void handleMessage(char*, size_t) override;

    RendererHost& m_host;

    IPC::Host m_ipcHost;
};

// BufferPool

BufferPool::BufferPool(uint32_t id, RendererHostClientProxy* client)
    : m_id(id), m_client(client) {
    m_buffers = { nullptr, nullptr, nullptr, nullptr };
}

// RendereHost

RendererHost::RendererHost() = default;

RendererHost& RendererHost::instance() {
    static RendererHost host;
    return host;
}

// There's one client created per webprocess
int RendererHost::createClient() {
    ALOGD("RendererHost::createClient()");

    auto* clientProxy = new RendererHostClientProxy(*this);
    m_clients.push_back(clientProxy);
    return clientProxy->releaseClientFD();
}

uint32_t RendererHost::createBufferPool(RendererHostClientProxy* client) {
    ALOGD("RendererHost::createBufferPool()");
    static uint32_t poolID = 0;

    auto* bufferPool = new BufferPool(poolID++, client);
    m_bufferPoolMap.insert({ bufferPool->id(), bufferPool });
    return bufferPool->id();
}

BufferPool* RendererHost::findBufferPool(uint32_t poolID) {
    auto it = m_bufferPoolMap.find(poolID);
    if (it == m_bufferPoolMap.end()) {
        ALOGW("RendererHost::findBufferPool(): " "Cannot find buffer pool with poolId %" PRIu32 " in render host.", poolID);
        return nullptr;
    }
    return it->second;
}

void RendererHost::registerViewBackend(uint32_t poolId, Exportable::ViewBackend* viewBackend) {
    m_viewBackendMap.insert({ poolId, viewBackend });
}

void RendererHost::unregisterViewBackend(uint32_t poolId) {
    auto it = m_viewBackendMap.find(poolId);
    if (it != m_viewBackendMap.end()) {
        m_viewBackendMap.erase(it);
    }
}

Exportable::ViewBackend* RendererHost::findViewBackend(uint32_t poolId) {
    auto it = m_viewBackendMap.find(poolId);
    if (it == m_viewBackendMap.end()) {
        ALOGW("RendererHost::findViewBackend(): " "Cannot find view backend with poolId %" PRIu32 " in render host.", poolId);
        return nullptr;
    }
    return it->second;
}

void RendererHost::releaseBuffer(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID) {
    auto* bufferPool = findBufferPool(poolID);

    IPC::ReleaseBuffer release;
    release.poolID = poolID;
    release.bufferID = bufferID;

    IPC::Message message;
    IPC::ReleaseBuffer::construct(message, release);
    bufferPool->client()->ipc().sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void RendererHost::frameComplete() {
    auto* bufferPool = findBufferPool(m_lastExportedPoolID);

    IPC::FrameComplete frameComplete;
    frameComplete.poolID = m_lastExportedPoolID;

    IPC::Message message;
    IPC::FrameComplete::construct(message, frameComplete);
    bufferPool->client()->ipc().sendMessage(IPC::Message::data(message), IPC::Message::size);
}

// RendereHostClientProxy

RendererHostClientProxy::RendererHostClientProxy(RendererHost& host)
    : m_host(host) {
    m_ipcHost.initialize(*this);
}

RendererHostClientProxy::~RendererHostClientProxy() {
    m_ipcHost.deinitialize();
}

int RendererHostClientProxy::releaseClientFD() {
    return m_ipcHost.releaseClientFD(true);
}

void RendererHostClientProxy::constructPool()
{
    uint32_t poolID = m_host.createBufferPool(this);

    IPC::PoolConstructionReply poolConstructionReply;
    poolConstructionReply.poolID = poolID;

    IPC::Message message;
    IPC::PoolConstructionReply::construct(message, poolConstructionReply);
    m_ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void RendererHostClientProxy::purgePool(uint32_t poolId) {
    auto* bufferPool = m_host.findBufferPool(poolId);

    for(int i=0; i<bufferPool->size(); i++) {
        auto* buffer = bufferPool->getBuffer(i);
        if (buffer)
            AHardwareBuffer_release(buffer);
        bufferPool->setBuffer(i, nullptr);
    }
}

void RendererHostClientProxy::bufferAllocation(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID)
{
    auto* bufferPool = m_host.findBufferPool(poolID);

    if (bufferID >= bufferPool->size()) {
        if (buffer)
            AHardwareBuffer_release(buffer);
        return;
    }

    if (bufferPool->getBuffer(bufferID))
        AHardwareBuffer_release(bufferPool->getBuffer(bufferID));
    bufferPool->setBuffer(bufferID, buffer);
}

void RendererHostClientProxy::bufferCommit(uint32_t poolID, uint32_t bufferID)
{
    auto* bufferPool = m_host.findBufferPool(poolID);

    if (bufferID >= bufferPool->size())
        return;

    auto* buffer = bufferPool->getBuffer(bufferID);
    ALOGV(" RendererHostClientProxy::bufferCommit: committing pool buffer %p\n", buffer);

    // TODO: This is only temprorary solution for PSON support. To make this work correctly
    // interface change is needed where we received pool id from frame complete callback.
    m_lastExportedPoolID = poolID;

    auto* viewBackend = m_host.findViewBackend(poolID);
    if (viewBackend) {
        auto* clientBundle = viewBackend->clientBundle();
        if (clientBundle->client && clientBundle->client->export_buffer)
            clientBundle->client->export_buffer(clientBundle->data, buffer, poolID, bufferID);
    } else {
        // In some cases viewbackend might have been already destroyed when buffer commit message
        // is dispatched from ipc queue. It means that webview is already destroyed or being destroyed
        // and all IPC is being torn down.
        //
        // In such case all we can do is to release the buffer
        if (buffer) {
            AHardwareBuffer_release(buffer);
            bufferPool->setBuffer(bufferID, nullptr);
        }
    }
}

void RendererHostClientProxy::handleMessage(char*data, size_t size) {
    ALOGV("RendererHostClientProxy::handleMessage() %p[%zu]", data, size);
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::PoolConstruction::code:
    {
        ALOGV("  PoolConstruction");
        constructPool();
        break;
    }
    case IPC::PoolPurge::code:
    {
        auto purge = IPC::PoolPurge::from(message);
        ALOGV("  PoolPurge: poolID %d", purge.poolID);
        purgePool(purge.poolID);
        break;
    }
    case IPC::BufferAllocation::code:
    {
        auto allocation = IPC::BufferAllocation::from(message);

        ALOGV("  BufferAllocation: poolID %u, bufferID %u", allocation.poolID, allocation.bufferID);

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
        ALOGV("RendererHostClientProxy: invalid message");
        break;
    }
}

struct wpe_renderer_host_interface android_renderer_host_impl = {
    // create
    [] () -> void* {
        ALOGD("wpe_renderer_host_interface - create");
        // libwpe stores value returned by this call as static singleton without any other usage.
        // This is called once prior to wpe_renderer_host_create_client calls.
        // Renderer host can be considered as singleton and thus no need to return anything
        // as libwpe doesn't use returned value. This is similar approach to wpebackend-fdo.
        return nullptr;
    },
    // destroy
    [] (void* data) {
        // wpewebkit/libwpe never calls this
    },
    // create_client
    [] (void* data) -> int {
        ALOGD("wpe_renderer_host_interface - create_client");
        return RendererHost::instance().createClient();
    },
};
