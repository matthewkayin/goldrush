#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_tex_coord;

uniform mat4 projection;

out vec3 frag_tex_coord;

void main() {
    gl_Position = projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
}