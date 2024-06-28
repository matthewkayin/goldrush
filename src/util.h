#pragma once

#include <cstdint>

struct fp8 {
    static const size_t integer_bits = 24;
    static const size_t fractional_bits = 8;
    static const int32_t fractional_mask = ((int32_t)1 << fractional_bits) - 1;
    static const int32_t fractional_divisor = (int32_t)1 << fractional_bits;
    static const size_t total_bits = integer_bits + fractional_bits;
    int32_t raw_value;

    static constexpr fp8 from_raw(int32_t raw_value) {
        return (fp8) { .raw_value = raw_value };
    }

    static fp8 integer(int32_t integer_value) {
        return (fp8) { .raw_value = integer_value << fractional_bits };
    }

    int32_t integer_part() const {
        return raw_value >> fractional_bits;
    }
    int32_t fractional_part() const {
        return raw_value & fractional_mask;
    }
    int32_t fractional_value() const {
        return ((raw_value & fractional_mask) * 1000) / fractional_divisor;
    }

    fp8 floor() const {
        return integer(integer_part());
    }
    fp8 ceil() const {
        return integer(integer_part() + 1);
    }
    fp8 abs() const {
        return *this >= integer(0) ? *this : from_raw(-raw_value);
    }

    bool operator==(const fp8& other) const {
        return raw_value == other.raw_value;
    }
    bool operator!=(const fp8& other) const {
        return raw_value != other.raw_value;
    }
    bool operator<(const fp8& other) const {
        return raw_value < other.raw_value;
    }
    bool operator<=(const fp8& other) const {
        return raw_value <= other.raw_value;
    }
    bool operator>(const fp8& other) const {
        return raw_value > other.raw_value;
    }
    bool operator>=(const fp8& other) const {
        return raw_value >= other.raw_value;
    }

    fp8 operator-() const {
        return from_raw(-raw_value);
    }
    fp8& operator+=(const fp8& other) {
        raw_value += other.raw_value;
        return *this;
    }
    fp8 operator+(const fp8& other) const {
        return from_raw(raw_value + other.raw_value);
    }
    fp8& operator-=(const fp8& other) {
        raw_value -= other.raw_value;
        return *this;
    }
    fp8 operator-(const fp8& other) {
        return from_raw(raw_value - other.raw_value);
    }
    fp8& operator*=(const fp8& other) {
        raw_value *= other.raw_value;
        return *this;
    }
    fp8& operator*=(int scaler) {
        raw_value *= scaler;
        return *this;
    }
    fp8 operator*(const fp8& other) const {
        return from_raw(raw_value * other.raw_value);
    }
    fp8 operator*(int scaler) const {
        return from_raw(raw_value * scaler);
    }
    fp8& operator/=(const fp8& other) {
        raw_value /= other.raw_value;
        return *this;
    }
    fp8& operator/=(int scaler) {
        raw_value /= scaler;
        return *this;
    }
    fp8 operator/(const fp8& other) const {
        return from_raw(raw_value / other.raw_value);
    }
    fp8 operator/(int scaler) const {
        return from_raw(raw_value / scaler);
    }
};

template <typename utype>
struct xy_t {
    utype x;
    utype y;

    xy_t() = default;
    xy_t(utype x, utype y) : x(x), y(y) {}
    xy_t operator==(const xy_t& other) const {
        return this->x == other.x && this->y == other.y;
    }
    xy_t operator-() const {
        return xy_t(-x, -y);
    }
    xy_t operator+(const xy_t& other) const {
        return xy_t(x + other.x, y + other.y);
    }
    xy_t operator-(const xy_t& other) const {
        return xy_t(x - other.x, y - other.y);
    }
    template <typename T>
    xy_t operator*(const T other) const {
        return xy_t(x * other, y * other);
    }
    template <typename T>
    xy_t operator/(const T other) const {
        return xy_t(x / other, y / other);
    }
    xy_t& operator+=(const xy_t& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    xy_t& operator-=(const xy_t& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    template <typename T>
    xy_t& operator*=(const T& other) {
        x *= other;
        y *= other;
        return *this;
    }
    template <typename T>
    xy_t& operator/=(const T& other) {
        x /= other;
        y /= other;
        return *this;
    }
};

template <typename utype>
struct rect_t {
    utype position;
    utype size;

    rect_t() = default;
    rect_t(const utype& position, const utype& size): position(position), size(size) {}
    bool has_point(const utype& point) const {
        return !(point.x < position.x || point.x >= position.x + size.x || point.y < position.y || point.y >= position.y + size.y);
    }
    bool intersects(const rect_t& other) const {
        return !(position.x > other.position.x + other.size.x ||
                 other.position.x > position.x + size.x ||
                 position.y > other.position.y + other.size.y ||
                 other.position.y > position.y + size.y);
    }
};

using ivec2 = xy_t<int>;
using vec2 = xy_t<fp8>;

using rect = rect_t<ivec2>;