#ifndef __BOMBCLIENT_H
#define __BOMBCLIENT_H

#include "Arduino.h"
#include <stdint.h>
#include "Wire.h"
#include <new>
#include "Common.h"
#include "AsyncI2CLib.h"

#define function []

class BombClient {
public:
    typedef void(*EventDispatcher)(uint8_t eventId, void* eventData, void* param);
    typedef void(*HandshakeHandler)(void** hsData, size_t* hsDataSize, void* param);

    struct TResponse {

    };

    template<class Resp>
    struct TRequest {

    };
private:
    struct EventDispatcherHandle {
        EventDispatcher m_Func;
        void*           m_Param;
        EventDispatcherHandle* m_Next;
    };

    struct NetPacketProlog {
        static constexpr char START_MAGIC = 0xFE;

        char        StartMagic;
        uint16_t    ContentSize;
    };

    struct WriteContext {
        BombClient* Cl;
        NetPacketProlog Prolog;
        void* Data;
        bool FreeData;
    };

    struct ServerRequest {
        IDHASH      HandlerID;
        uint16_t    ParamsSize;
        char*       Params;
        void(*      ResponseHandler)(void* resp, void* param);
        void*       ResponseHandlerParam;
    };

    struct HandshakeResponse {
        static constexpr const char* CHECK_CODE = "Julka";

        char    CheckCode[6];
        char    ModuleInfo[];
    };

    enum NetCommand : uint8_t {
        INVALID,
        POLL,
        RESPONSE,
        EVENT,
        HANDSHAKE,

        NET_COMMAND_MAX,
    };

    struct NetCommandPacket {
        NetCommand  CommandID;
        char        Params[];

        template<typename T>
        inline T* GetData() {
            return reinterpret_cast<T*>(Params);
        }

        inline void Close() {
            free(this);
        }
    };

    static constexpr size_t REQUEST_POOL_LIMIT = 8;
    static constexpr size_t REQUEST_POOL_FULL = -1;

    comm::AsyncI2C   m_I2C;

    ServerRequest    m_RequestPool[REQUEST_POOL_LIMIT];
    uint32_t         m_RequestQueueAlloc;

    bool             m_DiscoveryRequested;

    NetCommandPacket*m_CurrentCommand;

    void(*m_CommandHandlers[NET_COMMAND_MAX])(BombClient* client);

    EventDispatcherHandle* m_EventDispHead;
    EventDispatcherHandle* m_EventDispTail;

    HandshakeHandler m_HandshakeHandler;
    void*            m_HandshakeHandlerParam;

    public:
        BombClient();

        Promise* ReadPacket();
        Promise* WritePacket(void* data, size_t size, bool freeData = false);

        void Attach(int address);

        template<typename T, typename F>
        void AddEventDispatcher(F disp, T* param) {
            void(*func)(uint8_t, void*, T*) = static_cast<void(*)(uint8_t, void*, T*)>(disp);
            EventDispatcherHandle* h = new EventDispatcherHandle {(EventDispatcher)func, param, nullptr};
            if (m_EventDispHead) {
                m_EventDispTail->m_Next = h;
                m_EventDispTail = h;
            }
            else {
                m_EventDispHead = h;
                m_EventDispTail = h;
            }
        }

        template<typename T, typename F>
        void SetHandshakeHandler(F disp, T* param) {
            void(*func)(void**, size_t*, T*) = static_cast<void(*)(void**, size_t*, T*)>(disp);
            m_HandshakeHandler = (HandshakeHandler) func;
            m_HandshakeHandlerParam = static_cast<void*>(param);
        }

        void DispatchEvent();

        void RespondToHandshake();

        inline bool IsAllSyncDone() {
            return m_RequestQueueAlloc == 0;
        }

        inline uint32_t BitMask(int i) {
            return (1ull << (uint32_t)i);
        }

        void HandleResponse();

        void EmptyResponse();

        void FlushRequests();

        size_t GetAvailableRequestId();

        template <typename Resp, template <typename> typename Req, typename RespHnd, typename P>
        void QueueRequest(const char* handlerName, Req<Resp>* params, RespHnd handleResponse, P* handleRespParam = nullptr) {
            size_t allocIndex = GetAvailableRequestId();
            if (allocIndex != REQUEST_POOL_FULL) {
                void(*func)(Resp*, P*) = static_cast<void(*)(Resp*, P*)>(handleResponse);
                InsertRequest(allocIndex, handlerName, static_cast<void*>(params), sizeof(Req<Resp>), reinterpret_cast<void(*)(void*, void*)>(func), static_cast<void*>(handleRespParam));
            }
        }

        template <typename Resp, typename Req, typename RespHnd>
        void QueueRequest(const char* handlerName, Req* params, RespHnd handleResponse) {
            size_t allocIndex = GetAvailableRequestId();
            if (allocIndex != REQUEST_POOL_FULL) {
                void(*func)(Resp*) = static_cast<void(*)(Resp*)>(handleResponse);
                InsertRequest(allocIndex, handlerName, static_cast<void*>(params), sizeof(Req), reinterpret_cast<void(*)(void*, void*)>(func), nullptr);
            }
        }

        template<typename Req>
        void QueueRequest(const char* handlerName, Req* params) {
            size_t allocIndex = GetAvailableRequestId();
            if (allocIndex != REQUEST_POOL_FULL) {
                InsertRequest(allocIndex, handlerName, static_cast<void*>(params), sizeof(Req), nullptr, nullptr);
            }
        }

        void QueueRequest(const char* handlerName);
        
        void DiscardRequests();
    
    private:
        void InsertRequest(size_t id, const char* handlerName, void* params, size_t paramSize, void(*responseHandler)(void*, void*), void* handleRespParam);
};

#endif