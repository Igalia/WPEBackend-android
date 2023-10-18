#include "interfaces.h"

#include <array>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <cstdint>
#include <errno.h>
#include <unordered_map>


#include "ipc.h"
#include "ipc-messages.h"
#include "logging.h"

struct Buffer {
    uint32_t bufferID { 0 };
    bool locked { false };
    AHardwareBuffer* object { nullptr };

    struct {
        EGLImageKHR image { EGL_NO_IMAGE_KHR };
    } egl;

    struct {
        GLuint colorBuffer { 0 };
        GLuint dsBuffer { 0 };
    } gl;
};

class EGLTarget;

class RendererBackend final : public IPC::Client::Handler {
public:
    RendererBackend(int fd);
    ~RendererBackend();

    IPC::Client& ipc() { return m_ipcClient; }

    void registerEGLTarget(uint32_t poolId, EGLTarget*);
    void unregisterEGLTarget(uint32_t poolId);

private:

    // IPC::Client::Handle
    void handleMessage(char*, size_t) override;

    IPC::Client m_ipcClient;

    // (poolId -> EGLTarget)
    std::unordered_map<uint32_t, EGLTarget*> m_targetMap;
};

class EGLTarget : public IPC::Client::Handler {
public:
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(RendererBackend* backend, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);

    void frameWillRender();
    void frameRendered();

    void deinitialize();

    void releaseBuffer(uint32_t, uint32_t);

    // IPC::Client::Handle
    void handleMessage(char*, size_t) override;

    struct wpe_renderer_backend_egl_target* target;

    RendererBackend* m_backend { nullptr };

    IPC::Client ipcClient;

    struct {
        bool initialized { false };
        uint32_t width { 0 };
        uint32_t height { 0 };

        PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC getNativeClientBufferANDROID;
        PFNEGLCREATEIMAGEKHRPROC createImageKHR;
        PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR;
        PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC imageTargetRenderbufferStorageOES;
        PFNEGLCREATESYNCKHRPROC createSyncKHR;
        PFNEGLDESTROYSYNCKHRPROC destroySyncKHR;
        PFNEGLDUPNATIVEFENCEFDANDROIDPROC dupNativeFenceFDANDROID;

        GLuint framebuffer { 0 };
    } renderer;

    struct {
        Buffer* current { nullptr };

        uint32_t poolID { 0 };
        std::array<Buffer, 4> pool;
    } buffers;
};

static void destroyBufferPool(std::array<Buffer, 4>& pool, PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR)
{
    for (auto& buffer : pool) {
        if (buffer.gl.colorBuffer)
            glDeleteRenderbuffers(1, &buffer.gl.colorBuffer);
        if (buffer.gl.dsBuffer)
            glDeleteRenderbuffers(1, &buffer.gl.dsBuffer);
        buffer.gl = { };

        if (buffer.egl.image)
            destroyImageKHR(eglGetCurrentDisplay(), buffer.egl.image);
        buffer.egl = { };

        if (buffer.object)
            AHardwareBuffer_release(buffer.object);

        buffer.locked = false;
        buffer.object = nullptr;
    }
}

RendererBackend::RendererBackend(int fd) {
    m_ipcClient.initialize(*this, fd);
}

RendererBackend::~RendererBackend() {
    m_ipcClient.deinitialize();
}

void RendererBackend::registerEGLTarget(uint32_t poolId, EGLTarget* target) {
    m_targetMap.insert({poolId, target});
}

void RendererBackend::unregisterEGLTarget(uint32_t poolId) {
    auto it = m_targetMap.find(poolId);
    if (it != m_targetMap.end()) {
        m_targetMap.erase(it);
    }
}

