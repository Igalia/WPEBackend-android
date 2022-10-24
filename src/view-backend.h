#pragma once

#include <android/hardware_buffer.h>
#include <wpe-android/view-backend-exportable.h>
#include <vector>

#include "ipc.h"

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

    ClientBundle* clientBundle() const { return m_clientBundle; }

    void initialize();

    void frameComplete();
    void releaseBuffer(AHardwareBuffer*, uint32_t, uint32_t);

private:

    void registerPool(uint32_t poolId);
    void unregisterPool(uint32_t poolId);

    // IPC::Host::Handler
    void handleMessage(char*, size_t) override;

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    IPC::Host m_ipcHost;

    std::vector<uint32_t> m_poolIds;
};

} // namespace Exportable
