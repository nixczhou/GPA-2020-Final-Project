# version 410 core

out vec4 color;
uniform sampler2D tex_color;
uniform sampler2D tex_normal;
uniform bool enable_fog = true;
uniform vec4 fog_color = vec4(0.933, 0.910, 0.667, 0.0);

in TES_OUT
{
	vec2 tc;
	vec3 world_coord;
	vec3 eye_coord;
} fs_in;

vec4 fog(vec4 c) 
{
	float z = length(fs_in.eye_coord);
	float de = 0.025 * smoothstep(0.0, 6.0, 10.0 - fs_in.world_coord.y);
	float di = 0.045 * (smoothstep(0.0, 40.0, 20.0 - fs_in.world_coord.y));
	float extinction   = exp(-z * de);
	float inscattering = exp(-z * di);
	return c * extinction + fog_color * (1.0 - inscattering);
}

void main(void)
{
	vec4 landscape = texture(tex_color, fs_in.tc);
	vec4 normal_landscape = texture(tex_normal, fs_in.tc);
	if (enable_fog)
	{
		color = fog(landscape);
	}
	else
	{
		color = landscape;
	}
}