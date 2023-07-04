#include "Common.h"

IDHASH HashID(const char* name) {
    if (!name) {
        return 0;
    }
    IDHASH hash = 0x811C9DC5ul; //offset_basis
    unsigned char c;
    while (true) {
        c = *name;
        if (!c) {
            break;
        }
        hash = (hash ^ c) * 16777619ul; //FNV_prime
        name++;
    }
    return hash;
}