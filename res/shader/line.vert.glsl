#version 330 core

layout (location = 0) in vec2 in_position;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(in_position.xy, 0.0, 1.0);
}