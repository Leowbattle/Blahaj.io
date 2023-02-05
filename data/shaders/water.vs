#version 330 core

layout(location = 0) in vec2 a_xy;
layout(location = 1) in float a_u;
layout(location = 2) in vec3 a_normal;

uniform mat4 u_mat;
uniform mat4 u_view;

out vec3 pos;
out float u;
out vec3 normal;

void main() {
    vec3 ppos = vec3(a_xy.x, a_u, a_xy.y);
    gl_Position = u_mat * vec4(ppos, 1.);
    pos = ppos;
    u = a_u;
    normal = a_normal;
}
