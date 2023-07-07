#include "NeopixelModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"

NeopixelModuleLedDriver::NeopixelModuleLedDriver() : m_Neopixel() {
    m_Neopixel.updateLength(1);
    m_Neopixel.updateType(NEO_RGB | NEO_KHZ800);
}