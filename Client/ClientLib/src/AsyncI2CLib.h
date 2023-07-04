#ifndef __ASYNCI2CLIB_H
#define __ASYNCI2CLIB_H

#include <stddef.h>
#include <new>
#include "lambda.h"
#include <alloca.h>
#include "DebugPrint.h"
#include "Promise.h"

namespace comm {

    enum class ReadLocation {
        STACK,
        TEMP_HEAP,
        HEAP,
        DEFINED
    };

    class I2CReadPromiseQueue {
    private:
        struct Entry {
            Promise* m_Promise;
            size_t m_RemainingSize;
            char* m_ReadBuffer;
            char* m_ReadBufferPos;
            ReadLocation m_ReadLoc;

            Entry* m_Next;
        };

        Entry* m_Head;
    
    public:
        I2CReadPromiseQueue();

        bool IsEmpty();

        void Insert(Promise* promise, size_t readSize, ReadLocation readType, void* readPointer = nullptr);

        size_t ReadInto(size_t limit);
    };

    class I2CWritePromiseQueue {
    private:
        struct Entry {
            Promise* m_Promise;
            char* m_WriteBuffer;
            char* m_WriteBufferPos;
            size_t m_RemainingSize;

            Entry* m_Next;
        };

        Entry* m_Head;
    
    public:
        I2CWritePromiseQueue();

        void Insert(Promise* promise, void* data, size_t size);

        size_t WriteOut();
    };

    struct PacketHeader {
        char Magic;
        uint16_t Size;
    };

    class AsyncI2C {
    private:
        I2CReadPromiseQueue m_ReadQueue;
        I2CWritePromiseQueue m_WriteQueue;

        struct ReadContext {
            AsyncI2C* Reader;
            void* Context;
        };
    public:
        AsyncI2C();

        bool IsReceiving();

        size_t HandleReceive(size_t size);

        void HandleRequest();
        
        Promise* Read(void* context, size_t size, ReadLocation bufferLocation = ReadLocation::HEAP);

        Promise* ReadTemp(void* context, size_t size);

        Promise* ReadInto(void* context, size_t size, void* dest);

        Promise* Write(void* context, void* data, size_t size);
    };
}

#endif