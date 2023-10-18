#pragma once

#include <cstdint>
#include <vector>

#include <wpe-android/view-backend.h>

#include "ipc.h"

struct AHardwareBuffer;

namespace WPEAndroid {

class Buffer;
class ViewBackend;

class AndroidViewBackend {
public:

    AndroidViewBackend(uint32_t initialWidth, uint32_t initialHeight);

    uint32_t initialWidth() const { return m_initialWidth; }
    uint32_t initialHeight() const { return m_initialHeight; }

    ViewBackend* impl() const { return m_impl; }
    void setImpl(ViewBackend* impl) { m_impl = impl; }

    void setCommitBufferCallback(void* context, WPEAndroidViewBackend_CommitBuffer func);

    void commitBuffer(Buffer* buffer, int fenceID);

private:

    ViewBackend *m_impl = nullptr;

    uint32_t m_initialWidth;
    uint32_t m_initialHeight;

    using CommitBufferCallback = std::function<void(Buffer* buffer, int fenceID)>;
    CommitBufferCallback m_commitBufferCallback;
};

class ViewBackend : public IPC::Host::Handler {
public:
    ViewBackend(AndroidViewBackend* androidViewBackend, WPEViewBackend* wpeViewBackend);
    virtual ~ViewBackend();

    void initialize();

    IPC::Host& ipcHost() { return m_ipcHost; }

    AndroidViewBackend* androidBackend() const { return m_androidViewBackend; }

    WPEViewBackend* wpeBackend() const { return m_wpeViewBackend; }

    void setWPEBackend(WPEViewBackend* backend);

    void frameComplete();
    void releaseBuffer(Buffer*);

private:

    void registerPool(uint32_t poolId);
    void unregisterPool(uint32_t poolId);

    // IPC::Host::Handler
    void handleMessage(char*, size_t) override;

    AndroidViewBackend* m_androidViewBackend;
    WPEViewBackend* m_wpeViewBackend;

    IPC::Host m_ipcHost;

    std::vector<uint32_t> m_poolIds;
};

} // namespace WPEAndroid
