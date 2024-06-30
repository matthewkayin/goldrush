#pragma once

#include <cstdint>
#include <cstddef>

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

    static constexpr fixed from_raw(int32_t raw_value) {
        return (fixed) { .raw_value = raw_value };
    }

    static fixed from_int(int32_t integer_value) {
        return (fixed) { .raw_value = integer_value << fractional_bits };
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

    fixed floor() const {
        return from_int(integer_part());
    }
    fixed ceil() const {
        return from_int(integer_part() + 1);
    }
    fixed abs() const {
        return *this >= from_int(0) ? *this : from_raw(-raw_value);
    }

    static fixed sqrt(const fixed& value) {
        return from_raw((int32_t)sqrt_i64(((int64_t)value.raw_value) << fractional_bits));
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
};

struct vec2 {
    fixed x;
    fixed y;

    vec2() = default;
    vec2(fixed x, fixed y) : x(x), y(y) {}
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
        uint64_t x64 = (uint64_t)(x.raw_value < 0 ? -x.integer_part() : x.integer_part());
        uint64_t y64 = (uint64_t)(y.raw_value < 0 ? -y.integer_part() : y.integer_part());
        uint64_t length_squared = (x64 * x64) + (y64 * y64);
        return fixed::from_raw((int32_t)sqrt_i64(length_squared) << fixed::fractional_bits);
    }
    vec2 normalized() const {
        fixed _length = length();
        if (_length.raw_value == 0) {
            return vec2(fixed::from_raw(0), fixed::from_raw(0));
        }
        return vec2(x / _length, y / _length);
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