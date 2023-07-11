#ifndef __GAMEEVENT_H
#define __GAMEEVENT_H

#include "Arduino.h"

namespace game {
    template<typename C>
    struct EventChain;

    template<typename C>
    class Event;

    template<typename C>
    class EventManager;

    typedef bool(*EventFunc)(void*, void*, void*);

    template<typename C>
    class Event {
        friend class EventManager<C>;
        friend class EventChain<C>;
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
    
    public:
        template<typename F, typename D>
        Event(F func, D* data) : m_Data{static_cast<void*>(data)}, m_Next{nullptr} {
            bool(*_func)(Event*, C*, D*) = static_cast<bool(*)(Event*, C*, D*)>(func);
            m_Func = (EventFunc) _func;
            m_FreeData = true;
        }

        template<typename F>
        Event(F func) : m_Data{nullptr}, m_Next{nullptr} {
            bool(*_func)(Event*, C*) = static_cast<bool(*)(Event*, C*)>(func);
            m_Func = (EventFunc) _func;
            m_FreeData = false;
        }

        ~Event() {
            if (m_FreeData && m_Data) {
                free(m_Data);
            }
        }

        Event* SetDataPermanent() {
            m_FreeData = false;
            return this;
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
        friend class EventChain<C>;
    private:
        EventManager<C>* m_Mgr;
        EventChain<C>* m_Chain;
        bool           m_Active;

    public:
        EventChainHandle() {
            m_Active = false;
        }

    private:
        void SetChain(EventManager<C>* mgr, EventChain<C>* chain) {
            m_Mgr = mgr;
            m_Chain = chain;
            m_Active = true;
        }

    public:
        void Cancel() {
            if (m_Active) {
                m_Mgr->Cancel(this);
            }
        }
    };

    template<typename C>
    class EventManager {
    private:
        EventChain<C>* m_Events;
        C*  m_Container;
    
    public:
        EventManager(C* container) : m_Events{nullptr}, m_Container{container} {
            
        }

        void Start(Event<C>* event, EventChainHandle<C>* handle = nullptr) {
            if (event) {
                EventChain<C>* chain = new EventChain<C> {event, nullptr, m_Events, handle};
                while (event) {
                    event->m_Chain = chain;
                    event = event->m_Next;
                }
                m_Events->m_Prev = chain;
                m_Events = chain;
                if (handle) {
                    handle->SetChain(this, chain);
                    chain->m_Handle = handle;
                }
            }
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
            handle->m_Chain->Cancel();
            UnlinkChain(handle->m_Chain);
        }

        void CancelAll() {
            EventChain<C>* chn = m_Events;
            while (chn) {
                EventChain<C>* next = chn->m_Next;

                Event<C>* e = chn->m_CurEvent;
                while (e) {
                    Event<C>* nexte = e->m_Next;
                    delete e;
                    e = nexte;
                }

                delete chn;
                chn = next;
            }
            m_Events = nullptr;
        }

        bool Update() {
            EventChain<C>* chn = m_Events;
            bool running = false;
            while (chn) {
                bool endChain = !chn->m_CurEvent;
                if (!endChain) {
                    if (chn->m_CurEvent->Update(static_cast<void*>(m_Container))) {
                        Event<C>* previous = chn->m_CurEvent;
                        chn->m_CurEvent = chn->m_CurEvent->m_Next;
                        delete previous;
                        endChain = !chn->m_CurEvent;
                    }
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
        EventChain* m_Prev;
        EventChain* m_Next;
        EventChainHandle<C>* m_Handle;

        ~EventChain() {
            m_Handle->m_Active = false;
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
}

#endif