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