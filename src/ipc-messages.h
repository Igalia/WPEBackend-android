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

namespace IPC {

struct PoolConstruction {
    uint8_t padding[24];

    static const uint64_t code = 4;
    static void construct(Message& message, const PoolConstruction& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static PoolConstruction from(const Message& message)
    {
        PoolConstruction data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(PoolConstruction) == Message::dataSize, "PoolConstruction is of correct size");

struct PoolConstructionReply {
    uint32_t poolID;
    uint8_t padding[20];

    static const uint64_t code = 5;
    static void construct(Message& message, const PoolConstructionReply& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static PoolConstructionReply from(const Message& message)
    {
        PoolConstructionReply data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(PoolConstructionReply) == Message::dataSize, "PoolConstructionReply is of correct size");

struct PoolPurge {
    uint32_t poolID;
    uint8_t padding[20];

    static const uint64_t code = 6;
    static void construct(Message& message, const PoolPurge& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static PoolPurge from(const Message& message)
    {
        PoolPurge data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(PoolPurge) == Message::dataSize, "PoolPurge is of correct size");

struct RegisterPool {
    uint32_t poolID;
    uint8_t padding[20];

    static const uint64_t code = 7;
    static void construct(Message& message, const RegisterPool& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static RegisterPool from(const Message& message)
    {
        RegisterPool data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(RegisterPool) == Message::dataSize, "RegisterPool is of correct size");

struct UnregisterPool {
    uint32_t poolID;
    uint8_t padding[20];

    static const uint64_t code = 8;
    static void construct(Message& message, const UnregisterPool& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static UnregisterPool from(const Message& message)
    {
        UnregisterPool data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(UnregisterPool) == Message::dataSize, "UnregisterPool is of correct size");

struct BufferAllocation {
    uint32_t poolID;
    uint32_t bufferID;
    uint8_t padding[16];

    static const uint64_t code = 10;
    static void construct(Message& message, const BufferAllocation& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static BufferAllocation from(const Message& message)
    {
        BufferAllocation data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(BufferAllocation) == Message::dataSize, "BufferAllocation is of correct size");

struct BufferCommit {
    uint32_t poolID;
    uint32_t bufferID;
    uint8_t padding[16];

    static const uint64_t code = 15;
    static void construct(Message& message, const BufferCommit& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static BufferCommit from(const Message& message)
    {
        BufferCommit data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(BufferCommit) == Message::dataSize, "BufferCommit is of correct size");

struct ReleaseBuffer {
    uint32_t poolID;
    uint32_t bufferID;
    uint8_t padding[16];

    static const uint64_t code = 16;
    static void construct(Message& message, const ReleaseBuffer& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static ReleaseBuffer from(const Message& message)
    {
        ReleaseBuffer data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(ReleaseBuffer) == Message::dataSize, "ReleaseBuffer is of correct size");

struct FrameComplete {
    uint32_t poolID;
    uint8_t padding[20];

    static const uint64_t code = 23;
    static void construct(Message& message, const FrameComplete& data)
    {
        message.messageCode = code;
        std::memcpy(&message.messageData, &data, Message::dataSize);
    }

    static FrameComplete from(const Message& message)
    {
        FrameComplete data;
        std::memcpy(&data, &message.messageData, Message::dataSize);
        return data;
    }
};
static_assert(sizeof(FrameComplete) == Message::dataSize, "FrameComplete is of correct size");

}
