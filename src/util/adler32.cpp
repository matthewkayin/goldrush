#include "adler32.h"

#include "core/logger.h"
#include "core/asserts.h"
#include <SDL3/SDL.h>

#define MOD_ADLER 65521U
#define NMAX 5552

uint32_t adler32_scaler(uint8_t* data, size_t length) {
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

    if (length) {
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
        while (length) {
            b += (a += *data++);
            --length;
        }
        a %= MOD_ADLER;
        b %= MOD_ADLER;
    }

    return a | (b << 16);
}

#ifdef __SSE3__
    #include <xmmintrin.h>

    uint32_t adler32_sse3(uint8_t* data, size_t length) {

    }
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>

    uint32_t adler32_neon(uint8_t* data, size_t length) {
        uint32_t a = 1;
        uint32_t b = 0;

        // Serially compute a and b, until the data is 16-byte aligned
        // This works by checking to see if the address of data has any bits within the binary of 15 (00001111)
        // Also the if statement before the while is to avoid unnecessary modulous if the data is already 16 byte aligned
        if ((uintptr_t)data & 15) {
            while ((uintptr_t)data & 15) {
                b += (a += *data++);
                --length;
            }
            if (a >= MOD_ADLER) {
                a -= MOD_ADLER;
            }
            b %= MOD_ADLER;
        }

        // Now process the data in blocks
        const uint32_t BLOCK_SIZE = 1 << 5;
        size_t blocks_remaining = length / BLOCK_SIZE;
        length -= blocks_remaining * BLOCK_SIZE;

        while (blocks_remaining) {
            uint32_t n = NMAX / BLOCK_SIZE;
            if (n > blocks_remaining) {
                n = blocks_remaining;
            }
            blocks_remaining -= n;

            uint32x4_t vector_b = (uint32x4_t) { 0, 0, 0, a * n };
            uint32x4_t vector_a = (uint32x4_t) { 0, 0, 0, 0 };

            // vdupq_n_uint16 is vector *dup*licate scaler
            // so this code initializes each sum as 0
            uint16x8_t vector_column_sum1 = vdupq_n_u16(0);
            uint16x8_t vector_column_sum2 = vdupq_n_u16(0);
            uint16x8_t vector_column_sum3 = vdupq_n_u16(0);
            uint16x8_t vector_column_sum4 = vdupq_n_u16(0);

            do {
                // Load 32 input bytes
                const uint8x16_t bytes1 = vld1q_u8(data);
                const uint8x16_t bytes2 = vld1q_u8(data + 16);

                // Add previous block byte sum to b
                vector_b = vaddq_u32(vector_b, vector_a);

                // Horizontally add the bytes for a
                vector_a = vpadalq_u16(vector_a, vpadalq_u8(vpaddlq_u8(bytes1), bytes2));

                // Vertically add the bytes for b
                vector_column_sum1 = vaddw_u8(vector_column_sum1, vget_low_u8(bytes1));
                vector_column_sum2 = vaddw_u8(vector_column_sum2, vget_high_u8(bytes1));
                vector_column_sum3 = vaddw_u8(vector_column_sum3, vget_low_u8(bytes2));
                vector_column_sum4 = vaddw_u8(vector_column_sum4, vget_high_u8(bytes2));

                data += BLOCK_SIZE;
            } while (--n);

            vector_b = vshlq_n_u32(vector_b, 5);

            // Multiply-add bytes by [ 32, 31, 30, ... ] for b
            vector_b = vmlal_u16(vector_b, vget_low_u16(vector_column_sum1),
                (uint16x4_t) { 32, 31, 30, 29 });
            vector_b = vmlal_u16(vector_b, vget_high_u16(vector_column_sum1),
                (uint16x4_t) { 28, 27, 26, 25 });
            vector_b = vmlal_u16(vector_b, vget_low_u16(vector_column_sum2),
                (uint16x4_t) { 24, 23, 22, 21 });
            vector_b = vmlal_u16(vector_b, vget_high_u16(vector_column_sum2),
                (uint16x4_t) { 20, 19, 18, 17 });
            vector_b = vmlal_u16(vector_b, vget_low_u16(vector_column_sum3),
                (uint16x4_t) { 16, 15, 14, 13 });
            vector_b = vmlal_u16(vector_b, vget_high_u16(vector_column_sum3),
                (uint16x4_t) { 12, 11, 10,  9 });
            vector_b = vmlal_u16(vector_b, vget_low_u16(vector_column_sum4),
                (uint16x4_t) {  8,  7,  6,  5 });
            vector_b = vmlal_u16(vector_b, vget_high_u16(vector_column_sum4),
                (uint16x4_t) {  4,  3,  2,  1 });

            // Sum epi32 ints v_s1(s2) and accumulate in s1(s2).
            uint32x2_t sum_a = vpadd_u32(vget_low_u32(vector_a), vget_high_u32(vector_a));
            uint32x2_t sum_b = vpadd_u32(vget_low_u32(vector_b), vget_high_u32(vector_b));
            uint32x2_t ab = vpadd_u32(sum_a, sum_b);

            a += vget_lane_u32(ab, 0);
            b += vget_lane_u32(ab, 1);

            // Reduce
            a %= MOD_ADLER;
            b %= MOD_ADLER;
        }

        if (length) {
            if (length >= 16) {
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

            while (length) {
                b += (a += *data++);
                --length;
            }

            if (a >= MOD_ADLER) {
                a -= MOD_ADLER;
            }
            b %= MOD_ADLER;
        }

        return a | (b << 16);
    }
#endif

uint32_t adler32(uint8_t* data, size_t length) {
    #ifdef __ARM_NEON
        if (SDL_HasNEON()) {
            return adler32_neon(data, length);
        }
    #endif

    #ifdef __SSE3__
        if (SDL_HasSSE3()) {
            return adler32_ss3(data, length);
        }
    #endif

    return adler32_scaler(data, length);
}

#ifdef GOLD_SIMD_CHECKSUM_TEST

void adler32_test(uint8_t* data, size_t length) {
    #ifdef __ARM_NEON
        if (!SDL_HasNEON()) {
            log_error("Cannot test adler32 without NEON.");
            return;
        }
    #endif

    #ifdef __SSE3__
        if (!SDL_HasSSE3()) {
            log_error("Cannot test adler32 without SSE3.");
            return false;
        }
    #endif

    uint32_t simd_checksum = adler32(data, length);
    uint32_t scaler_checksum = adler32_scaler(data, length);
    if (simd_checksum == scaler_checksum) {
        log_info("SIMD CHECKSUM test pass. simd %u vs scaler %u", simd_checksum, scaler_checksum);
    } else {
        log_error("SIMD CHECKSUM test fail. simd %u vs scaler %u", simd_checksum, scaler_checksum);
    }
}

#else

#define adler32_test(data, length)

#endif