void RendererBackend::handleMessage(char* data, size_t size) {
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::FrameComplete::code:
    {   auto frameComplete = IPC::FrameComplete::from(message);
        ALOGV("RendererBackend::handleMessage(): FrameComplete { poolID %u }", frameComplete.poolID);
        auto it = m_targetMap.find(frameComplete.poolID);
        if (it == m_targetMap.end()) {
            // This situation can happen if during intensive rendering page is destroyed while frame is still
            // being processed by UIProcess. This used to be g_error but we must not crash in such situation.
            g_warning("RendererBackend - Cannot find buffer pool with poolId %" PRIu32 " in renderer backend.", frameComplete.poolID);
            return;
        }

        wpe_renderer_backend_egl_target_dispatch_frame_complete(it->second->target);
        break;
    }
    case IPC::ReleaseBuffer::code:
    {
        auto release = IPC::ReleaseBuffer::from(message);
        ALOGV("RendererBackend::handleMessage(): BufferRelease { poolID %u, bufferID %u }", release.poolID, release.bufferID);
        auto it = m_targetMap.find(release.poolID);
        if (it == m_targetMap.end()) {
            // This situation can happen if during intensive rendering page is destroyed while frame is still
            // being processed by UIProcess. This used to be g_error but we must not crash in such situation.
            g_warning("RendererBackend - Cannot find buffer pool with poolId %" PRIu32 " in renderer backend.", release.poolID);
            return;
        }

        it->second->releaseBuffer(release.poolID, release.bufferID);
        break;
    }
    default:
        ALOGV("RendererBackend: invalid message");
        break;
    }
}

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
{
    ipcClient.initialize(*this, hostFd);

    for (auto& buffer : buffers.pool)
        buffer.bufferID = uint32_t(std::distance(buffers.pool.begin(), &buffer));
}

EGLTarget::~EGLTarget()
{
    IPC::UnregisterPool unregisterPool;
    unregisterPool.poolID = buffers.poolID;

    IPC::Message message;
    IPC::UnregisterPool::construct(message, unregisterPool);
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);

    ipcClient.deinitialize();

    m_backend->unregisterEGLTarget(buffers.poolID);
    for (auto& buffer : buffers.pool) {
        if (buffer.object)
            AHardwareBuffer_release(buffer.object);
    }
}

void EGLTarget::initialize(RendererBackend* backend, uint32_t width, uint32_t height)
{
    ALOGD("EGLTarget::initialize() (%u,%u)", width, height);
    m_backend = backend;
    renderer.width = width;
    renderer.height = height;

    IPC::PoolConstruction poolConstruction;

    IPC::Message message;
    IPC::PoolConstruction::construct(message, poolConstruction);

    // We can safely assume that returned message is PoolConstructionReply because there are no
    // other messages coming this way in this socket at this point
    backend->ipc().sendAndReceiveMessage(IPC::Message::data(message), IPC::Message::size, [&] (char* data, size_t size) {
        ALOGV("EGLTarget::initialize - handleMessage() %p[%zu]", data, size);
        if (size != IPC::Message::size)
            return;

        auto& message = IPC::Message::cast(data);
        switch (message.messageCode) {
            case IPC::PoolConstructionReply::code:
            {
                auto reply = IPC::PoolConstructionReply::from(message);
                ALOGV("  PoolConstructionReply: poolID %u", reply.poolID);

                buffers.poolID = reply.poolID;

                m_backend->registerEGLTarget(buffers.poolID, this);

                IPC::RegisterPool registerPool;
                registerPool.poolID = reply.poolID;

                IPC::Message message;
                IPC::RegisterPool::construct(message, registerPool);
                ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
            }
        }
    });
}

