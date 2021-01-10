#version 410

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 viewSpacePos;

uniform mat4 um4mv;
uniform mat4 um4p;

in vec4 viewSpace_coord;

in VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
	vec3 V;
    vec3 H; // eye space halfway vector
    vec2 texcoord;
	vec4 fragPosLightSpace;

	vec3 FragPos;
    vec3 TangentFragPos;
	vec3 viewN;
} vertexData;

// diffuse mapping texture
uniform sampler2D texture_diffuse0;
uniform int lightEffect_switch;
// normal mapping texture
uniform sampler2D texture_normal0;
uniform int normalMap_switch;

// Blinn-Phong Lighting
uniform vec3 Ia = vec3(0.4, 0.4, 0.4);
uniform vec3 Id = vec3(1.0, 1.0, 1.0);
uniform vec3 Is = vec3(1.0, 1.0, 1.0);
uniform vec3 Ks = vec3(0.2, 0.2, 0.2);
uniform int shinness = 8;

// shadow mapping
uniform sampler2D depthMap;
uniform int shadowMap_switch;
float ShadowCalculation(vec4 fragPosLightSpace);

void main()
{
	vec3 N, L, V, H;

	if(normalMap_switch == 1) {
		vec3 normalMap = texture(texture_normal0, vertexData.texcoord).rgb;
		N = normalize(normalMap * 2.0 - 1.0);
	}
	else {
		N = normalize(vertexData.N);
	}
	L = normalize(vertexData.L);
	V = normalize(vertexData.V);
	H = normalize(L + V);
	
	vec3 texColor = texture(texture_diffuse0, vertexData.texcoord).rgb;
	vec3 ambient = texColor * Ia;
	vec3 diffuse = texColor * Id * max(dot(N, L), 0.0);
	vec3 specular = Ks * Is * pow(max(dot(N, H), 0.0), shinness);
	vec4 lightingColor;

	float shadow = ShadowCalculation(vertexData.fragPosLightSpace);
	if(shadowMap_switch == 1) {
		if(lightEffect_switch == 1) {
			lightingColor = vec4(ambient + (1.0 - shadow) * (diffuse + specular), 1.0);
		}
		else {
			lightingColor = (1.0 - shadow) * vec4(texColor, 1.0);
		}
	}
	else {
		if(lightEffect_switch == 1) {
			lightingColor = vec4(ambient + diffuse + specular, 1.0);
		}
		else {
			lightingColor = vec4(texColor, 1.0);
		}
	}
	fragColor = lightingColor;
	//fragColor = vec4(texture(texture_normal0, vertexData.texcoord).rgb, 1.0);
	fragNormal = normalize(vertexData.viewN);
	viewSpacePos = viewSpace_coord.xyz;
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
	float bias = max(0.05 * (1.0 - dot(vertexData.N, vertexData.L)), 0.002);
	vec2 texelSize = 1.0 / textureSize(depthMap, 0);
    float currentDepth = projCoords.z;

	float shadow = 0.0;
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float closestDepth = texture(depthMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth -bias > closestDepth  ? 1.0 : 0.0;
			}
	}
	shadow /= 9.0;
    return shadow;
}