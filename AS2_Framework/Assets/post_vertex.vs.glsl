#version 410
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

out VS_OUT
{
	vec2 texcoord;

} vs_out;

void main(void)
{
	gl_Position = vec4(position, 1.0, 1.0);
	vs_out.texcoord = texcoord;
}