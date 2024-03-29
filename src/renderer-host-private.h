/**
 * Copyright (C) 2024 Igalia S.L. <info@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

struct AHardwareBuffer;

namespace WPEAndroid {

class RendererHostClientProxy;
class ViewBackend;


class Buffer {
public:
    Buffer(AHardwareBuffer* hardwareBuffer, uint32_t poolID, uint32_t bufferID);
    ~Buffer();

    AHardwareBuffer* hardwareBuffer() const { return m_hardwareBuffer; }
    uint32_t bufferID() const { return m_bufferID; }
    uint32_t poolID() const { return m_poolID; }

    bool locked() const { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }

    bool pendingDelete() const { return m_pendingDelete; }
    void setSPendingDelete(bool pendingDelete) { m_pendingDelete = pendingDelete; }

private:

    AHardwareBuffer* m_hardwareBuffer;
    uint32_t m_bufferID;
    uint32_t m_poolID;
    bool m_locked;
    bool m_pendingDelete;
};

class BufferPool {
public:
    BufferPool(uint32_t id, RendererHostClientProxy* client);

    uint32_t id() const { return m_id; }

    RendererHostClientProxy* client() const { return m_client; }

    size_t size() const { return m_buffers.size(); }

    Buffer* getBuffer(int bufferId) const { return m_buffers[bufferId]; }
    void setBuffer(int bufferId, Buffer* buffer) { m_buffers[bufferId] = buffer; }

    Buffer* releaseBuffer(int bufferId);

private:
    uint32_t m_id;
    RendererHostClientProxy* m_client;
    std::array<Buffer*, 4> m_buffers;
};

class RendererHost final {
public:
    RendererHost();

    static RendererHost& instance();

    int createClient();

    uint32_t createBufferPool(RendererHostClientProxy* client);

    BufferPool* findBufferPool(uint32_t);

    void registerViewBackend(uint32_t poolId, ViewBackend* viewBackend);
    void unregisterViewBackend(uint32_t poolId);

    ViewBackend* findViewBackend(uint32_t);

    void releaseBuffer(Buffer* buffer);
    void frameComplete();

private:

    // (poolId -> BufferPool)
    std::unordered_map<uint32_t, BufferPool*> m_bufferPoolMap;

    // (poolId -> ViewBackend)
    std::unordered_map<uint32_t, ViewBackend*> m_viewBackendMap;

    std::vector<RendererHostClientProxy*> m_clients;
};

} // namespace WPEAndroid
