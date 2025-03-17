#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

out vec2 frag_tex_coord;

uniform vec2 screen_size;
uniform vec2 window_size;

void main() {
    float ratio = window_size.x / screen_size.x;
    float scaled_y = screen_size.y * ratio;

    gl_Position = vec4(in_position.x, in_position.y * (scaled_y / window_size.y), 0.0, 1.0);
    frag_tex_coord = in_tex_coord;
}