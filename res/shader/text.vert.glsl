#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;
layout (location = 2) in vec3 in_color;

uniform mat4 projection;

out vec2 frag_tex_coord;
out vec3 frag_in_color;

void main() {
    gl_Position = projection * vec4(in_position.xy, 0.0, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_in_color = in_color;
}