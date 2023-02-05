#version 330 core

in vec2 uv;
in vec3 normal;

layout(location = 0) out vec4 fragColour;

uniform sampler2D u_tex;

void main() {
    vec3 lightDir = normalize(vec3(0, 1, -1));

    float ambient = 0.4;
    float diff = max(dot(normalize(normal), lightDir), 0.0);

    vec4 objectColor = texture(u_tex, uv);
    vec3 result = (ambient + diff) * objectColor.xyz;
    fragColour = vec4(result, 1.);
}
