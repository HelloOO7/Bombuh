#ifndef __GAMEEVENT_H
#define __GAMEEVENT_H

#include "Arduino.h"
#include "util/atomic.h"
#include "lambda.h"

//#define EVENT_DEBUG

namespace game {
    template<typename C>
    struct EventChain;

    template<typename C>
    class Event;

    template<typename C>
    class EventManager;

    typedef bool(*EventFunc)(void*, void*, void*);

    #ifdef EVENT_DEBUG
    extern int g_EventInstCount;
    #endif

    template<typename C>
    class Event {
        friend class EventManager<C>;
        friend struct EventChain<C>;
    private:
        EventFunc m_Func;
        void* m_Data;
        bool m_FreeData;
        Event* m_Next;

        EventChain<C>* m_Chain;

        bool Update(void* container) {
            if (!m_Func) {
                return true;
            }
            return m_Func(this, container, m_Data);
        }

        void Init(EventFunc func) {
            m_Func = func;
            #ifdef EVENT_DEBUG
            g_EventInstCount++;
            PRINTF_P("Event %p created, instance count: %d\n", this, g_EventInstCount);
            #endif
        }
    
    public:
        template<typename F, typename D>
        Event(F func, D* data) : m_Data{static_cast<void*>(data)}, m_Next{nullptr} {
            bool(*_func)(Event*, C*, D*) = static_cast<bool(*)(Event*, C*, D*)>(func);
            m_FreeData = true;
            Init((EventFunc)_func);
        }

        template<typename F>
        Event(F func) : m_Data{nullptr}, m_Next{nullptr} {
            bool(*_func)(Event*, C*) = static_cast<bool(*)(Event*, C*)>(func);
            m_FreeData = false;
            Init((EventFunc)_func);
        }

        ~Event() {
            if (m_FreeData && m_Data) {
                free(m_Data);
                m_Data = nullptr;
            }
            #ifdef EVENT_DEBUG
            g_EventInstCount--;
            PRINTF_P("Event %p deleted, instance count: %d\n", this, g_EventInstCount);
            #endif
        }

        Event* SetDataPermanent() {
            m_FreeData = false;
            return this;
        }

        template<typename F, typename D>
        Event* Then(F func, D* data) {
            return Then(new Event(func, data));
        }

        template<typename F>
        Event* Then(F func) {
            return Then(new Event(func));
        }
    
        Event* Then(Event* nextEvent) {
            Event* oldnext = m_Next;
            m_Next = nextEvent;
            nextEvent->m_Chain = m_Chain;
            Event* end = nextEvent;
            while (end->m_Next) {
                end = end->m_Next;
                end->m_Chain = m_Chain;
            }
            if (oldnext) {
                end->m_Next = oldnext;
            }
            return nextEvent;
        }

        void Cancel() {
            m_Chain->Cancel();
        }
    };

    template<typename C>
    Event<C>* CreateWaitEvent(long ms) {
        unsigned long* data = new unsigned long[2];
        data[0] = 0;
        data[1] = ms;
        return new Event<C>(function(Event<C>* e, C* context, unsigned long* data) {
            unsigned long time = millis();
            if (!data[0]) {
                data[0] = time + data[1];
            }
            return time >= data[0];
        }, data);
    }

    template<typename C>
    struct EventChainHandle {
        friend class EventManager<C>;
        friend struct EventChain<C>;
    private:
        EventManager<C>* m_Mgr{nullptr};
        EventChain<C>* m_Chain{nullptr};

    public:
        EventChainHandle() {
            
        }

        void Reset() {
            SetChain(nullptr, nullptr);
        }

    private:
        void SetChain(EventManager<C>* mgr, EventChain<C>* chain) {
            m_Mgr = mgr;
            m_Chain = chain;
        }

    public:
        void Cancel() {
            if (m_Mgr) {
                m_Mgr->Cancel(this);
            }
        }
    };

    template<typename C>
    class EventManager {
    private:
        EventChain<C>* m_Events;
        C*  m_Container;
        bool m_IsInEvent;
    
    public:
        EventManager(C* container) : m_Events{nullptr}, m_Container{container}, m_IsInEvent{false} {
            
        }

        Event<C>* Start(Event<C>* event, EventChainHandle<C>* handle = nullptr) {
            if (event) {
                EventChain<C>* chain = new EventChain<C>(event);
                chain->m_Next = m_Events;
                chain->m_Handle = handle;
                
                event->m_Chain = chain;
                while (event->m_Next) {
                    event = event->m_Next;
                    event->m_Chain = chain;
                }

                if (m_Events) {
                    m_Events->m_Prev = chain;
                }
                m_Events = chain;
                if (handle) {
                    handle->SetChain(this, chain);
                    chain->m_Handle = handle;
                }
            }
            return event;
        }
    private:
        void UnlinkChain(EventChain<C>* chn) {
            if (chn->m_Prev) {
                chn->m_Prev->m_Next = chn->m_Next;
            }
            else {
                m_Events = chn->m_Next;
            }
            if (chn->m_Next) {
                chn->m_Next->m_Prev = chn->m_Prev;
            }
            delete chn;
        }

