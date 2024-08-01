
#version 330 core

uniform sampler2D tex;

out vec4 frag_color;
in vec3 p;

void main() {

    frag_color = texture(tex, vec2(fract(p.x), 1.0 - fract(p.y)));

    // frag_color = vec4(a, a, a, 1.0);

}
