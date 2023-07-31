#include <stdio.h>
#include "Arduino.h"

#include "UARTPrint.h"

#ifndef DISABLE_SERIAL_PRINT
static FILE uartout = {0};

// create a output function
// This works because Serial.write, although of
// type virtual, already exists.
static int uart_putchar(char c, FILE *stream);

// create a output function
// This works because Serial.write, although of
// type virtual, already exists.
int uart_putchar(char c, FILE *stream)
{
    Serial.write(c) ;
    return 0 ;
}
#endif

void print_init(unsigned long baud)
{
    #ifndef DISABLE_SERIAL_PRINT
    Serial.begin(baud);

    // fill in the UART file descriptor with pointer to writer.
    fdev_setup_stream(&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);

    // The uart is the standard output device STDOUT.
    stdout = &uartout;
    #endif
}