#ifndef __MODULELEDDRIVER_H
#define __MODULELEDDRIVER_H

#include <stdint.h>

class ModuleLedDriver {
public:
    typedef uint32_t ledcolor_t;

    virtual void Init() = 0;
    virtual void TurnOff() = 0;
    virtual void TurnOn(ledcolor_t color) = 0;
};

#endif