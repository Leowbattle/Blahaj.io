#version 330 core

// https://learnopengl.com/Advanced-OpenGL/Cubemaps

layout(location = 0) out vec4 fragColour;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{    
    fragColour = texture(skybox, TexCoords);
    // fragColour = vec4(TexCoords, 1.);
}