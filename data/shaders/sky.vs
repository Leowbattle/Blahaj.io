#version 330 core

// https://learnopengl.com/Advanced-OpenGL/Cubemaps

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    TexCoords.y *= -1;
    gl_Position = projection * view * vec4(aPos, 1.0);
} 