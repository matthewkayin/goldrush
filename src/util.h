#pragma once

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
        return xy_t(x - other.x, y + other.y);
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
};

using xy = xy_t<int>;
using rect = rect_t<xy>;