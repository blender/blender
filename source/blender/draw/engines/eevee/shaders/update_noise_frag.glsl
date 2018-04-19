
uniform sampler2D blueNoise;
uniform vec3 offsets;

out vec4 FragColor;

#define M_2PI 6.28318530717958647692

void main(void)
{
	vec3 blue_noise = texelFetch(blueNoise, ivec2(gl_FragCoord.xy), 0).xyz;

	float noise = fract(blue_noise.y + offsets.z);
	FragColor.x = fract(blue_noise.x + offsets.x);
	FragColor.y = fract(blue_noise.z + offsets.y);
	FragColor.z = cos(noise * M_2PI);
	FragColor.w = sin(noise * M_2PI);
}
