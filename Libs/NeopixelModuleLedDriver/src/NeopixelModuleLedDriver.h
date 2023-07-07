#ifndef __NEOPIXELMODULELEDDRIVER_H
#define __NEOPIXELMODULELEDDRIVER_H

#include "ModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"
#include "BombComponent.h"

class NeopixelModuleLedDriver : public ModuleLedDriver {
private:
    Adafruit_NeoPixel m_Neopixel;

public:
    NeopixelModuleLedDriver();

    void Init() override {
        m_Neopixel.begin();
    }

    void TurnOn(ledcolor_t color) override {
        m_Neopixel.setPixelColor(0, color);
        m_Neopixel.show();
    }

    void TurnOff() override {
        m_Neopixel.clear();
        m_Neopixel.show();
    }

    void SetPin(int pin) {
        m_Neopixel.setPin(pin);
    }
};

class NeopixelLedModuleTrait {
private:
    NeopixelModuleLedDriver m_NeopixelLedDriver;

protected:
    ModuleLedDriver* GetNeopixelLedDriver() {
        return &m_NeopixelLedDriver;
    }

    void SetModuleLedPin(int pin) {
        m_NeopixelLedDriver.SetPin(pin);
    }
};

#endif