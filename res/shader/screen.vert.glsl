#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

out vec2 frag_tex_coord;

uniform ivec2 screen_size;
uniform ivec2 window_size;

void main() {
    float aspect = window_size.x / screen_size.x;
    float scaled_y = screen_size.y * aspect;
    float border_size = (window_size.y - scaled_y) / 2.0;
    float device_screen_height = 1.0 - (border_size / window_size.y);

    gl_Position = vec4(in_position.x, in_position.y * device_screen_height, 0.0, 1.0);
    frag_tex_coord = in_tex_coord;
}