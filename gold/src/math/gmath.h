#pragma once

#include "defines.h"
#include <cstdint>
#include <cmath>

// from https://github.com/chmike/fpsqrt/blob/master/fpsqrt.c
// sqrt_i64 computes the squrare root of a 64bit integer and returns
// a 64bit integer value. It requires that v is positive.
inline int64_t sqrt_i64(uint64_t v) {
    uint64_t b = ((uint64_t)1)<<62, q = 0, r = v;
    while (b > r)
        b >>= 2;
    while( b > 0 ) {
        uint64_t t = q + b;
        q >>= 1;           
        if( r >= t ) {     
            r -= t;        
            q += b;        
        }
        b >>= 2;
    }
    return q;
}

struct fixed {
    static const size_t integer_bits = 24;
    static const size_t fractional_bits = 8;
    static const int32_t scale_factor = (int32_t)1 << fractional_bits;
    static const int32_t fractional_mask = scale_factor - 1;
    static const size_t total_bits = integer_bits + fractional_bits;
    int32_t raw_value;

    static constexpr fixed from_int(int32_t integer_value) {
        return (fixed) { .raw_value = integer_value << fractional_bits };
    }
    static constexpr fixed from_raw(int32_t raw_value) {
        return (fixed) { .raw_value = raw_value };
    }
    static constexpr fixed from_int_and_raw_decimal(int32_t integer_value, int32_t raw_decimal_value) {
        return (fixed) { .raw_value = (integer_value << fractional_bits) + raw_decimal_value };
    }

    int32_t integer_part() const {
        return raw_value >> fractional_bits;
    }
    int32_t fractional_part() const {
        return raw_value & fractional_mask;
    }
    int32_t fractional_value() const {
        return ((raw_value & fractional_mask) * 1000) / scale_factor;
    }

    static fixed round(const fixed& value) {
        return fixed::from_int(value.integer_part() + (value.fractional_part() <= 128 ? 0 : 1));
    }
    static fixed abs(const fixed& value) {
        return value < fixed::from_raw(0) ? value * fixed::from_int(-1) : value;
    }

    bool operator==(const fixed& other) const {
        return raw_value == other.raw_value;
    }
    bool operator!=(const fixed& other) const {
        return raw_value != other.raw_value;
    }
    bool operator<(const fixed& other) const {
        return raw_value < other.raw_value;
    }
    bool operator<=(const fixed& other) const {
        return raw_value <= other.raw_value;
    }
    bool operator>(const fixed& other) const {
        return raw_value > other.raw_value;
    }
    bool operator>=(const fixed& other) const {
        return raw_value >= other.raw_value;
    }

    fixed operator-() const {
        return from_raw(-raw_value);
    }
    fixed& operator+=(const fixed& other) {
        raw_value += other.raw_value;
        return *this;
    }
    fixed operator+(const fixed& other) const {
        return from_raw(raw_value + other.raw_value);
    }
    fixed& operator-=(const fixed& other) {
        raw_value -= other.raw_value;
        return *this;
    }
    fixed operator-(const fixed& other) const {
        return from_raw(raw_value - other.raw_value);
    }
    fixed operator*(const fixed& other) const {
        return from_raw(((int64_t)raw_value * other.raw_value) >> fractional_bits);
    }
    fixed operator*(int scaler) const {
        return from_raw(raw_value * scaler);
    }
    fixed operator/(const fixed& other) const {
        return from_raw((((int64_t)raw_value) << fractional_bits) / other.raw_value);
    }
    fixed operator/(int scaler) const {
        return from_raw(raw_value / scaler);
    }
};

struct ivec2 {
    int x;
    int y;

