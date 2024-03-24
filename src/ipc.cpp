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

#include "ipc.h"

#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logging.h"

namespace IPC {

Host::Host() = default;

void Host::initialize(Handler& handler)
{
    m_handler = &handler;

    int sockets[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if (ret == -1)
        return;

    m_socket = g_socket_new_from_fd(sockets[0], nullptr);
    if (!m_socket) {
        close(sockets[0]);
        close(sockets[1]);
        return;
    }

    m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
    g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(socketCallback), this, nullptr);
    g_source_attach(m_source, g_main_context_get_thread_default());

    m_clientFd = sockets[1];
}

void Host::deinitialize()
{
    if (m_clientFd != -1)
        close(m_clientFd);

    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }

    if (m_socket)
        g_clear_object(&m_socket);

    m_handler = nullptr;
}

int Host::socketFd()
{
    if (m_socket)
        return g_socket_get_fd(m_socket);
    return -1;
}

int Host::releaseClientFD(bool closeSourceFd)
{
    int fd = dup(m_clientFd);
    if (closeSourceFd && m_clientFd != -1) {
        close(m_clientFd);
        m_clientFd = -1;
    }
    return fd;
}

void Host::sendMessage(char* data, size_t size)
{
    g_socket_send(m_socket, data, size, nullptr, nullptr);
}

int Host::receiveFileDescriptor()
{
    struct msghdr msg = {0};

    char m_buffer[1];
    struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);

    int result;
    do {
        result = recvmsg(socketFd(), &msg, 0);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        result = errno;
        ALOGV("Error reading ile descriptor from socket: error %#x (%s)",
                result, strerror(result));
        return -result;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    int fd;
    memmove(&fd, CMSG_DATA(cmsg), sizeof(fd));

    return fd;
}

gboolean Host::socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    auto& host = *static_cast<Host*>(data);

    char* buffer = g_new0(char, Message::size);
    GInputVector vector = { buffer, Message::size };
    gssize len = g_socket_receive_message(socket, nullptr, &vector, 1,
        nullptr, nullptr, nullptr, nullptr, nullptr);

    // If nothing is read, give up.
    if (len == -1) {
        g_free(buffer);
        return FALSE;
    }

    if (len == Message::size)
        host.m_handler->handleMessage(buffer, Message::size);

    g_free(buffer);
    return TRUE;
}

Client::Client() = default;

void Client::initialize(Handler& handler, int fd)
{
    m_handler = &handler;

    m_socket = g_socket_new_from_fd(fd, nullptr);
    if (!m_socket)
        return;

    m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
    g_source_set_name(m_source, "WPEBackend-android::socket");
    g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(socketCallback), this, nullptr);
    g_source_attach(m_source, g_main_context_get_thread_default());
}

void Client::deinitialize()
{
    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }

    g_clear_object(&m_socket);

    m_handler = nullptr;
}

int Client::socketFd()
{
    if (m_socket)
        return g_socket_get_fd(m_socket);
    return -1;
}

gboolean Client::socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    GError* error = nullptr;
    char* buffer = g_new0(char, Message::size);
    gssize len = g_socket_receive(socket, buffer, Message::size, nullptr, &error);
    if (len == -1) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED))
            g_warning("Failed to read message from socket: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    auto client = reinterpret_cast<Client*>(data);
    if (len == Message::size && client->m_handler)
        client->m_handler->handleMessage(buffer, Message::size);

    g_free(buffer);
    return TRUE;
}

void Client::sendMessage(char* data, size_t size)
{
    g_socket_send(m_socket, data, size, nullptr, nullptr);
}

void Client::sendAndReceiveMessage(char* data, size_t size, std::function<void(char*, size_t)> handler)
{
    g_socket_send(m_socket, data, size, nullptr, nullptr);

    char* buffer = g_new0(char, Message::size);
    gssize len = g_socket_receive_with_blocking(m_socket, buffer, Message::size, TRUE, nullptr, nullptr);

    if (len == Message::size)
        handler(buffer, Message::size);

    g_free(buffer);
}

int Client::sendFileDescriptor(int fd)
{
    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, '\0', sizeof(buf));

    struct iovec io = { .iov_base = (char*)"", .iov_len = 1 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    memmove(CMSG_DATA(cmsg), &fd, sizeof(fd));

    msg.msg_controllen = CMSG_SPACE(sizeof(fd));

    int result;
    do {
        result = sendmsg(socketFd(), &msg, 0);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        result = errno;
        ALOGV("Error writing file descriptor to socket: error %#x (%s)",
                result, strerror(result));
        return -result;
    }
    return NO_ERROR;
}

} // namespace IPC
