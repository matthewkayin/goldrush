#version 330 core

in vec3 frag_tex_coord;

out vec4 frag_color;

uniform sampler2DArray sprite_texture_array;

void main() {
    frag_color = texture(sprite_texture_array, frag_tex_coord);
}