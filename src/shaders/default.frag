
#version 330 core

out vec4 frag_color;
in vec3 p;

void main() {

    float a;
    if (abs(p.y * 10.0 - sin(p.x * 10.0)) < 0.2) {
        a = 1.0;
    } else {
        a = 0.0;
    }

    frag_color = vec4(a, a, a, 1.0);

}
