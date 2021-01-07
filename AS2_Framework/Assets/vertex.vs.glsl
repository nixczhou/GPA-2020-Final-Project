#version 410

layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;
layout(location = 3) in vec3 iv3tangent;

uniform mat4 um4mv;
uniform mat4 um4p;
uniform mat4 lightSpaceMatrix;

out VertexData
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

out vec4 viewSpace_coord;

void main()
{
	mat3 normalMatrix = transpose(inverse(mat3(um4mv)));
	vec4 P = um4mv * vec4(iv3vertex, 1.0);
	vec3 T = normalize(normalMatrix * iv3tangent);
	vec3 N = normalize(normalMatrix * iv3normal);
	vec3 B = cross(N, T);
	T = normalize(T - dot(T, N) * N);
	mat3 TBN = transpose(mat3(T, B, N));

	gl_Position = um4p * um4mv * vec4(iv3vertex, 1.0);
	vertexData.texcoord = iv2tex_coord;

	viewSpace_coord = um4mv * vec4(iv3vertex, 1.0);
	vertexData.fragPosLightSpace = lightSpaceMatrix * vec4(iv3vertex, 1.0);
 
	vertexData.N = TBN * N;
	vertexData.L = TBN * (um4mv * vec4(0, 1, -1, 0)).xyz;
    vertexData.V  = TBN * (-P.xyz);

    vertexData.TangentFragPos  = TBN * vec3(um4mv * vec4(iv3vertex, 0.0));
	
	vertexData.viewN = N;
}