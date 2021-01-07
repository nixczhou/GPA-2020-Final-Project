#version 420 core

uniform int mode;
uniform float bar_position = 0.5f;
uniform float width;
uniform float height;
uniform float time;
uniform float mouse_x;
uniform float mouse_y;
uniform float magnify;
float PI = 3.14159265;
layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D noiseMap;

out vec4 color;

in VS_OUT
{
  vec2 texcoord;

} fs_in;

vec4 Quantization()
{
	float nbins = 8.0;
	vec3 texture_color = texture2D(tex, fs_in.texcoord).rgb;
	texture_color = floor(texture_color * nbins) / nbins;
	return vec4(texture_color, 1);
}

vec4 DoG(void)
{
	float sigma_e = 2.0f;
	float sigma_r = 2.8f;
	float phi = 3.4f;
	float tau = 0.99f;
	float twoSigmaESquared = 2.0 * sigma_e * sigma_e;
	float twoSigmaRSquared = 2.0 * sigma_r * sigma_r;
	int halfWidth = int(ceil(2.0 * sigma_r));
	vec2 img_size = vec2(width, height);

	vec2 sum = vec2(0);
	vec2 norm = vec2(0);
	int kernel_count = 0;
	for (int i = -halfWidth; i <= halfWidth; i++) {
		for (int j = -halfWidth; j <= halfWidth; j++) {
			float d = length(vec2(i, j));
			vec2 kernel = vec2(exp(-d * d / twoSigmaESquared),
				exp(-d * d / twoSigmaRSquared));
			vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / img_size);
			vec2 L = vec2(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);
			norm += 2.0 * kernel;
			sum += kernel * L;
		}
	}
	sum /= norm;
	float H = 100.0 * (sum.x - tau * sum.y);
	float edge = (H > 0.0) ? 1.0 : 2.0 * smoothstep(-2.0, 2.0, phi * H);
	return vec4(edge, edge, edge, 1.0);
}

vec4 blur_water(vec2 uv)
{
	// Texture coordinates are between [0 1], so we map the pixel indices to the [0 1] space
	int blurSize = 1;
	float dx = 1.0f / width;
	float dy = 1.0f / height;

	// To make the blur effect, we average the colors of surrounding pixels
	vec4 sum = vec4(0, 0, 0, 1);
	for (int i = -blurSize; i < blurSize; i++)
		for (int j = -blurSize; j < blurSize; j++)
			sum.rgb += texture(tex, uv + vec2(i * dx, j * dy)).rgb;

	return sum / (4 * blurSize * blurSize);
}

vec4 blur(vec2 uv)
{
	// Texture coordinates are between [0 1], so we map the pixel indices to the [0 1] space
	int blurSize = 5;
	float dx = 1.0f / width;
	float dy = 1.0f / height;

	// To make the blur effect, we average the colors of surrounding pixels
	vec4 sum = vec4(0, 0, 0, 1);
	for (int i = -blurSize; i < blurSize; i++)
		for (int j = -blurSize; j < blurSize; j++)
			sum.rgb += texture(tex, uv + vec2(i * dx, j * dy)).rgb;

	return sum / (4 * blurSize * blurSize);
}

vec4 blur2(vec3 rgb)
{
	// Texture coordinates are between [0 1], so we map the pixel indices to the [0 1] space
	int blurSize = 5;
	float dx = 1.0f / width;
	float dy = 1.0f / height;

	// To make the blur effect, we average the colors of surrounding pixels
	vec4 sum = vec4(0, 0, 0, 1);
	for (int i = -blurSize; i < blurSize; i++)
		for (int j = -blurSize; j < blurSize; j++)
			sum.rgb += rgb;

	return sum / (4 * blurSize * blurSize);
}


void main(void) {
	//Drawing After Filtering Scene in the Right Side
	if (fs_in.texcoord.x > bar_position + 0.002)
	{
		if (mode == 0) { // image  abstraction

			color = blur(fs_in.texcoord) * Quantization() * DoG();
		}
		else if (mode == 1) { // water color

			float dx = 1.0f / width;
			float dy = 1.0f / height;
			vec2 strokeDir = vec2(0.5, 0.5);

			vec2 uv = fs_in.texcoord + (1 - texture(noiseMap, fs_in.texcoord).r) * vec2(strokeDir.x * dx, strokeDir.y * dy) * 10;
			vec4 distorted_uv = blur_water(uv);

			//Quantization
			int nbins = 8;
			color = vec4(floor(distorted_uv.r * float(nbins)) / float(nbins), floor(distorted_uv.g * float(nbins)) / float(nbins), floor(distorted_uv.b * float(nbins)) / float(nbins), 1.0);

		}
		else if (mode == 2) { // Magnifier

			if (magnify == 1) {
				float R = 200.0;
				float h = 40.0;
				float hr = R * sqrt(1.0 - ((R - h) / R) * ((R - h) / R));

				vec2 xy = gl_FragCoord.xy - vec2(mouse_x, height - mouse_y);
				float r = sqrt(xy.x * xy.x + xy.y * xy.y);

				vec2 new_xy;
				if (r < hr) new_xy = xy * 0.5;
				else new_xy = xy;

				if (r < hr + 3 && r>hr)
					color = vec4(0.0, 0.0, 0.0, 1.0);
				else
					color = texture(tex, (new_xy + vec2(mouse_x, height - mouse_y)) / vec2(width, height));
			}
			else {
				color = texture(tex, fs_in.texcoord);
			}

		}
		else if (mode == 3) { //bloom effect
			vec4 blur = blur2(blur(fs_in.texcoord).rgb);
			color = texture(tex, fs_in.texcoord) * 0.8 + blur + 1.2 / 256;

		}
		else if (mode == 4) { // pixelation
			int size = 10;

			//  we map the pixel indices to the [0 1] space and initialize baseUV and uvSize
			vec2 baseUV = vec2(0, 0);
			vec2 uvSize = vec2((1.0f / width) * size, (1.0f / width) * size);

			// Find the base UV. The base UV is the UV on the left-upper corner of the block
			while (fs_in.texcoord.y >= baseUV.y) {
				baseUV.y += uvSize.y;
			}
			while (fs_in.texcoord.x >= baseUV.x) {
				baseUV.x += uvSize.x;
			}
			baseUV -= uvSize;

			// Calcuate the average color for the pixels in each block
			vec3 sum = vec3(0, 0, 0);
			for (int i = 0; i < size; i++)
				for (int j = 0; j < size; j++)
					sum += texture(tex, baseUV + vec2(i * (1.0f / width), j * (1.0f / width))).rgb;

			color = vec4(sum / (size * size), 1.0);

		}
		else if (mode == 5) { // Sin Wave 

			vec2 uv = fs_in.texcoord;
			uv.x += 0.05 * (sin(uv.y * 2 * PI + time));
			color = clamp(texture(tex, uv), 0.0, 1.0);

		}
	}

	//Drawing Before Scene in the Left Side
	else if (fs_in.texcoord.x < bar_position - 0.002)
	{
		color = texture(tex, fs_in.texcoord);
	}

	//To set the color of the comparison bar 
	else
	{
		color = vec4(0.0, 1.0, 0.0, 1.0);
	}
}
