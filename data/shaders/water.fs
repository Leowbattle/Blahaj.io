#version 330 core

in vec3 pos;
in float u;
in vec3 normal;

layout(location = 0) out vec4 fragColour;

uniform samplerCube skybox;

void main() {
    // vec3 dx = dFdx(pos);
    // vec3 dy = dFdy(pos);
    // vec3 normal = normalize(cross(dx, dy));

    vec3 lightDir = normalize(vec3(0, 1, 0));

    float ambient = 0.4;
    float diff = max(dot(normalize(normal), lightDir), 0.0);

    vec4 objectColor = vec4(0., 0., 1., 1.);
    vec3 result = (ambient + diff) * objectColor.xyz;
    fragColour = vec4(result, 0.5);

    // fragColour = texture(skybox, reflect(lightDir, normal));

    // fragColour = vec4(normal, 0.5);
}
