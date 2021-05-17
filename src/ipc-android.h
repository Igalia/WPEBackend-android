#pragma once

namespace IPC {

struct PoolConstruction {
    uint32_t poolID;
    uint8_t padding[20];

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

struct BufferAllocation {
    uint32_t poolID;
    uint32_t bufferID;
    uint8_t padding[16];

    static const uint64_t code = 8;
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
    uint8_t padding[24];

    static const uint64_t code = 23;
    static void construct(Message& message)
    {
        message.messageCode = code;
    }
};
static_assert(sizeof(FrameComplete) == Message::dataSize, "FrameComplete is of correct size");

}
