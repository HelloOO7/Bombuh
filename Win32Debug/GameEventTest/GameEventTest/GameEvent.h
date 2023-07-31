#ifndef __GAMEEVENT_H
#define __GAMEEVENT_H

#define function []

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
            handle->m_Chain->Cancel();
            UnlinkChain(handle->m_Chain);
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
                        if (chn->m_CurEvent) {
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
        EventChainHandle<C>* m_Handle;

        EventChain(Event<C>* event) {
            m_CurEvent = event;
        }

        ~EventChain() {
            if (m_Handle) {
                m_Handle->m_Active = false;
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
}

#endif