#pragma once

#include <cstdint>
#include <cstddef>
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

    static fixed from_int(int32_t integer_value) {
        return (fixed) { .raw_value = integer_value << fractional_bits };
    }

    static constexpr fixed from_raw(int32_t raw_value) {
        return (fixed) { .raw_value = raw_value };
    }
    fixed& operator=(int32_t value) {
        raw_value = value << fractional_bits;
        return *this;
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

struct ivec2 {
    int x;
    int y;

    ivec2() = default;
    ivec2(int x, int y) : x(x), y(y) {}
    bool operator==(const ivec2& other) const {
        return this->x == other.x && this->y == other.y;
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
};

#include "logger.h"
struct vec2 {
    fixed x;
    fixed y;

    vec2() = default;
    vec2(fixed x, fixed y) : x(x), y(y) {}
    vec2(const ivec2& other) : x(fixed::from_int(other.x)), y(fixed::from_int(other.y)) {}
    bool operator==(const vec2& other) const {
        return this->x == other.x && this->y == other.y;
    }
    vec2 operator-() const {
        return vec2(-x, -y);
    }
    vec2 operator+(const vec2& other) const {
        return vec2(x + other.x, y + other.y);
    }
    vec2& operator+=(const vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    vec2 operator-(const vec2& other) const {
        return vec2(x - other.x, y - other.y);
    }
    vec2& operator-=(const vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    vec2 operator*(const fixed& scaler) const {
        return vec2(x * scaler, y * scaler);
    }
    vec2 operator/(const fixed& scaler) const {
        return vec2(x / scaler, y / scaler);
    }
    fixed length() const {
        int64_t x64 = x.raw_value;
        int64_t y64 = y.raw_value;
        uint64_t length_squared = (x64 * x64) + (y64 * y64);
        return fixed::from_raw((int32_t)sqrt_i64(length_squared));
    }
    vec2 normalized() const {
        fixed _length = length();
        // fixed _length = length();
        if (_length.raw_value == 0) {
            return vec2(fixed::from_raw(0), fixed::from_raw(0));
        }
        vec2 result = vec2(x / _length, y / _length);
        return result;
    }
    vec2 direction_to(const vec2& other) const {
        return (other - *this).normalized();
    }
    fixed distance_to(const vec2& other) const {
        return (other - *this).length();
    }
};

struct rect_t {
    ivec2 position;
    ivec2 size;

    rect_t() = default;
    rect_t(const ivec2& position, const ivec2& size): position(position), size(size) {}
    bool has_point(const ivec2& point) const {
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

/*
 * Quick note: vec2(1,1).normalized() in the diagonal direction would result in vec2(0.707,0.707)
 * To create a fixed with value .707, we take the fractional scale 256 * .707 = 181
*/

const vec2 DIRECTION_VEC2[8] = {
    vec2(fixed::from_int(0), fixed::from_int(-1)), // North
    vec2(fixed::from_raw(181), fixed::from_raw(-181)), // Northeast
    vec2(fixed::from_int(1), fixed::from_int(0)), // East
    vec2(fixed::from_raw(181), fixed::from_raw(181)), // Southeast
    vec2(fixed::from_int(0), fixed::from_int(1)), // South
    vec2(fixed::from_raw(-181), fixed::from_raw(181)), // Southwest
    vec2(fixed::from_int(-1), fixed::from_int(0)), // West
    vec2(fixed::from_raw(-181), fixed::from_raw(-181)) // Northwest
};