    ivec2() = default;
    ivec2(int x, int y) : x(x), y(y) {}
    bool operator==(const ivec2& other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(const ivec2& other) const {
        return this->x != other.x || this->y != other.y;
    }
    ivec2 operator-() const {
        return ivec2(-x, -y);
    }
    ivec2 operator+(const ivec2& other) const {
        return ivec2(x + other.x, y + other.y);
    }
    ivec2& operator+=(const ivec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    ivec2 operator-(const ivec2& other) const {
        return ivec2(x - other.x, y - other.y);
    }
    ivec2& operator-=(const ivec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    ivec2 operator*(int scaler) const {
        return ivec2(x * scaler, y * scaler);
    }
    ivec2 operator/(int scaler) const {
        return ivec2(x / scaler, y / scaler);
    }
    static int manhattan_distance(const ivec2& a, const ivec2& b) {
        return abs(a.x - b.x) + abs(a.y - b.y);
    }
    static int euclidean_distance_squared(const ivec2& a, const ivec2& b) {
        return (abs(a.x - b.x) * abs(a.x - b.x)) + (abs(a.y - b.y) * abs(a.y - b.y));
    }
};

struct fvec2 {
    fixed x;
    fixed y;

    fvec2() = default;
    fvec2(fixed x, fixed y) : x(x), y(y) {}
    fvec2(const ivec2& other) : x(fixed::from_int(other.x)), y(fixed::from_int(other.y)) {}
    ivec2 to_ivec2() const {
        return ivec2(x.integer_part(), y.integer_part());
    }
    bool operator==(const fvec2& other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(const fvec2& other) const {
        return this->x != other.x || this->y != other.y;
    }
    fvec2 operator-() const {
        return fvec2(-x, -y);
    }
    fvec2 operator+(const fvec2& other) const {
        return fvec2(x + other.x, y + other.y);
    }
    fvec2& operator+=(const fvec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    fvec2 operator-(const fvec2& other) const {
        return fvec2(x - other.x, y - other.y);
    }
    fvec2& operator-=(const fvec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    fvec2 operator*(const fixed& scaler) const {
        return fvec2(x * scaler, y * scaler);
    }
    fvec2 operator/(const fixed& scaler) const {
        return fvec2(x / scaler, y / scaler);
    }
    fixed length() const {
        int64_t x64 = x.raw_value;
        int64_t y64 = y.raw_value;
        uint64_t length_squared = (x64 * x64) + (y64 * y64);
        return fixed::from_raw((int32_t)sqrt_i64(length_squared));
    }
    fixed distance_to(const fvec2& other) const {
        return (other - *this).length();
    }
};

enum direction {
    DIRECTION_NORTH,
    DIRECTION_NORTHEAST,
    DIRECTION_EAST,
    DIRECTION_SOUTHEAST,
    DIRECTION_SOUTH,
    DIRECTION_SOUTHWEST,
    DIRECTION_WEST,
    DIRECTION_NORTHWEST,
    DIRECTION_COUNT
};

const ivec2 DIRECTION_IVEC2[8] = {
    ivec2(0, -1), // North
    ivec2(1, -1), // Northeast
    ivec2(1, 0), // East
    ivec2(1, 1), // Southeast
    ivec2(0, 1), // South
    ivec2(-1, 1), // Southwest
    ivec2(-1, 0), // West
    ivec2(-1, -1), // Northwest
};

const fvec2 DIRECTION_FVEC2[8] = {
    fvec2(fixed::from_int(0), fixed::from_int(-1)), // North
    fvec2(fixed::from_raw(181), fixed::from_raw(-181)), // Northeast
    fvec2(fixed::from_int(1), fixed::from_int(0)), // East
    fvec2(fixed::from_raw(181), fixed::from_raw(181)), // Southeast
    fvec2(fixed::from_int(0), fixed::from_int(1)), // South
    fvec2(fixed::from_raw(-181), fixed::from_raw(181)), // Southwest
    fvec2(fixed::from_int(-1), fixed::from_int(0)), // West
    fvec2(fixed::from_raw(-181), fixed::from_raw(-181)) // Northwest
};

const uint8_t DIRECTION_MASK[DIRECTION_COUNT] = {
    1,
    2,
    4,
    8,
    16,
    32,
    64, 
    128
};

const ivec2 AUTOTILE_EDGE_OFFSETS[4] = { ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1) };
const uint8_t AUTOTILE_EDGE_MASK[4] = { 
    (uint8_t)(DIRECTION_MASK[DIRECTION_NORTH] + DIRECTION_MASK[DIRECTION_NORTHWEST] + DIRECTION_MASK[DIRECTION_WEST]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_NORTH] + DIRECTION_MASK[DIRECTION_NORTHEAST] + DIRECTION_MASK[DIRECTION_EAST]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_WEST] + DIRECTION_MASK[DIRECTION_SOUTHWEST] + DIRECTION_MASK[DIRECTION_SOUTH]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_EAST] + DIRECTION_MASK[DIRECTION_SOUTHEAST] + DIRECTION_MASK[DIRECTION_SOUTH])
};

inline ivec2 autotile_edge_lookup(uint32_t edge, uint8_t neighbors) {
    switch (edge) {
        case 0:
            switch (neighbors) {
                case 0: return ivec2(0, 0);
                case 128: return ivec2(0, 0);
                case 1: return ivec2(0, 3);
                case 129: return ivec2(0, 3);
                case 64: return ivec2(1, 2);
                case 192: return ivec2(1, 2);
                case 65: return ivec2(2, 0);
                case 193: return ivec2(1, 3);
                default: return ivec2(-1, -1);
            }
        case 1:
            switch (neighbors) {
                case 0: return ivec2(1, 0);
                case 2: return ivec2(1, 0);
                case 1: return ivec2(3, 3);
                case 3: return ivec2(3, 3);
                case 4: return ivec2(1, 2);
                case 6: return ivec2(1, 2);
                case 5: return ivec2(3, 0);
                case 7: return ivec2(1, 3);
                default: return ivec2(-1, -1);
            }
        case 2:
            switch (neighbors) {
                case 0: return ivec2(0, 1);
                case 32: return ivec2(0, 1);
                case 16: return ivec2(0, 3);
                case 48: return ivec2(0, 3);
                case 64: return ivec2(1, 5);
                case 96: return ivec2(1, 5);
                case 80: return ivec2(2, 1);
                case 112: return ivec2(1, 3);
                default: return ivec2(-1, -1);
            }
        case 3:
            switch (neighbors) {
                case 0: return ivec2(1, 1);
                case 8: return ivec2(1, 1);
                case 4: return ivec2(1, 5);
                case 12: return ivec2(1, 5);
                case 16: return ivec2(3, 3);
                case 24: return ivec2(3, 3);
                case 20: return ivec2(3, 1);
                case 28: return ivec2(1, 3);
                default: return ivec2(-1, -1);
            }
        default: return ivec2(-1, -1);
    }
}

inline direction enum_from_ivec2_direction(ivec2 ivec2_direction) {
    for (int enum_direction = 0; enum_direction < DIRECTION_COUNT; enum_direction++) {
        if (DIRECTION_IVEC2[enum_direction] == ivec2_direction) {
            return (direction)enum_direction;
        }
    }
    return DIRECTION_COUNT;
}

// Rect

struct rect_t {
    int x;
    int y;
    int w;
    int h;
};

inline direction enum_direction_to_rect(ivec2 from, rect_t rect) {
    if (from.y < rect.y) {
        if (from.x < rect.x) {
            return DIRECTION_SOUTHEAST;
        } else if (from.x >= rect.x + rect.w) {
            return DIRECTION_SOUTHWEST;
        } else {
            return DIRECTION_SOUTH;
        }
    } else if (from.y >= rect.y + rect.h) {
        if (from.x < rect.x) {
            return DIRECTION_NORTHEAST;
        } else if (from.x >= rect.x + rect.w) {
            return DIRECTION_NORTHWEST;
        } else {
            return DIRECTION_NORTH;
        }
    } else if (from.x < rect.x) {
        return DIRECTION_EAST;
    } else {
        return DIRECTION_WEST;
    }
}

inline fvec2 cell_center(ivec2 cell) {
    return fvec2(
        fixed::from_int((cell.x * TILE_SIZE) + (TILE_SIZE / 2)),
        fixed::from_int((cell.y * TILE_SIZE) + (TILE_SIZE / 2))
    );
}

inline bool rect_has_point(rect_t rect, ivec2 point) {
    return !(point.x < rect.x || point.x >= rect.x + rect.w || point.y < rect.y || point.y >= rect.y + rect.h);
}

inline bool sdl_rects_are_adjacent(const rect_t a, const rect_t b) {
    if (a.x + a.w == b.x || b.x + b.w == a.x) {
        return (a.y >= b.y && a.y + a.h <= b.y + b.h) ||
               (b.y >= a.y && b.y + b.h <= a.y + a.h);
    } else if (a.y + a.h == b.y || b.y + b.h == a.y) {
        return (a.x >= b.x && a.x + a.w <= b.x + b.w) ||
               (b.x >= a.x && b.x + b.h <= a.x + a.w);
    } else {
        return false;
    }
}

inline ivec2 cell_within_rect_a_nearest_to_rect_b(const rect_t a, const rect_t b) {
    ivec2 cell;
    if (b.y + b.h - 1 < a.y) {
        cell.y = a.y;
    } else if (b.y > a.y + a.h - 1) {
        cell.y = a.y + a.h - 1;
    } else if (b.y > a.y) {
        cell.y = b.y;
    } else {
        cell.y = a.y;
    }

    if (b.x + b.w - 1 < a.x) {
        cell.x = a.x;
    } else if (b.x > a.x + a.w - 1) {
        cell.x = a.x + a.w - 1;
    } else if (b.x > a.x) {
        cell.x = b.x;
    } else {
        cell.x = a.x;
    }

    return cell;
}

inline int euclidean_distance_squared_between(const rect_t a, const rect_t b) {
    ivec2 cell_in_a_nearest_to_b = cell_within_rect_a_nearest_to_rect_b(a, b);
    ivec2 cell_in_b_nearest_to_a = cell_within_rect_a_nearest_to_rect_b(b, a);
    return ivec2::euclidean_distance_squared(cell_in_a_nearest_to_b, cell_in_b_nearest_to_a);
}

inline ivec2 get_nearest_cell_in_rect(ivec2 start_cell, rect_t rect) {
    ivec2 nearest_cell = ivec2(rect.x, rect.y);
    uint32_t nearest_cell_dist = ivec2::manhattan_distance(start_cell, nearest_cell);

    for (int y = rect.y; y < rect.y + rect.h; y++) {
        for (int x = rect.x; x < rect.x + rect.w; x++) {
            uint32_t cell_dist = ivec2::manhattan_distance(start_cell, ivec2(x, y));
            if (cell_dist < nearest_cell_dist) {
                nearest_cell = ivec2(x, y);
                nearest_cell_dist = cell_dist;
            }
        }
    }

    return nearest_cell;
}

inline int next_largest_power_of_two(int number) {
    int power_of_two = 1;
    while (power_of_two < number) {
        power_of_two *= 2;
    }
    
    return power_of_two;
}