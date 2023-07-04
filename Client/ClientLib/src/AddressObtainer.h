#ifndef __ADDRESSOBTAINER_H
#define __ADDRESSOBTAINER_H

#include "Arduino.h"

class AddressObtainer {
public:
    static int FromAnalogPin(int pin) {
        pinMode(pin, INPUT);
        int val = analogRead(pin) & 0b1111111;
        if (val < 0x8) {
            val = 0x8;
        }
        if (val >= 0x78) {
            val = 0x77;
        }
        return val;
    }
};

#endif