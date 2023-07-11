#ifndef __ADDRESSOBTAINER_H
#define __ADDRESSOBTAINER_H

#include "Arduino.h"
#include "UARTPrint.h"

class AddressObtainer {
public:
    static int FromAnalogPin(int pin) {
        pinMode(pin, INPUT);
        int val = analogRead(pin);
        PRINTF_P("Analog value %d\n", val);
        int addressRange = 0x77 - 0x8;
        
        return (int)(0x8 + (long)addressRange * (long)val / 1023L);
    }
};

#endif