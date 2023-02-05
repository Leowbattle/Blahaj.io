#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;

uniform mat4 u_mat;
uniform mat4 u_view;

out vec2 uv;
out vec3 normal;

void main() {
    gl_Position = u_mat * vec4(a_pos, 1.);

    uv = a_uv;
    normal = (u_view * vec4(a_normal, 0.)).xyz;
}
