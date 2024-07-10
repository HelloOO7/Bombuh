#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include <stddef.h>

template<typename T, size_t size>
class RingBuffer {
private:
    T      m_Data[size];
    size_t m_WritePos{0};
    size_t m_ReadPos{0};
    bool   m_Full{false};

public:
    inline bool HasNext() {
        return m_ReadPos != m_WritePos || m_Full;
    }

    bool Push(T element) {
        if (m_Full) {
            return false;
        }
        m_Data[m_WritePos++] = element;
        if (m_WritePos == size) {
            m_WritePos = 0;
        }
        if (m_WritePos == m_ReadPos) {
            m_Full = true;
        }
        return true;
    }

    T& Pop() {
        T& retval = m_Data[m_ReadPos++];
        if (m_ReadPos == size) {
            m_ReadPos = 0;
        }
        return retval;
    }
};

#endif