#ifndef PROMISE_DEBUG
#undef DEBUG
#endif

#include "Promise.h"

PromiseStub::~PromiseStub() {

}

void PromiseStub::Resolve(void* resp) {

}

void PromiseStub::OnEnqueued(void* lastResp) {
    
}

#ifdef DEBUG
static int g_DebugPromiseIcount = 0;
#endif

Promise::Promise(void* context) {
    m_ThenResolve = nullptr;
    m_Next = nullptr;
    m_Context = context;
    m_IsLast = false;
    DEBUG_PRINTF_P("New promise %p, instance count: %d\n", this, ++g_DebugPromiseIcount)
}

Promise::~Promise() {
    DEBUG_PRINTF_P("Del promise %p, instance count: %d\n", this, --g_DebugPromiseIcount)
}

void Promise::Resolve(void* resp) {
    if (m_ThenResolve) {
        if (m_IsLast) {
            DEBUG_PRINTF_P("Resolving last promise %p...\n", this)
            m_ThenResolve(m_Context, resp); //undefined return
            if (m_Next) {
                m_Next->Resolve(resp);
            }
        }
        else {
            DEBUG_PRINTF_P("Resolving promise %p...\n", this)
            PromiseStub* next = m_ThenResolve(m_Context, resp);
            if (next) {
                PromiseStub* _next = next;
                //if this promise continues, make sure it is only after the now returned finishes
                if (m_Next) {
                    while (next->m_Next) {
                        next = next->m_Next; //scroll to end of previous promise chain
                    }
                    if (next->m_ThenResolve) {
                        PromiseStub* _next = next;
                        next = new Promise(m_Context);
                        _next->m_Next = next;
                    }
                    next->m_ThenResolve = m_Next->m_ThenResolve;
                    next->m_IsLast = m_Next->m_IsLast;
                    next->m_Next = m_Next->m_Next;
                    delete m_Next;
                }
                _next->OnEnqueued(resp);
            }
            else if (m_Next) {
                PromiseStub* n = m_Next;
                while (n) {
                    PromiseStub* n2 = n->m_Next;
                    delete n;
                    n = n2;
                }
            }
        }
    }
    else {
        DEBUG_PRINTF_P("Promise %p has no successors!\n", this)
    }
    delete this;
}

void Promise::Reject() {
    if (m_Next) {
        PromiseStub* n = m_Next;
        while (n) {
            PromiseStub* n2 = n->m_Next;
            delete n;
            n = n2;
        }
    }
    delete this;
}

EmptyPromise::EmptyPromise(void* context) : Promise(context) {

}

void EmptyPromise::OnEnqueued(void* lastResp) {
    Resolve(lastResp);
}