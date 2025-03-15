#version 330 core

#define MAX_ATLASES 16

in vec3 frag_tex_coord;

out vec4 frag_color;

uniform sampler2D sprite_textures[MAX_ATLASES];

void main() {
    frag_color = texture(sprite_textures[int(frag_tex_coord.z)], frag_tex_coord.xy);
}