    public:
        void Cancel(EventChainHandle<C>* handle) {
            if (handle->m_Chain && handle->m_Mgr == this) {
                handle->m_Chain->Cancel();
                UnlinkChain(handle->m_Chain); //EventChain dtor will call handle->Reset
            }
        }

        void CancelAll() {
            EventChain<C>* chn = m_Events;
            while (chn) {
                EventChain<C>* next = chn->m_Next;

                chn->Cancel();

                if (!m_IsInEvent) {
                    delete chn;
                }
                chn = next;
            }
            if (!m_IsInEvent) {
                m_Events = nullptr;
            }
        }

        bool Update() {
            EventChain<C>* chn = m_Events;
            bool running = false;
            while (chn) {
                bool endChain = !chn->m_CurEvent;
                if (!endChain) {
                    m_IsInEvent = true;
                    if (chn->m_CurEvent->Update(static_cast<void*>(m_Container))) {
                        Event<C>* previous = chn->m_CurEvent;
                        if (previous) {
                            //may have been canceled during update
                            chn->m_CurEvent = chn->m_CurEvent->m_Next;
                            delete previous;
                            endChain = !chn->m_CurEvent;
                        }
                        else {
                            endChain = true;
                        }
                    }
                    m_IsInEvent = false;
                }
                EventChain<C>* next = chn->m_Next;
                if (endChain) {
                    UnlinkChain(chn);
                }
                else {
                    running |= true;
                }
                chn = next;
            }
            return running;
        }
    };

    template<typename C>
    struct EventChain {
        Event<C>* m_CurEvent;
        EventChain* m_Prev{nullptr};
        EventChain* m_Next{nullptr};
        EventChainHandle<C>* m_Handle{nullptr};

        EventChain(Event<C>* event) {
            m_CurEvent = event;
        }

        ~EventChain() {
            if (m_Handle) {
                m_Handle->Reset();
            }
        }

        void Cancel() {
            Event<C>* e = m_CurEvent;
            while (e) {
                Event<C>* e2 = e->m_Next;
                delete e;
                e = e2;
            }
            m_CurEvent = nullptr;
        }
    };

    template<typename C>
    class EventQueue {
    public:
        struct Mutex {
            friend class EventQueue;
        private:
            bool m_On;
        public:
            Mutex() : m_On{false} {

            }
        };
    private:
        struct Node {
            Event<C>* m_Event;
            Mutex* m_Mutex;
            Node* m_Next;

            ~Node() {
                m_Mutex->m_On = false;
            }
        };

        EventManager<C>* m_EventMgr;
        
        Node* m_EventHead;
        Node* m_EventTail;
    
    public:
        EventQueue(EventManager<C>* mgr) : m_EventHead{nullptr}, m_EventTail{nullptr} {
            m_EventMgr = mgr;
        }

        void Clear() {
            Node* n = m_EventHead;
            while (n) {
                Node* next = n->m_Next;
                delete n->m_Event;
                delete n;
                n = next;
            }
            m_EventHead = nullptr;
            m_EventTail = nullptr;
        }

        void Execute() {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                Node* n = m_EventHead;
                while (n) {
                    Node* next = n->m_Next;
                    m_EventMgr->Start(n->m_Event);
                    delete n;
                    n = next;
                }
                m_EventHead = nullptr;
                m_EventTail = nullptr;
            }
        }
    private:
        void InsertNode(Node* n) {
            if (!m_EventHead) {
                m_EventHead = n;
            }
            else {
                m_EventTail->m_Next = n;
            }
            m_EventTail = n;
        }
    public:
        Event<C>* Queue(Event<C>* event) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                Node* n = new Node{event, nullptr, nullptr};
                InsertNode(n);
            }
            return event;
        }

        template<typename F, typename D>
        Event<C>* Queue(F func, D* data) {
            return Queue(new Event<C>(func, data));
        }

        template<typename F>
        Event<C>* Then(F func) {
            return Queue(new Event<C>(func));
        }

        bool QueueExclusive(Event<C>* event, Mutex& mutex) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (!mutex.m_On) {
                    mutex.m_On = true;
                    Node* n = new Node{event, &mutex, nullptr};
                    InsertNode(n);
                    return true;
                }
            }
            return false;
        }
    };
}

#endif