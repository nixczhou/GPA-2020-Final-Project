#version 410 core

out VS_OUT
{
	vec2 texcoord;
} vs_out;

void main()
{
	vec4 positions[4] = vec4[](
		 vec4(-1.0, -1.0, 0.0, 1.0),
		 vec4( 1.0, -1.0, 0.0, 1.0),
		 vec4(-1.0,  1.0, 0.0, 1.0),
		 vec4( 1.0,  1.0, 0.0, 1.0));
	gl_Position = positions[gl_VertexID];
	vs_out.texcoord = positions[gl_VertexID].xy * 0.5 + 0.5;
}