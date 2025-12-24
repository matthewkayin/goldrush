#include "noise.h"

#include "core/asserts.h"
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

    uint64_t xsbp = (uint64_t)xsb * PRIME_X;
    uint64_t ysbp = (uint64_t)ysb * PRIME_X;

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

uint8_t noise_value_from_result(MapType map_type, double result) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA: {
            if (result < 0.15) {
                return NOISE_VALUE_WATER;
            } else if (result < 0.6) { 
                return NOISE_VALUE_LOWGROUND;
            } else {
                return NOISE_VALUE_HIGHGROUND;
            }
        }
        case MAP_TYPE_KLONDIKE: {
            if (result < 0.4) {
                return NOISE_VALUE_HIGHGROUND;
            } else if (result < 0.9) {
                return NOISE_VALUE_LOWGROUND;
            } else {
                return NOISE_VALUE_WATER;
            }
        }
        case MAP_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return NOISE_VALUE_LOWGROUND;
        }
    }
}

uint8_t noise_forest_value_from_result(MapType map_type, double result) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA:
            return 0;
        case MAP_TYPE_KLONDIKE:
            return (uint8_t)(result < 0.25);
        case MAP_TYPE_COUNT:
            GOLD_ASSERT(false);
            return 0;
    }
}

Noise* noise_init(int width, int height) {
    Noise* noise = (Noise*)malloc(sizeof(Noise));
    noise->width = width;
    noise->height = height;
    noise->map = (uint8_t*)malloc(noise->width * noise->height * sizeof(uint8_t));
    noise->forest = (uint8_t*)malloc(noise->width * noise->height * sizeof(uint8_t));

    return noise;
}

Noise* noise_generate(MapType map_type, uint64_t seed, uint64_t forest_seed, int width, int height) {
    Noise* noise = noise_init(width, height);

    const double FREQUENCY = 1.0 / 56.0;
    const double FOREST_FREQUENCY = 1.0 / 8.0;

    for (int x = 0; x < noise->width; x++) {
        for (int y = 0; y < noise->height; y++) {
            // simplex_noise generates a result from -1 to 1, so we convert to the range 0 to 1
            double perlin_result = (1.0 + simplex_noise(seed, x * FREQUENCY, y * FREQUENCY)) * 0.5;
            noise->map[x + (y * noise->width)] = noise_value_from_result(map_type, perlin_result);

            double forest_result = (1.0 + simplex_noise(forest_seed, x * FOREST_FREQUENCY, y * FOREST_FREQUENCY)) * 0.5;
            noise->forest[x + (y * noise->width)] = noise_forest_value_from_result(map_type, forest_result);
        }
    }

    return noise;
}

void noise_free(Noise* noise) {
    free(noise->map);
    free(noise->forest);
    free(noise);
}

size_t noise_serialized_size(const Noise* noise) {
    return sizeof(noise->width) + 
        sizeof(noise->height) + 
        // map
        (noise->width * noise->height * sizeof(uint8_t)) +
        // forest
        (noise->width * noise->height * sizeof(uint8_t));
}

size_t noise_serialize(const Noise* noise, uint8_t* buffer) {
    size_t offset = 0;

    memcpy(buffer, &noise->width, sizeof(noise->width));
    offset += sizeof(noise->width);

    memcpy(buffer + offset, &noise->height, sizeof(noise->height));
    offset += sizeof(noise->height);

    memcpy(buffer + offset, noise->map, noise->width * noise->height * sizeof(uint8_t));
    offset += noise->width * noise->height * sizeof(uint8_t);

    memcpy(buffer + offset, noise->forest, noise->width * noise->height * sizeof(uint8_t));
    offset += noise->width * noise->height * sizeof(uint8_t);

    return offset;
}

Noise* noise_deserialize(uint8_t* buffer) {
    int width, height;
    size_t offset = 0;

    memcpy(&width, buffer, sizeof(width));
    offset += sizeof(width);

    memcpy(&height, buffer + offset, sizeof(height));
    offset += sizeof(height);

    Noise* noise = noise_init(width, height);

    memcpy(noise->map, buffer + offset, noise->width * noise->height * sizeof(uint8_t));
    offset += noise->width * noise->height * sizeof(uint8_t);

    memcpy(noise->forest, buffer + offset, noise->width * noise->height * sizeof(uint8_t));
    offset += noise->width * noise->height * sizeof(uint8_t);

    return noise;
}

void noise_fwrite(const Noise* noise, FILE* file) {
    fwrite(&noise->width, 1, sizeof(noise->width), file);
    fwrite(&noise->height, 1, sizeof(noise->height), file);
    fwrite(noise->map, 1, noise->width * noise->height * sizeof(uint8_t), file);
    fwrite(noise->forest, 1, noise->width * noise->height * sizeof(uint8_t), file);
}

Noise* noise_fread(FILE* file) {
    int width, height;
    fread(&width, 1, sizeof(width), file);
    fread(&height, 1, sizeof(width), file);

    Noise* noise = noise_init(width, height);
    fread(noise->map, 1, noise->width * noise->height * sizeof(uint8_t), file);
    fread(noise->forest, 1, noise->width * noise->height * sizeof(uint8_t), file);

    return noise;
}