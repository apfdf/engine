
#version 330 core

layout (location = 0) in vec3 aPos;
out vec3 p;

uniform mat4 view_mat;
uniform mat4 project_mat;

void main() {

    p = aPos;
    gl_Position = project_mat * view_mat * vec4(aPos, 1.0);

}
