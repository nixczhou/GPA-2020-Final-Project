#version 430 core

layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform samplerCube envMap;

in vec3 normal;
in vec3 view;

const vec3 waterColor = vec3(0.55, 0.8, 0.95);
const float ambient = 0.2;
const float shininess = 200.0;
const vec3 lightVec = vec3(1, 1, -3);

void main(void)
{
	vec3 V = normalize(view);
	vec3 N = normalize(normal);
	vec3 L = normalize(lightVec);
	vec3 H = normalize(L + V);

	vec3 diffuse = waterColor * (ambient + max(0, dot(N, L)));
	vec3 spec = vec3(1) * pow(max(0, dot(H, N)), shininess);
	vec3 env = texture(envMap, reflect(-V, N)).rgb;
	vec3 color = env * 0.5 + (diffuse + spec) * 0.5;

	fragColor = vec4(color, 1.0);
}