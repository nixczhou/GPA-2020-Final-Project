#version 410

in vec2 vv2texcoord;

uniform sampler2D diffuseTexture;

layout(location = 0) out vec4 fragColor;

void main()
{
	vec4 color = texture(diffuseTexture, vv2texcoord);
	/*if(color.a < 0.5)
	{
		discard;
	}*/
    fragColor = color;
}