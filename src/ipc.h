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

#include <functional>
#include <gio/gio.h>
#include <memory>
#include <stdint.h>
#include <unistd.h>

#define NO_ERROR 0L

namespace IPC {

struct Message {
    static const size_t size = 32;
    static const size_t dataSize = 24;

    uint64_t messageCode { 0 };
    uint8_t messageData[dataSize] { 0, };

    static char* data(Message& message) { return reinterpret_cast<char*>(std::addressof(message)); }
    static Message& cast(char* data) { return *reinterpret_cast<Message*>(data); }
};
static_assert(sizeof(Message) == Message::size, "Message is of correct size");

class Host {
public:
    class Handler {
    public:
        virtual void handleMessage(char*, size_t) = 0;
    };

    Host();

    void initialize(Handler&);
    void deinitialize();

    int socketFd();
    int releaseClientFD(bool closeSourceFd = false);

    void sendMessage(char*, size_t);
    int receiveFileDescriptor();

private:
    static gboolean socketCallback(GSocket*, GIOCondition, gpointer);

    Handler* m_handler { nullptr };

    GSocket* m_socket { nullptr };
    GSource* m_source { nullptr };
    int m_clientFd { -1 };
};

class Client {
public:
    class Handler {
    public:
        virtual void handleMessage(char*, size_t) = 0;
    };

    Client();

    void initialize(Handler&, int);
    void deinitialize();

    int socketFd();

    void sendMessage(char*, size_t);
    void sendAndReceiveMessage(char*, size_t, std::function<void(char*, size_t)> handler);
    int sendFileDescriptor(int fd);

private:
    static gboolean socketCallback(GSocket*, GIOCondition, gpointer);

    Handler* m_handler { nullptr };

    GSocket* m_socket { nullptr };
    GSource* m_source { nullptr };
};

} // namespace IPC
