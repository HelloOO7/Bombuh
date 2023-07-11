#include "Arduino.h"
#include <stdint.h>
#include "Wire.h"
#include <new>
#include "BombClient.h"
#include "Promise.h"
#include "AsyncI2CLib.h"
#include <stdlib.h>
#include "UARTPrint.h"
#include "DebugPrint.h"

BombClient::BombClient() :
    m_I2C(),
    m_RequestQueueAlloc{0},
    m_DiscoveryRequested{false},
    m_CurrentCommand{nullptr},
    m_EventDispHead{nullptr},
    m_EventDispTail{nullptr},
    m_HandshakeHandler{nullptr}
{
    memset(m_CommandHandlers, 0, sizeof(m_CommandHandlers));
    m_CommandHandlers[NetCommand::INVALID] = nullptr;
    m_CommandHandlers[NetCommand::POLL] = function(BombClient* client) {
        client->FlushRequests();
    };
    m_CommandHandlers[NetCommand::EVENT] = function(BombClient* client) {
        client->DispatchEvent();
    };
    m_CommandHandlers[NetCommand::HANDSHAKE] = function(BombClient* client) {
        client->RespondToHandshake();
    };
    m_CommandHandlers[NetCommand::RESPONSE] = function(BombClient* client) {
        client->HandleResponse();
    };
}

Promise* BombClient::ReadPacket() {
    return m_I2C.ReadTemp(this, sizeof(NetPacketProlog))->Then<BombClient, NetPacketProlog>(function(BombClient* cl, NetPacketProlog* response) -> Promise* {
        if (response->StartMagic == NetPacketProlog::START_MAGIC) {
            DEBUG_PRINTF_P("Read packet of size %d.\n", response->ContentSize);
            if (response->ContentSize) {
                return cl->m_I2C.Read(cl, response->ContentSize);
            }
            else {
                return new EmptyPromise(cl);
            }
        }
        return nullptr;
    });
}

Promise* BombClient::WritePacket(void* data, size_t size, bool freeData) {
    WriteContext* ctx = new WriteContext {this, {NetPacketProlog::START_MAGIC, size}, data, freeData};
    return m_I2C.Write(ctx, &ctx->Prolog, sizeof(NetPacketProlog))->Then<WriteContext>(function(WriteContext* ctx) -> Promise* {
        Promise* p;
        if (ctx->Prolog.ContentSize) {
            p = ctx->Cl->m_I2C.Write(ctx, ctx->Data, ctx->Prolog.ContentSize);
        } else {
            p = new EmptyPromise(ctx);
        }
        p->Then<WriteContext>(function(WriteContext* ctx) {
            if (ctx->FreeData) {
                free(ctx->Data);
            }
            delete ctx;
        });
        return p;
    });
}

void BombClient::Attach(int address) {
    Wire.setClock(28800);
    
    static BombClient* _wireclient = this;

    Wire.onRequest(function() {
        DEBUG_PRINTLN("Bytes requested!")
        BombClient* b = _wireclient;
        if (b->m_DiscoveryRequested) {
            b->m_DiscoveryRequested = false;
            Wire.write(0xAE);
            return;
        }
        b->m_I2C.HandleRequest();
        DEBUG_PRINTLN("OnRequest finished.")
    });

    Wire.onReceive(function(int n) {
        size_t bytes = (size_t) Wire.available();

        if (!bytes) {
            return;
        }

        DEBUG_PRINTF_P("Receiving %d bytes...\n", n)

        BombClient* b = _wireclient;
        bytes -= b->m_I2C.HandleReceive(bytes);

        if (bytes) {
            if (Wire.peek() == 0xEA) {
                b->m_DiscoveryRequested = true;
                Wire.read();
                return;
            }
            else {
                b->ReadPacket()->Then<BombClient, void>(function(BombClient* cl, void* packetContents) {
                    cl->m_CurrentCommand = static_cast<NetCommandPacket*>(packetContents);
                    DEBUG_PRINTF_P("Packet type %d\n", cl->m_CurrentCommand->CommandID);

                    if (cl->m_CurrentCommand->CommandID < NetCommand::NET_COMMAND_MAX && cl->m_CommandHandlers[cl->m_CurrentCommand->CommandID]) {
                        cl->m_CommandHandlers[cl->m_CurrentCommand->CommandID](cl);
                    }
                    cl->m_CurrentCommand->Close();
                });
                while (bytes) {
                    if (!b->m_I2C.IsReceiving()) {
                        break;
                    }
                    bytes -= b->m_I2C.HandleReceive(bytes);
                }
            }
        }
        DEBUG_PRINTLN("OnReceive finished.");
    });

    Wire.begin(address);
}

