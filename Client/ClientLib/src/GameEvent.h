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
            m_FreeData = false;
        }

        template<typename F>
        Event(F func) : m_Data{nullptr}, m_Next{nullptr} {
            bool(*_func)(Event*, C*) = static_cast<bool(*)(Event*, C*)>(func);
            m_Func = (EventFunc) _func;
            m_FreeData = false;
        }

        ~Event() {
            if (m_FreeData) {
                free(m_Data);
            }
        }

        Event* SetDataTransient() {
            m_FreeData = true;
            return this;
        }
    
        Event* Then(Event* nextEvent) {
            Event* oldnext = m_Next;
            m_Next = nextEvent;
            nextEvent->m_Chain = m_Chain;
            if (oldnext) {
                Event* end = nextEvent;
                while (end->m_Next) {
                    end = end->m_Next;
                    end->m_Chain = m_Chain;
                }
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
        unsigned long long* data = new unsigned long long[2];
        data[0] = 0;
        data[1] = ms;
        return (new Event<C>(function(Event<C>* e, C* context, unsigned long long* data) {
            unsigned long long time = millis();
            if (!data[0]) {
                data[0] = time + data[1];
            }
            return time >= data[0];
        }, data))->SetDataTransient();
    }

    template<typename C>
    class EventManager {
    private:
        EventChain<C>* m_Events;
        C*  m_Container;
    
    public:
        EventManager(C* container) : m_Events{nullptr}, m_Container{container} {
            
        }

        void Start(Event<C>* event) {
            if (event) {
                EventChain<C>* chain = new EventChain<C> {event, m_Events, nullptr};
                while (event) {
                    event->m_Chain = m_Events;
                    event = event->m_Next;
                }
                m_Events->m_Next = chain;
                m_Events = chain;
            }
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