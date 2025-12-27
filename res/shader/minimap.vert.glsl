#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

uniform mat4 projection;
uniform vec2 screen_scale;

out vec2 frag_tex_coord;

void main() {
    vec4 projected_position = projection * vec4(in_position.xy, 0.0, 1.0);
    gl_Position = vec4(projected_position.x * screen_scale.x, projected_position.y * screen_scale.y, 0.0, 1.0);
    frag_tex_coord = in_tex_coord;
}