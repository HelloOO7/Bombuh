#ifndef I2C_DEBUG
#undef DEBUG
#endif

#include <stddef.h>
#include "lambda.h"
#include <alloca.h>
#include "DebugPrint.h"
#include "Promise.h"
#include "AsyncI2CLib.h"
#include "Wire.h"

namespace comm {
    
    I2CReadPromiseQueue::I2CReadPromiseQueue() {
        m_Head = nullptr;
    }

    bool I2CReadPromiseQueue::IsEmpty() {
        return m_Head == nullptr;
    }

    void I2CReadPromiseQueue::Insert(Promise* promise, size_t readSize, ReadLocation readType, void* readPointer) {
        Entry* entry = new Entry();
        entry->m_Promise = promise;
        entry->m_Next = m_Head;
        entry->m_RemainingSize = readSize;
        entry->m_ReadLoc = readType;
        entry->m_ReadBuffer = (char*)readPointer;
        entry->m_ReadBufferPos = entry->m_ReadBuffer;
        m_Head = entry;
        DEBUG_PRINTF_P("Promising %d bytes to promise %p.\n", readSize, promise)
    }

    size_t I2CReadPromiseQueue::ReadInto(size_t limit) {
        DEBUG_PRINTF_P("Distributing %d bytes to promises.\n", limit)
        size_t read = 0;

        START:
        if (!limit) {
            return read;
        }
        Entry* e = m_Head;
        if (e) {
            if (e->m_ReadBuffer == nullptr && e->m_RemainingSize) {
                if (e->m_ReadLoc == ReadLocation::STACK && e->m_RemainingSize > limit) {
                    e->m_ReadLoc = ReadLocation::TEMP_HEAP;
                }

                switch (e->m_ReadLoc) {
                    case ReadLocation::HEAP:
                    case ReadLocation::TEMP_HEAP:
                        e->m_ReadBuffer = (char*)malloc(e->m_RemainingSize);
                        break;
                    case ReadLocation::STACK:
                        e->m_ReadBuffer = (char*)alloca(e->m_RemainingSize);
                        break;
                    default:
                        break;
                }
                e->m_ReadBufferPos = e->m_ReadBuffer;
            }
            size_t myLimit = limit;
            if (myLimit > e->m_RemainingSize) {
                myLimit = e->m_RemainingSize;
            }
            size_t hwread = Wire.readBytes(e->m_ReadBufferPos, myLimit);
            DEBUG_PRINTF_P("In promise: read %d bytes.\n", hwread)
            e->m_ReadBufferPos += hwread;
            e->m_RemainingSize -= hwread;
            limit -= hwread;

            if (!e->m_RemainingSize) {
                m_Head = e->m_Next;
                DEBUG_PRINTF_P("Resolving read promise %p...\n", e->m_Promise)
                e->m_Promise->Resolve(e->m_ReadBuffer);
                DEBUG_PRINTLN("Promise resolved!")

                if (e->m_ReadLoc == ReadLocation::TEMP_HEAP) {
                    delete[] e->m_ReadBuffer;
                }

                delete e;
            }

            read += hwread;
            goto START; //prevent recursion
        }
        #ifdef DEBUG
        Serial.flush();
        #endif
        return read;
    }

    I2CWritePromiseQueue::I2CWritePromiseQueue() {
        m_Head = nullptr;
    }

    void I2CWritePromiseQueue::Insert(Promise* promise, void* data, size_t size) {
        Entry* entry = new Entry();
        entry->m_Promise = promise;
        entry->m_Next = m_Head;
        entry->m_RemainingSize = size;
        entry->m_WriteBuffer = (char*)data;
        entry->m_WriteBufferPos = entry->m_WriteBuffer;
        m_Head = entry;
        DEBUG_PRINTF_P("Promising to write %d bytes to %p.\n", size, promise)            
    }

    size_t I2CWritePromiseQueue::WriteOut() { //only one response at a time
        if (m_Head) {
            Entry* e = m_Head;
            DEBUG_PRINTF_P("Writing %d of promised packet data.\n", e->m_RemainingSize)
            #ifdef DEBUG
            char* startBufPos = e->m_WriteBufferPos;
            #endif
            size_t wreq = e->m_RemainingSize;
            if (wreq > 32) {
                wreq = 32;
            }
            size_t written = Wire.write(e->m_WriteBufferPos, wreq);
            e->m_WriteBufferPos += written;
            e->m_RemainingSize -= written;
            DEBUG_PRINTF_P("Wrote %d\n", written);
            #ifdef DEBUG
            Serial.print("{");
            for (size_t i = 0; i < written; i++) {
                if (i) {
                    Serial.print(", ");
                }
                Serial.print((uint8_t)startBufPos[i], HEX);
            }
            Serial.println("}");
            #endif
            if (e->m_RemainingSize == 0) {
                m_Head = e->m_Next;
                DEBUG_PRINTF_P("Resolving write promise %p...\n", e->m_Promise)
                e->m_Promise->Resolve(e->m_WriteBufferPos);
                DEBUG_PRINTLN("Resolved!");
                delete e;
            }
            return written;
        }
        return 0;
    }

    AsyncI2C::AsyncI2C() : m_ReadQueue(), m_WriteQueue() {

    }

    bool AsyncI2C::IsReceiving() {
        return !m_ReadQueue.IsEmpty();
    }

    size_t AsyncI2C::HandleReceive(size_t size) {
        size_t s = m_ReadQueue.ReadInto(size);
        DEBUG_PRINTF_P("Consumed %d bytes in read promises.\n", s)
        return s;
    }

    void AsyncI2C::HandleRequest() {
        m_WriteQueue.WriteOut();
    }
    
    Promise* AsyncI2C::Read(void* context, size_t size, ReadLocation bufferLocation) {
        Promise* promise = new Promise(context);
        m_ReadQueue.Insert(promise, size, bufferLocation);
        return promise;
    }

    Promise* AsyncI2C::ReadTemp(void* context, size_t size) {
        Promise* promise = new Promise(context);
        m_ReadQueue.Insert(promise, size, ReadLocation::STACK);
        return promise;
    }

    Promise* AsyncI2C::ReadInto(void* context, size_t size, void* dest) {
        Promise* promise = new Promise(context);
        m_ReadQueue.Insert(promise, size, ReadLocation::DEFINED, dest);
        return promise;
    }

    Promise* AsyncI2C::Write(void* context, void* data, size_t size) {
        Promise* promise = new Promise(context);
        m_WriteQueue.Insert(promise, data, size);
        return promise;
    }
}