#pragma once

#include "defines.h"
#include <cmath>
#include <SDL2/SDL.h>

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

struct xy {
    int x;
    int y;

    xy() = default;
    xy(int x, int y) : x(x), y(y) {}
    bool operator==(const xy& other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(const xy& other) const {
        return this->x != other.x || this->y != other.y;
    }
    xy operator-() const {
        return xy(-x, -y);
    }
    xy operator+(const xy& other) const {
        return xy(x + other.x, y + other.y);
    }
    xy& operator+=(const xy& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    xy operator-(const xy& other) const {
        return xy(x - other.x, y - other.y);
    }
    xy& operator-=(const xy& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    xy operator*(int scaler) const {
        return xy(x * scaler, y * scaler);
    }
    xy operator/(int scaler) const {
        return xy(x / scaler, y / scaler);
    }
    static int manhattan_distance(const xy& a, const xy& b) {
        return abs(a.x - b.x) + abs(a.y - b.y);
    }
    static int euclidean_distance_squared(const xy& a, const xy& b) {
        return (abs(a.x - b.x) * abs(a.x - b.x)) + (abs(a.y - b.y) * abs(a.y - b.y));
    }
};

struct xy_fixed {
    fixed x;
    fixed y;

    xy_fixed() = default;
    xy_fixed(fixed x, fixed y) : x(x), y(y) {}
    xy_fixed(const xy& other) : x(fixed::from_int(other.x)), y(fixed::from_int(other.y)) {}
    xy to_xy() const {
        return xy(x.integer_part(), y.integer_part());
    }
    bool operator==(const xy_fixed& other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(const xy_fixed& other) const {
        return this->x != other.x || this->y != other.y;
    }
    xy_fixed operator-() const {
        return xy_fixed(-x, -y);
    }
    xy_fixed operator+(const xy_fixed& other) const {
        return xy_fixed(x + other.x, y + other.y);
    }
    xy_fixed& operator+=(const xy_fixed& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    xy_fixed operator-(const xy_fixed& other) const {
        return xy_fixed(x - other.x, y - other.y);
    }
    xy_fixed& operator-=(const xy_fixed& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    xy_fixed operator*(const fixed& scaler) const {
        return xy_fixed(x * scaler, y * scaler);
    }
    xy_fixed operator/(const fixed& scaler) const {
        return xy_fixed(x / scaler, y / scaler);
    }
    fixed length() const {
        int64_t x64 = x.raw_value;
        int64_t y64 = y.raw_value;
        uint64_t length_squared = (x64 * x64) + (y64 * y64);
        return fixed::from_raw((int32_t)sqrt_i64(length_squared));
    }
    fixed distance_to(const xy_fixed& other) const {
        return (other - *this).length();
    }
};

inline bool sdl_rect_has_point(SDL_Rect rect, xy point) {
    return !(point.x < rect.x || point.x >= rect.x + rect.w || point.y < rect.y || point.y >= rect.y + rect.h);
}

enum Direction {
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

const xy DIRECTION_XY[8] = {
    xy(0, -1), // North
    xy(1, -1), // Northeast
    xy(1, 0), // East
    xy(1, 1), // Southeast
    xy(0, 1), // South
    xy(-1, 1), // Southwest
    xy(-1, 0), // West
    xy(-1, -1), // Northwest
};

const xy_fixed DIRECTION_XY_FIXED[8] = {
    xy_fixed(fixed::from_int(0), fixed::from_int(-1)), // North
    xy_fixed(fixed::from_raw(181), fixed::from_raw(-181)), // Northeast
    xy_fixed(fixed::from_int(1), fixed::from_int(0)), // East
    xy_fixed(fixed::from_raw(181), fixed::from_raw(181)), // Southeast
    xy_fixed(fixed::from_int(0), fixed::from_int(1)), // South
    xy_fixed(fixed::from_raw(-181), fixed::from_raw(181)), // Southwest
    xy_fixed(fixed::from_int(-1), fixed::from_int(0)), // West
    xy_fixed(fixed::from_raw(-181), fixed::from_raw(-181)) // Northwest
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

const xy AUTOTILE_EDGE_OFFSETS[4] = { xy(0, 0), xy(1, 0), xy(0, 1), xy(1, 1) };
const uint8_t AUTOTILE_EDGE_MASK[4] = { 
    (uint8_t)(DIRECTION_MASK[DIRECTION_NORTH] + DIRECTION_MASK[DIRECTION_NORTHWEST] + DIRECTION_MASK[DIRECTION_WEST]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_NORTH] + DIRECTION_MASK[DIRECTION_NORTHEAST] + DIRECTION_MASK[DIRECTION_EAST]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_WEST] + DIRECTION_MASK[DIRECTION_SOUTHWEST] + DIRECTION_MASK[DIRECTION_SOUTH]),
    (uint8_t)(DIRECTION_MASK[DIRECTION_EAST] + DIRECTION_MASK[DIRECTION_SOUTHEAST] + DIRECTION_MASK[DIRECTION_SOUTH])
};

inline Direction enum_from_xy_direction(xy xy_direction) {
    for (int enum_direction = 0; enum_direction < DIRECTION_COUNT; enum_direction++) {
        if (DIRECTION_XY[enum_direction] == xy_direction) {
            return (Direction)enum_direction;
        }
    }
    return DIRECTION_COUNT;
}

inline Direction enum_direction_to_rect(xy from, xy rect, int rect_size) {
    if (from.y < rect.y) {
        if (from.x < rect.x) {
            return DIRECTION_SOUTHEAST;
        } else if (from.x >= rect.x + rect_size) {
            return DIRECTION_SOUTHWEST;
        } else {
            return DIRECTION_SOUTH;
        }
    } else if (from.y >= rect.y + rect_size) {
        if (from.x < rect.x) {
            return DIRECTION_NORTHEAST;
        } else if (from.x >= rect.x + rect_size) {
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

inline xy_fixed cell_center(xy cell) {
    return xy_fixed(
        fixed::from_int((cell.x * TILE_SIZE) + (TILE_SIZE / 2)),
        fixed::from_int((cell.y * TILE_SIZE) + (TILE_SIZE / 2))
    );
}

inline bool sdl_rects_are_adjacent(const SDL_Rect& a, const SDL_Rect& b) {
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

inline xy cell_within_rect_a_nearest_to_rect_b(const SDL_Rect& a, const SDL_Rect& b) {
    xy cell;
    if (a.y > b.y) {
        cell.y = a.y;
    } else if (a.y + a.h <= b.y) {
        cell.y = a.y + a.h - 1;
    } else {
        cell.y = a.y;
    }

    if (a.x > b.x) {
        cell.x = a.x;
    } else if (a.x + a.w <= b.x) {
        cell.x = a.x + a.w - 1;
    } else {
        cell.x = b.x;
    }
    
    return cell;
}

inline int euclidean_distance_squared_between(const SDL_Rect& a, const SDL_Rect& b) {
    xy cell_in_a_nearest_to_b = cell_within_rect_a_nearest_to_rect_b(a, b);
    xy cell_in_b_nearest_to_a = cell_within_rect_a_nearest_to_rect_b(b, a);
    return xy::euclidean_distance_squared(cell_in_a_nearest_to_b, cell_in_b_nearest_to_a);
}