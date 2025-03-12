#version 330 core

in vec3 frag_tex_coord;

out vec4 frag_color;

uniform sampler2D sprite_texture;

void main() {
    frag_color = texture(sprite_texture, frag_tex_coord.xy);
}