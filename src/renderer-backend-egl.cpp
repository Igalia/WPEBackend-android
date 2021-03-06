#include "interfaces.h"

#include "ipc.h"
#include "ipc-android.h"
#include "logging.h"
#include <array>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <errno.h>

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

class EGLTarget : public IPC::Client::Handler {
public:
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);

    void frameWillRender();
    void frameRendered();

    void deinitialize();

    void releaseBuffer(uint32_t, uint32_t);

    // IPC::Client::Handle
    void handleMessage(char*, size_t) override;

    struct wpe_renderer_backend_egl_target* target;

    IPC::Client ipcClient;

    struct {
        bool initialized { false };
        uint32_t width { 0 };
        uint32_t height { 0 };

        PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC getNativeClientBufferANDROID;
        PFNEGLCREATEIMAGEKHRPROC createImageKHR;
        PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR;
        PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC imageTargetRenderbufferStorageOES;

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

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
{
    ipcClient.initialize(*this, hostFd);

    for (auto& buffer : buffers.pool)
        buffer.bufferID = uint32_t(std::distance(buffers.pool.begin(), &buffer));

    {
        IPC::PoolConstruction poolConstruction;
        poolConstruction.poolID = 0;

        IPC::Message message;
        IPC::PoolConstruction::construct(message, poolConstruction);
        ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

EGLTarget::~EGLTarget()
{
    ipcClient.deinitialize();
}

void EGLTarget::initialize(uint32_t width, uint32_t height)
{
    ALOGV("EGLTarget::initialize() (%u,%u)", width, height);
    renderer.width = width;
    renderer.height = height;
}

void EGLTarget::resize(uint32_t width, uint32_t height)
{
    if (renderer.width == width && renderer.height == height)
        return;

    renderer.width = width;
    renderer.height = height;

    destroyBufferPool(buffers.pool, renderer.destroyImageKHR);

    ++buffers.poolID;
    {
        IPC::PoolConstruction poolConstruction;
        poolConstruction.poolID = buffers.poolID;

        IPC::Message message;
        IPC::PoolConstruction::construct(message, poolConstruction);
        ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EGLTarget::frameWillRender()
{
    ALOGV("EGLTarget::frameWillRender(), renderer.initialized %d", renderer.initialized);
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

        GLuint framebuffer { 0 };
        glGenFramebuffers(1, &framebuffer);
        renderer.framebuffer = framebuffer;

        ALOGV("  initialized, entrypoints %p/%p/%p/%p, framebuffer %u",
            renderer.getNativeClientBufferANDROID, renderer.createImageKHR, renderer.destroyImageKHR, renderer.imageTargetRenderbufferStorageOES,
            renderer.framebuffer);
    }

    ALOGV("EGLTarget::frameWillRender(), buffers.current %p", buffers.current);
    for (auto& buffer : buffers.pool)
        ALOGV("  buffer: id %u, locked %d object %p", buffer.bufferID, buffer.locked, buffer.object);

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
    } else
        ALOGV("  found current buffer %p at array[%zu]", buffers.current, std::distance(buffers.pool.begin(), buffers.current));

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

        ALOGV("  spawned EGL state: clientBuffer %p, EGLImageKHR %p, egl err %x", clientBuffer, current.egl.image, eglGetError());

        std::array<GLuint, 2> renderbuffers { 0, 0 };
        glGenRenderbuffers(2, renderbuffers.data());
        current.gl.colorBuffer = renderbuffers[0];
        current.gl.dsBuffer = renderbuffers[1];

        glBindRenderbuffer(GL_RENDERBUFFER, current.gl.colorBuffer);
        renderer.imageTargetRenderbufferStorageOES(GL_RENDERBUFFER, current.egl.image);

        glBindRenderbuffer(GL_RENDERBUFFER, current.gl.dsBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, renderer.width, renderer.height);

        ALOGV("  spawned GL state: colorBuffer %u dsBuffer %u, gl err %x", current.gl.colorBuffer, current.gl.dsBuffer, glGetError());

        {
            IPC::BufferAllocation allocation;
            allocation.poolID = buffers.poolID;
            allocation.bufferID = current.bufferID;

            IPC::Message message;
            IPC::BufferAllocation::construct(message, allocation);
            ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);

            while (true) {
                int ret = AHardwareBuffer_sendHandleToUnixSocket(current.object, ipcClient.socketFd());
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
    glFlush();

    ALOGV("EGLTarget::frameRendered(), buffers.current %p, errors: %x/%x",
        buffers.current, eglGetError(), glGetError());
    if (buffers.current->object) {
        ALOGV("  committing object %p", buffers.current->object);

        IPC::BufferCommit commit;
        commit.poolID = buffers.poolID;
        commit.bufferID = buffers.current->bufferID;

        IPC::Message message;
        IPC::BufferCommit::construct(message, commit);
        ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }

    buffers.current->locked = true;
    buffers.current = nullptr;
}

void EGLTarget::deinitialize()
{
    destroyBufferPool(buffers.pool, renderer.destroyImageKHR);

    if (renderer.framebuffer)
        glDeleteFramebuffers(1, &renderer.framebuffer);
    renderer.framebuffer = 0;
}

void EGLTarget::releaseBuffer(uint32_t poolID, uint32_t bufferID)
{
    ALOGV("EGLTarget::releaseBuffer() poolID %u, bufferID %u", poolID, bufferID);

    if (buffers.poolID != poolID)
        return;

    for (auto& buffer : buffers.pool) {
        ALOGV("  buffers.pool[%zu]: id %u, locked %d, object %p",
            std::distance(buffers.pool.begin(), &buffer),
            buffer.bufferID, buffer.locked, buffer.object);
    }
    for (auto& buffer : buffers.pool) {
        if (buffer.bufferID == bufferID) {
            buffer.locked = false;
            break;
        }
    }
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::FrameComplete::code:
    {
        wpe_renderer_backend_egl_target_dispatch_frame_complete(target);
        break;
    }
    case IPC::ReleaseBuffer::code:
    {
        auto release = IPC::ReleaseBuffer::from(message);
        ALOGV("EGLTarget::handleMessage(): BufferRelease { poolID %u, bufferID %u }", release.poolID, release.bufferID);
        releaseBuffer(release.poolID, release.bufferID);
        break;
    }
    default:
        ALOGV("EGLTarget: invalid message");
        break;
    }
}

struct wpe_renderer_backend_egl_interface android_renderer_backend_egl_impl = {
    // create
    [] (int) -> void*
    {
        ALOGV("noop_renderer_backend_egl_impl::create()");
        return nullptr;
    },
    // destroy
    [] (void*)
    {
        ALOGV("noop_renderer_backend_egl_impl::destroy()");
    },
    // get_native_display
    [] (void*) -> EGLNativeDisplayType
    {
        ALOGV("noop_renderer_backend_egl_impl::get_native_display()");
        return EGL_DEFAULT_DISPLAY;
    },
    // get_platform
    [] (void*) -> uint32_t
    {
        ALOGV("noop_renderer_backend_egl_impl::get_platform()");
        return EGL_PLATFORM_SURFACELESS_MESA;
    },
};

struct wpe_renderer_backend_egl_target_interface android_renderer_backend_egl_target_impl = {
    // create
    [] (struct wpe_renderer_backend_egl_target* target, int hostFd) -> void*
    {
        ALOGV("noop_renderer_backend_egl_target_impl::create() fd %d", hostFd);
        return new EGLTarget(target, hostFd);
    },
    // destroy
    [] (void* data)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::destroy()");
        auto* target = static_cast<EGLTarget*>(data);
        delete target;
    },
    // initialize
    [] (void* data, void*, uint32_t width, uint32_t height)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::initialize() (%u,%u)", width, height);
        static_cast<EGLTarget*>(data)->initialize(width, height);
    },
    // get_native_window
    [] (void*) -> EGLNativeWindowType
    {
        ALOGV("noop_renderer_backend_egl_target_impl::get_native_window()");
        return nullptr;
    },
    // resize
    [] (void* data, uint32_t width, uint32_t height)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::resize() (%u,%u)", width, height);
        static_cast<EGLTarget*>(data)->resize(width, height);
    },
    // frame_will_render
    [] (void* data)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::frame_will_render()");
        static_cast<EGLTarget*>(data)->frameWillRender();
    },
    // frame_rendered
    [] (void* data)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::frame_rendered()");
        static_cast<EGLTarget*>(data)->frameRendered();
    },
    // deinitialize
    [] (void* data)
    {
        ALOGV("noop_renderer_backend_egl_target_impl::deinitialize()");
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
