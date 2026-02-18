#include "adler32.h"

#define MOD_ADLER 65521U
#define NMAX 5552

uint32_t adler32(uint8_t* data, size_t length) {
    uint32_t a = 1;
    uint32_t b = 0;
    uint32_t n;

    while (length >= NMAX) {
        n = NMAX / 16;
        do {
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
        } while (--n);
        a %= MOD_ADLER;
        b %= MOD_ADLER;
        length -= NMAX;
    }

    if (length > 0) {
        while (length >= 16) {
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);

            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            b += (a += *data++);
            length -= 16;
        }
        while (length > 0) {
            b += (a += *data++);
            length--;
        }
        a %= MOD_ADLER;
        b %= MOD_ADLER;
    }

    return a | (b << 16);
}