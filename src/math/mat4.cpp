#include "mat4.h"

#include <cstring>

mat4 mat4_identity() {
    mat4 out;

    out.data[0] = 1.0f;
    out.data[5] = 1.0f;
    out.data[10] = 1.0f;
    out.data[15] = 1.0f;

    return out;
}

mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far) {
    mat4 out;
    memset(out.data, 0, sizeof(out.data));

    out.data[0] = 2.0f / (right - left);
    out.data[5] = 2.0f / (top - bottom);
    out.data[10] = -2.0f / (far - near);
    out.data[12] = -(right + left) / (right - left);
    out.data[13] = -(top + bottom) / (top - bottom);
    out.data[14] = -(far + near) / (far - near);

    out.data[15] = 1.0f;

    return out;
}