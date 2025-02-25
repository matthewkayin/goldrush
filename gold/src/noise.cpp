#include "noise.h"

#include "util.h"
#include <cstdlib>
#include <vector>

int fast_floor(double x) {
    int xi = (int)x;
    return x < xi ? xi - 1 : xi;
}

float grad(uint64_t seed, uint64_t xsvp, uint64_t ysvp, float dx, float dy) {
    const uint64_t HASH_MULTIPLIER = 6026932503003350773;
    const int N_GRADS_2D_EXPONENT = 7;
    const int N_GRADS_2D = 1 << N_GRADS_2D_EXPONENT;
    const double NORMALIZER_2D = 0.05481866495625118;
    static float GRADIENTS_2D[N_GRADS_2D * 2];
    static bool gradients_initialized = false;

    if (!gradients_initialized) {
        float grad2[48] = {
            0.38268343236509f,   0.923879532511287f,
            0.923879532511287f,  0.38268343236509f,
            0.923879532511287f, -0.38268343236509f,
            0.38268343236509f,  -0.923879532511287f,
            -0.38268343236509f,  -0.923879532511287f,
            -0.923879532511287f, -0.38268343236509f,
            -0.923879532511287f,  0.38268343236509f,
            -0.38268343236509f,   0.923879532511287f,
            //-------------------------------------//
            0.130526192220052f,  0.99144486137381f,
            0.608761429008721f,  0.793353340291235f,
            0.793353340291235f,  0.608761429008721f,
            0.99144486137381f,   0.130526192220051f,
            0.99144486137381f,  -0.130526192220051f,
            0.793353340291235f, -0.60876142900872f,
            0.608761429008721f, -0.793353340291235f,
            0.130526192220052f, -0.99144486137381f,
            -0.130526192220052f, -0.99144486137381f,
            -0.608761429008721f, -0.793353340291235f,
            -0.793353340291235f, -0.608761429008721f,
            -0.99144486137381f,  -0.130526192220052f,
            -0.99144486137381f,   0.130526192220051f,
            -0.793353340291235f,  0.608761429008721f,
            -0.608761429008721f,  0.793353340291235f,
            -0.130526192220052f,  0.99144486137381f,
        };
        for (int i = 0; i < 48; i++) {
            grad2[i] = (float)(grad2[i] / NORMALIZER_2D);
        }
        for (int i = 0; i < N_GRADS_2D * 2; i++) {
            GRADIENTS_2D[i] = grad2[i % 48];
        }
        gradients_initialized = true;
    }

    uint64_t hash = (seed ^ xsvp ^ ysvp) * HASH_MULTIPLIER;
    hash ^= hash >> (64 - N_GRADS_2D_EXPONENT + 1);
    int gi = (int)hash & ((N_GRADS_2D - 1) << 1);
    return GRADIENTS_2D[gi | 0] * dx + GRADIENTS_2D[gi | 1] * dy;
}

