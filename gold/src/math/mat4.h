#pragma once

struct mat4 {
    float data[16];
};

mat4 mat4_identity();
mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far);