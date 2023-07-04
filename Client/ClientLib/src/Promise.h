#ifndef __PROMISE_H
#define __PROMISE_H

#include "DebugPrint.h"

class Promise;

class PromiseStub {
public:
    typedef PromiseStub*(*ResolveHandler)(void*, void*);
protected:
    virtual ~PromiseStub();

    virtual void Resolve(void* resp);
    virtual void OnEnqueued(void* lastResp);

    ResolveHandler m_ThenResolve;

    PromiseStub* m_Next;

    bool m_IsLast;

    friend class Promise;
};

class Promise : public PromiseStub {
private:
    void*       m_Context;

public:
    Promise(void* context);

    ~Promise() override;

    template<typename C>
    Promise* Then(Promise*(*onResolve)(C*)) {
        m_ThenResolve = reinterpret_cast<ResolveHandler>(onResolve);
        Promise* next = new Promise(m_Context);
        m_Next = next;
        return next;
    }

    template<typename C, typename R>
    Promise* Then(Promise*(*onResolve)(C*, R*)) {
        m_ThenResolve = reinterpret_cast<ResolveHandler>(onResolve);
        Promise* next = new Promise(m_Context);
        m_Next = next;
        return next;
    }

    template<typename C, typename R>
    void Then(void(*onResolve)(C*, R*)) {
        m_ThenResolve = reinterpret_cast<ResolveHandler>(onResolve);
        m_IsLast = true;
    }

    template<typename C>
    void Then(void(*onResolve)(C*)) {
        m_ThenResolve = reinterpret_cast<ResolveHandler>(onResolve);
        m_IsLast = true;
    }

    void Resolve(void* resp) override;

    void Reject();
};

class EmptyPromise : public Promise {
public:
    EmptyPromise(void* context);

    virtual void OnEnqueued(void* lastResp) override;
};

#endif