float simplex_noise(uint64_t seed, double x, double y) {
    const double SKEW_2D = 0.366025403784439;
    const double UNSKEW_2D = -0.21132486540518713;
    const float RSQUARED_2D = 2.0f / 3.0f;
    const uint64_t PRIME_X = 5910200641878280303;
    const uint64_t PRIME_Y = 6452764530575939509;

    double skew = SKEW_2D * (x + y);
    double xs = x + skew;
    double ys = y + skew;

    int xsb = fast_floor(xs);
    int ysb = fast_floor(ys);
    float xi = (float)(xs - xsb);
    float yi = (float)(ys - ysb);

    uint64_t xsbp = xsb * PRIME_X;
    uint64_t ysbp = ysb * PRIME_X;

    float t = (xi + yi) * (float)UNSKEW_2D;
    float dx0 = xi + t;
    float dy0 = yi + t;

    float a0 = RSQUARED_2D - (dx0 * dx0) - (dy0 * dy0);
    float value = (a0 * a0) * (a0 * a0) * grad(seed, xsbp, ysbp, dx0, dy0);

    float a1 = (float)(2 * (1 + 2 * UNSKEW_2D) * (1 / UNSKEW_2D + 2)) * t + ((float)(-2 * (1 + 2 * UNSKEW_2D) * (1 + 2 * UNSKEW_2D)) + a0);
    float dx1 = dx0 - (float)(1 + 2 * UNSKEW_2D);
    float dy1 = dy0 - (float)(1 + 2 * UNSKEW_2D);
    value += (a1 * a1) * (a1 * a1) * grad(seed, xsbp + PRIME_X, ysbp + PRIME_Y, dx1, dy1);

    float xmyi = xi - yi;
    if (t < UNSKEW_2D) {
        if (xi + xmyi > 1) {
            float dx2 = dx0 - (float)(3 * UNSKEW_2D + 2);
            float dy2 = dy0 - (float)(3 * UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp + (PRIME_X << 1), ysbp + PRIME_Y, dx2, dy2);
            }
        } else {
            float dx2 = dx0 - (float)UNSKEW_2D;
            float dy2 = dy0 - (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp, ysbp + PRIME_Y, dx2, dy2);
            }
        }

        if (yi - xmyi > 1) {
            float dx3 = dx0 - (float)(3 * UNSKEW_2D + 1);
            float dy3 = dy0 - (float)(3 * UNSKEW_2D + 2);
            float a3 = RSQUARED_2D - dx3 * dx3 - dy3 * dy3;
            if (a3 > 0) {
                value += (a3 * a3) * (a3 * a3) * grad(seed, xsbp + PRIME_X, ysbp + (PRIME_Y << 1), dx3, dy3);
            }
        } else {
            float dx3 = dx0 - (float)(UNSKEW_2D + 1);
            float dy3 = dy0 - (float)UNSKEW_2D;
            float a3 = RSQUARED_2D - dx3 * dx3 - dy3 * dy3;
            if (a3 > 0) {
                value += (a3 * a3) * (a3 * a3) * grad(seed, xsbp + PRIME_X, ysbp, dx3, dy3);
            }
        }
    } else {
        if (xi + xmyi < 0) {
            float dx2 = dx0 + (float)(1 + UNSKEW_2D);
            float dy2 = dy0 + (float)UNSKEW_2D;
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp - PRIME_X, ysbp, dx2, dy2);
            }
        } else {
            float dx2 = dx0 - (float)(UNSKEW_2D + 1);
            float dy2 = dy0 - (float)UNSKEW_2D;
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp + PRIME_X, ysbp, dx2, dy2);
            }
        }

        if (yi < xmyi) {
            float dx2 = dx0 + (float)UNSKEW_2D;
            float dy2 = dy0 + (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp, ysbp - PRIME_Y, dx2, dy2);
            }
        } else {
            float dx2 = dx0 - (float)UNSKEW_2D;
            float dy2 = dy0 - (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0) {
                value += (a2 * a2) * (a2 * a2) * grad(seed, xsbp, ysbp + PRIME_Y, dx2, dy2);
            }
        }
    }

    return value;
}

noise_t noise_generate(uint64_t seed, uint32_t width, uint32_t height) {
    noise_t noise;
    noise.width = width;
    noise.height = height;
    noise.map = (int8_t*)malloc(noise.width * noise.height * sizeof(int8_t));

    const double FREQUENCY = 1.0 / 56.0;
    for (int x = 0; x < noise.width; x++) {
        for (int y = 0; y < noise.height; y++) {
            // Generates result from -1 to 1
            double perlin_result = (1.0 + simplex_noise(seed, x * FREQUENCY, y * FREQUENCY)) * 0.5;
            if (perlin_result < 0.15) {
                noise.map[x + (y * noise.width)] = -1;
            } else if (perlin_result < 0.6) {
                noise.map[x + (y * noise.width)] = 0;
            } else if (perlin_result < 0.8) {
                noise.map[x + (y * noise.width)] = 1;
            } else {
                noise.map[x + (y * noise.width)] = 2;
            }
        }
    }

    return noise;
}