#version 430 core

struct WaterColumn
{
	float height;
	float flow;
};

layout(std430, binding = 0) buffer WaterGrid
{
	WaterColumn columns[];
};

layout(location = 0) uniform mat4 mvp;
layout(location = 1) uniform vec3 eye;

out vec3 normal;
out vec3 view;

#define ROW_SIZE 180
const uint offset[4] = uint[4](0, 1, ROW_SIZE, ROW_SIZE + 1);
const vec2 poffset[4] = vec2[4](vec2(0, 0),
								vec2(1, 0),
								vec2(0, 1),
								vec2(1, 1));

void main(void)
{
	uvec2 coord = uvec2(mod(gl_InstanceID, ROW_SIZE - 1), uint(gl_InstanceID / (ROW_SIZE - 1)));
	uint idx = coord.y * ROW_SIZE + coord.x;
	idx = idx + offset[gl_VertexID];
	vec3 pos = vec3(
	float(coord.x) - float(ROW_SIZE / 2) + poffset[gl_VertexID].x,
	columns[idx].height,
	float(coord.y) - float(ROW_SIZE / 2) + poffset[gl_VertexID].y);
	gl_Position = mvp * vec4(pos, 1.0);
	vec3 dx = vec3(1, 0, 0);
	vec3 dy = vec3(0, 0, 1);
	if(coord.x < ROW_SIZE - 2)
	{
		dx = vec3(1, columns[idx + 1].height - columns[idx].height, 0);
	}

	if(coord.y < ROW_SIZE - 2)
	{
		dy = vec3(0, columns[idx + ROW_SIZE].height - columns[idx].height, 1);
	}

	normal = normalize(cross(dy, dx));
	view = eye - pos;
}
