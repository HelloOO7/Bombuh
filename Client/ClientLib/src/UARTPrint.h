#ifndef __UARTPRINT_H
#define __UARTPRINT_H

#include <stdio.h>
#include "Arduino.h"

//https://playground.arduino.cc/Main/Printf/

void print_init(unsigned long baud);

#define PRINTF_P(format, ...) printf_P(PSTR(format), __VA_ARGS__)

#endif