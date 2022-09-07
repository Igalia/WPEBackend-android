#pragma once

#include "view-backend.h"
#include <android/hardware_buffer.h>
#include <array>
#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

class RendererHostClientProxy;

class BufferPool {
public:
    BufferPool(uint32_t id, RendererHostClientProxy* client);

    uint32_t id() const { return m_id; }

    RendererHostClientProxy* client() const { return m_client; }

    size_t size() const { return m_buffers.size(); }

    AHardwareBuffer* getBuffer(int bufferId) { return m_buffers[bufferId]; }
    void setBuffer(int bufferId, AHardwareBuffer* buffer) { m_buffers[bufferId] = buffer; }

private:
    uint32_t m_id;
    RendererHostClientProxy* m_client;
    std::array<AHardwareBuffer*, 4> m_buffers;
};

class RendererHost final {
public:
    RendererHost();

    static RendererHost& instance();

    int createClient();

    uint32_t createBufferPool(RendererHostClientProxy* client);

    BufferPool* findBufferPool(uint32_t);

    void registerViewBackend(uint32_t poolId, Exportable::ViewBackend* viewBackend);
    void unregisterViewBackend(uint32_t poolId);

    Exportable::ViewBackend* findViewBackend(uint32_t);

    void releaseBuffer(AHardwareBuffer* buffer, uint32_t poolID, uint32_t bufferID);
    void frameComplete();

private:

    // (poolId -> BufferPool)
    std::unordered_map<uint32_t, BufferPool*> m_bufferPoolMap;

    // (poolId -> ViewBackend)
    std::unordered_map<uint32_t, Exportable::ViewBackend*> m_viewBackendMap;

    std::vector<RendererHostClientProxy*> m_clients;
};