void EGLTarget::resize(uint32_t width, uint32_t height)
{
    if (renderer.width == width && renderer.height == height)
        return;
    ALOGV("EGLTarget::resize() (%u,%u)", width, height);
    renderer.width = width;
    renderer.height = height;

    destroyBufferPool(buffers.pool, renderer.destroyImageKHR);

    IPC::PoolPurge poolPurge;
    poolPurge.poolID = buffers.poolID;

    IPC::Message message;
    IPC::PoolPurge::construct(message, poolPurge);
    m_backend->ipc().sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::frameWillRender()
{
    if (!renderer.initialized) {
        renderer.initialized = true;

        renderer.getNativeClientBufferANDROID = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
            eglGetProcAddress("eglGetNativeClientBufferANDROID"));
        renderer.createImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
            eglGetProcAddress("eglCreateImageKHR"));
        renderer.destroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
            eglGetProcAddress("eglDestroyImageKHR"));
        renderer.imageTargetRenderbufferStorageOES = reinterpret_cast<PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC>(
            eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES"));
        renderer.createSyncKHR = reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(
            eglGetProcAddress("eglCreateSyncKHR"));
        renderer.destroySyncKHR = reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(
            eglGetProcAddress("eglDestroySyncKHR"));
        renderer.dupNativeFenceFDANDROID = reinterpret_cast<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>(
            eglGetProcAddress("eglDupNativeFenceFDANDROID"));

        GLuint framebuffer { 0 };
        glGenFramebuffers(1, &framebuffer);
        renderer.framebuffer = framebuffer;

        ALOGV("  initialized, entrypoints %p/%p/%p/%p, framebuffer %u",
            renderer.getNativeClientBufferANDROID, renderer.createImageKHR, renderer.destroyImageKHR, renderer.imageTargetRenderbufferStorageOES,
            renderer.framebuffer);
    }

    for (auto& buffer : buffers.pool) {
        if (buffer.locked)
            continue;

        buffers.current = &buffer;
        break;
    }
    if (!buffers.current) {
        ALOGV("  no available current-buffer found");
        std::abort();
        return;
    }

    auto& current = *buffers.current;

    if (!current.object) {
        AHardwareBuffer_Desc description;
        description.width = renderer.width;
        description.height = renderer.height;
        description.layers = 1;
        description.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        description.usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;
        description.stride = description.rfu0 = description.rfu1 = 0;

        int ret = AHardwareBuffer_allocate(&description, &current.object);
        if (!!ret || !current.object) {
            ALOGV("  failed to allocate AHardwareBuffer: ret %d", ret);
            return;
        }

        EGLClientBuffer clientBuffer = renderer.getNativeClientBufferANDROID(current.object);
        current.egl.image = renderer.createImageKHR(eglGetCurrentDisplay(),
            EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, nullptr);

        std::array<GLuint, 2> renderbuffers { 0, 0 };
        glGenRenderbuffers(2, renderbuffers.data());
        current.gl.colorBuffer = renderbuffers[0];
        current.gl.dsBuffer = renderbuffers[1];

        glBindRenderbuffer(GL_RENDERBUFFER, current.gl.colorBuffer);
        renderer.imageTargetRenderbufferStorageOES(GL_RENDERBUFFER, current.egl.image);

        glBindRenderbuffer(GL_RENDERBUFFER, current.gl.dsBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, renderer.width, renderer.height);

        {
            IPC::BufferAllocation allocation;
            allocation.poolID = buffers.poolID;
            allocation.bufferID = current.bufferID;

            IPC::Message message;
            IPC::BufferAllocation::construct(message, allocation);
            m_backend->ipc().sendMessage(IPC::Message::data(message), IPC::Message::size);

            while (true) {
                int ret = AHardwareBuffer_sendHandleToUnixSocket(current.object, m_backend->ipc().socketFd());
                if (!ret || ret != -EAGAIN)
                    break;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, renderer.framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, current.gl.colorBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, current.gl.dsBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, current.gl.dsBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ALOGV("EGLTarget: GL_FRAMEBUFFER not COMPLETE");
}

void EGLTarget::frameRendered()
{
    EGLSyncKHR sync = renderer.createSyncKHR(eglGetCurrentDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);

    glFlush();

    int syncFd = -1;
    if (sync != EGL_NO_SYNC_KHR) {
        // native fence fd will not be populated until flush() is done
        syncFd = renderer.dupNativeFenceFDANDROID(eglGetCurrentDisplay(), sync);
        if (syncFd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
            ALOGV("EGLTarget: EGL_NO_NATIVE_FENCE_FD_ANDROID");
        }
        renderer.destroySyncKHR(eglGetCurrentDisplay(), sync);
    } else {
        ALOGV("EGLTarget: EGL_NO_SYNC_KHR");
    }

    if (buffers.current->object) {
        IPC::BufferCommit commit;
        commit.poolID = buffers.poolID;
        commit.bufferID = buffers.current->bufferID;

        IPC::Message message;
        IPC::BufferCommit::construct(message, commit);
        m_backend->ipc().sendMessage(IPC::Message::data(message), IPC::Message::size);
        m_backend->ipc().sendFileDescriptor(syncFd);
    }

    buffers.current->locked = true;
    buffers.current = nullptr;
}

void EGLTarget::deinitialize()
{
    ALOGD("EGLTarget::deinitialize()");
    destroyBufferPool(buffers.pool, renderer.destroyImageKHR);

    if (renderer.framebuffer)
        glDeleteFramebuffers(1, &renderer.framebuffer);
    renderer.framebuffer = 0;
}

void EGLTarget::releaseBuffer(uint32_t poolID, uint32_t bufferID)
{
    if (buffers.poolID != poolID)
        return;

    for (auto& buffer : buffers.pool) {
        if (buffer.bufferID == bufferID) {
            buffer.locked = false;
            break;
        }
    }
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    // TODO:
}

struct wpe_renderer_backend_egl_interface android_renderer_backend_egl_impl = {
    // create
    [] (int host_fd) -> void*
    {
        ALOGD("android_renderer_backend_egl_impl::create()");
        return new RendererBackend(host_fd);
    },
    // destroy
    [] (void* data)
    {
        ALOGD("android_renderer_backend_egl_impl::destroy()");
        auto* backend = static_cast<RendererBackend*>(data);
        delete backend;
    },
    // get_native_display
    [] (void*) -> EGLNativeDisplayType
    {
        ALOGV("android_renderer_backend_egl_impl::get_native_display()");
        return EGL_DEFAULT_DISPLAY;
    },
    // get_platform
    [] (void*) -> uint32_t
    {
        ALOGV("android_renderer_backend_egl_impl::get_platform()");
        return EGL_PLATFORM_SURFACELESS_MESA;
    },
};

struct wpe_renderer_backend_egl_target_interface android_renderer_backend_egl_target_impl = {
    // create
    [] (struct wpe_renderer_backend_egl_target* target, int hostFd) -> void*
    {
        ALOGD("android_renderer_backend_egl_target_impl::create() fd %d", hostFd);
        return new EGLTarget(target, hostFd);
    },
    // destroy
    [] (void* data)
    {
        ALOGD("android_renderer_backend_egl_target_impl::destroy()");
        auto* target = static_cast<EGLTarget*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto* target = static_cast<EGLTarget*>(data);
        auto* backend = static_cast<RendererBackend*>(backend_data);
        target->initialize(backend, width, height);
    },
    // get_native_window
    [] (void*) -> EGLNativeWindowType
    {
        ALOGV("android_renderer_backend_egl_target_impl::get_native_window()");
        return nullptr;
    },
    // resize
    [] (void* data, uint32_t width, uint32_t height)
    {
        ALOGV("android_renderer_backend_egl_target_impl::resize() (%u,%u)", width, height);
        static_cast<EGLTarget*>(data)->resize(width, height);
    },
    // frame_will_render
    [] (void* data)
    {
        static_cast<EGLTarget*>(data)->frameWillRender();
    },
    // frame_rendered
    [] (void* data)
    {
        static_cast<EGLTarget*>(data)->frameRendered();
    },
    // deinitialize
    [] (void* data)
    {
        ALOGD("android_renderer_backend_egl_target_impl::deinitialize()");
        static_cast<EGLTarget*>(data)->deinitialize();
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface android_renderer_backend_egl_offscreen_target_impl = {
    // create
    [] () -> void*
    {
        return nullptr;
    },
    // destroy
    [] (void*)
    { },
    // initialize
    [] (void*, void*)
    { },
    // get_native_window
    [] (void*) -> EGLNativeWindowType
    {
        return nullptr;
    },
};
