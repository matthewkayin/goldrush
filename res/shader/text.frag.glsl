#version 330 core

in vec2 frag_tex_coord;
in vec3 frag_in_color;

out vec4 frag_color;

uniform sampler2D font_texture;

void main() {
    frag_color = vec4(frag_in_color, texture(font_texture, frag_tex_coord).r);
}