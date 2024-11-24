#pragma once

#include <cmath>
#include <SDL2/SDL.h>

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

inline bool sdl_rect_has_point(SDL_Rect rect, xy point) {
    return !(point.x < rect.x || point.x >= rect.x + rect.w || point.y < rect.y || point.y >= rect.y + rect.h);
}