#ifndef __DEBUGPRINT_H
#define __DEBUGPRINT_H

#include "UARTPrint.h"

#ifdef DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__);
#define DEBUG_PRINTF_P(format, ...) PRINTF_P(format, __VA_ARGS__);
#define DEBUG_PRINTLN(str) puts(str);
#define DEBUG_PRINTLN_P(str) puts_P(PSTR(str));
#else
#define DEBUG_PRINTF(...) 
#define DEBUG_PRINTF_P(...)
#define DEBUG_PRINTLN(str) 
#define DEBUG_PRINTLN_P(str)
#endif

#endif