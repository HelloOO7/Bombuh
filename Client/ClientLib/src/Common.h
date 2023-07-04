#ifndef __COMMON_H
#define __COMMON_H

#include <stdint.h>
#include <stddef.h>

#ifdef AVR
#define AVR_SASSERT(expr) static_assert(expr);
#else
#define AVR_SASSERT(expr)
#endif

typedef int32_t bombclock_t;

typedef uint32_t IDHASH;
IDHASH HashID(const char* name);

template<typename T>
struct FixedArrayRef {
private:
    size_t Count;
    T*     Elements;
public:
    inline size_t Size() {
        return Count;
    }

    inline T& operator[](int index) const
    {
        return Elements[index];
    }

    inline T** ArrayPointer() {
        return &Elements;
    }
};

#endif