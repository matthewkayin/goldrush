#pragma once

#include "asserts.h"
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

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

    static fixed sqrt(const fixed& value) {
        return from_raw((int32_t)sqrt_i64(((int64_t)value.raw_value) << fractional_bits));
    }
    static int32_t roundi(const fixed& value) {
        int32_t result = value.integer_part();
        if (value.fractional_part() >= scale_factor / 2) {
            result += value.raw_value > 0 ? 1 : -1;
        }
        return result;
    }
    static fixed round(const fixed& value) {
        return fixed::from_int(roundi(value));
    }
    static fixed abs(const fixed& value) {
        return value.raw_value < 0 ? from_raw(-value.raw_value) : from_raw(value.raw_value);
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
    xy_fixed normalized() const {
        fixed _length = length();
        // fixed _length = length();
        if (_length.raw_value == 0) {
            return xy_fixed(fixed::from_raw(0), fixed::from_raw(0));
        }
        xy_fixed result = xy_fixed(x / _length, y / _length);
        return result;
    }
    xy_fixed direction_to(const xy_fixed& other) const {
        return (other - *this).normalized();
    }
    fixed distance_to(const xy_fixed& other) const {
        return (other - *this).length();
    }
};

struct rect_t {
    xy position;
    xy size;

    rect_t() = default;
    rect_t(const xy& position, const xy& size): position(position), size(size) {}
    bool has_point(const xy& point) const {
        return !(point.x < position.x || point.x >= position.x + size.x || point.y < position.y || point.y >= position.y + size.y);
    }
    bool intersects(const rect_t& other) const {
        return !(position.x > other.position.x + other.size.x ||
                 other.position.x > position.x + size.x ||
                 position.y > other.position.y + other.size.y ||
                 other.position.y > position.y + size.y);
    }
};

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
const uint8_t DIRECTION_BITMASK[8] = {
    1, // N
    2, // NE
    4, // E
    8, // SE
    16, // S
    32, // SW
    64, // W
    128 // NW
};

inline Direction get_enum_direction_from_xy_direction(xy xy_direction) {
    for (int enum_direction = 0; enum_direction < DIRECTION_COUNT; enum_direction++) {
        if (DIRECTION_XY[enum_direction] == xy_direction) {
            return (Direction)enum_direction;
        }
    }
    GOLD_ASSERT_MESSAGE(false, "Tried to get direction enum but the passed in xy value is not a valid direction.");
    return DIRECTION_COUNT;
}

inline Direction get_enum_direction_to_rect(xy from, rect_t rect) {
    if (from.y < rect.position.y) {
        if (from.x < rect.position.x) {
            return DIRECTION_SOUTHEAST;
        } else if (from.x >= rect.position.x + rect.size.x) {
            return DIRECTION_SOUTHWEST;
        } else {
            return DIRECTION_SOUTH;
        }
    } else if (from.y >= rect.position.y + rect.size.y) {
        if (from.x < rect.position.x) {
            return DIRECTION_NORTHEAST;
        } else if (from.x >= rect.position.x + rect.size.x) {
            return DIRECTION_NORTHWEST;
        } else {
            return DIRECTION_NORTH;
        }
    } else if (from.x < rect.position.x) {
        return DIRECTION_EAST;
    } else {
        return DIRECTION_WEST;
    }
}

/*
 * Quick note: xy_fixed(1,1).normalized() in the diagonal direction would result in vec2(0.707,0.707)
 * To create a fixed with value .707, we take the fractional scale 256 * .707 = 181
*/

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

inline rect_t create_bounding_rect_for_points(xy* points, uint32_t point_count) {
    xy point_min = points[0];
    xy point_max = points[0];
    for (uint32_t i = 1; i < point_count; i++) {
        point_min.x = std::min(point_min.x, points[i].x);
        point_min.y = std::min(point_min.y, points[i].y);
        point_max.x = std::max(point_max.x, points[i].x);
        point_max.y = std::max(point_max.y, points[i].y);
    }

    point_max.x++;
    point_max.y++;
    
    return rect_t(point_min, point_max - point_min);
}