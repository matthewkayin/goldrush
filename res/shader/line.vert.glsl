#version 330 core

layout (location = 0) in vec3 in_position;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(in_position.xyz, 1.0);
}