void BombClient::DispatchEvent() {
    EventDispatcherHandle* evd = m_EventDispHead;
    uint8_t eventId = m_CurrentCommand->Params[0];
    void* eventData = &m_CurrentCommand->Params[1];
    while (evd) {
        evd->m_Func(eventId, eventData, evd->m_Param);
        evd = evd->m_Next;
    }
    EmptyResponse();
}

void BombClient::RespondToHandshake() {
    void* data = nullptr;
    size_t dataSize = 0;
    if (m_HandshakeHandler) {
        m_HandshakeHandler(&data, &dataSize, m_HandshakeHandlerParam);
    }
    size_t packetSize = dataSize + sizeof(HandshakeResponse);
    void* packet = malloc(packetSize);
    HandshakeResponse* r = reinterpret_cast<HandshakeResponse*>(packet);
    r->CheckCode = HandshakeResponse::CHECK_CODE;
    memcpy(&r->ModuleInfo, data, dataSize);
    free(data);
    WritePacket(r, packetSize, true);
}

void BombClient::HandleResponse() {
    uint8_t respId = m_CurrentCommand->Params[0];
    void* respData = &m_CurrentCommand->Params[1];
    if (respId < REQUEST_POOL_LIMIT) {
        if (m_RequestPool[respId].ResponseHandler) {
            m_RequestPool[respId].ResponseHandler(respData, m_RequestPool[respId].ResponseHandlerParam);
        }
    }
    else {
        PRINTF_P("Response ID out of range: %d\n", respId);
    }
    m_RequestQueueAlloc &= ~BitMask(respId);
}

void BombClient::EmptyResponse() {
    WritePacket(nullptr, 0);
}

void BombClient::FlushRequests() {
    size_t packetSize = 1; //count
    uint32_t mask = m_RequestQueueAlloc;
    size_t entryCount = 0;
    for (size_t i = 0; i < REQUEST_POOL_LIMIT; i++) {
        if (mask & BitMask(i)) {
            entryCount++;
            packetSize += 7 + m_RequestPool[i].ParamsSize;
        }
    }
    char* pbuf = new char[packetSize];
    pbuf[0] = entryCount;
    char* pstream = pbuf + 1;
    
    for (size_t i = 0; i < REQUEST_POOL_LIMIT; i++) {
        if (mask & BitMask(i)) {
            ServerRequest* r = &m_RequestPool[i];
            *(pstream++) = i;
            memcpy(pstream, &r->HandlerID, sizeof(r->HandlerID));
            pstream += sizeof(r->HandlerID);
            memcpy(pstream, &r->ParamsSize, sizeof(r->ParamsSize));
            pstream += sizeof(r->ParamsSize);
            memcpy(pstream, r->Params, r->ParamsSize);
            pstream += r->ParamsSize;
            delete r->Params;
        }
    }
    
    WritePacket(pbuf, packetSize, true);
}

size_t BombClient::GetAvailableRequestId() {
    size_t allocIndex = REQUEST_POOL_FULL;
    for (size_t i = 0; i < REQUEST_POOL_LIMIT; i++) {
        if ((m_RequestQueueAlloc & BitMask(i)) == 0) {
            allocIndex = i;
            break;
        }
    }
    return allocIndex;
}

void BombClient::QueueRequest(const char* handlerName)  {
    size_t allocIndex = GetAvailableRequestId();
    if (allocIndex != REQUEST_POOL_FULL) {
        InsertRequest(allocIndex, handlerName, nullptr, 0, nullptr, nullptr);
    }
    else {
        PRINTF_P("Could not insert request for %s - queue full!\n", handlerName);
    }
}

void BombClient::DiscardRequests() {
    m_RequestQueueAlloc = 0;
}

void BombClient::InsertRequest(size_t id, const char* handlerName, void* params, size_t paramSize, void(*responseHandler)(void*, void*), void* handleRespParam) {
    ServerRequest* req = &m_RequestPool[id];
    req->HandlerID = HashID(handlerName);
    req->ParamsSize = paramSize;
    req->Params = new char[paramSize];
    req->ResponseHandler = responseHandler;
    req->ResponseHandlerParam = handleRespParam;
    memcpy(req->Params, params, paramSize);
    m_RequestQueueAlloc |= BitMask(id);
}