#version 330 core

in vec2 frag_tex_coord;

out vec4 frag_color;

uniform sampler2D font_texture;
uniform vec3 font_color;

void main() {
    frag_color = vec4(font_color, texture(font_texture, frag_tex_coord).r);
}