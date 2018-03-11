
/* Based on Separable SSS. by Jorge Jimenez and Diego Gutierrez */

#define MAX_SSS_SAMPLES 65
layout(std140) uniform sssProfile {
	vec4 kernel[MAX_SSS_SAMPLES];
	vec4 radii_max_radius;
	int sss_samples;
};

uniform sampler2D depthBuffer;
uniform sampler2D sssData;
uniform sampler2D sssAlbedo;

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)
#endif /* UTIL_TEX */

layout(location = 0) out vec4 FragColor;
#ifdef RESULT_ACCUM
layout(location = 1) out vec4 sssColor;
#endif

float get_view_z_from_depth(float depth)
{
	if (ProjectionMatrix[3][3] == 0.0) {
		float d = 2.0 * depth - 1.0;
		return -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
	}
	else {
		return viewVecs[0].z + depth * viewVecs[1].z;
	}
}

#define LUT_SIZE 64
#define M_PI_2     1.5707963267948966        /* pi/2 */
#define M_2PI      6.2831853071795865        /* 2*pi */

void main(void)
{
	vec2 pixel_size = 1.0 / vec2(textureSize(depthBuffer, 0).xy); /* TODO precompute */
	vec2 uvs = gl_FragCoord.xy * pixel_size;
	vec4 sss_data = texture(sssData, uvs).rgba;
	float depth_view = get_view_z_from_depth(texture(depthBuffer, uvs).r);

	float rand = texelfetch_noise_tex(gl_FragCoord.xy).r;
#ifdef FIRST_PASS
	float angle = M_2PI * rand + M_PI_2;
	vec2 dir = vec2(1.0, 0.0);
#else /* SECOND_PASS */
	float angle = M_2PI * rand;
	vec2 dir = vec2(0.0, 1.0);
#endif
	vec2 dir_rand = vec2(cos(angle), sin(angle));

	/* Compute kernel bounds in 2D. */
	float homcoord = ProjectionMatrix[2][3] * depth_view + ProjectionMatrix[3][3];
	vec2 scale = vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) * sss_data.aa / homcoord;
	vec2 finalStep = scale * radii_max_radius.w;
	finalStep *= 0.5; /* samples range -1..1 */

	/* Center sample */
	vec3 accum = sss_data.rgb * kernel[0].rgb;

	for (int i = 1; i < sss_samples && i < MAX_SSS_SAMPLES; i++) {
		vec2 sample_uv = uvs + kernel[i].a * finalStep * ((abs(kernel[i].a) > sssJitterThreshold) ? dir : dir_rand);
		vec3 color = texture(sssData, sample_uv).rgb;
		float sample_depth = texture(depthBuffer, sample_uv).r;
		sample_depth = get_view_z_from_depth(sample_depth);

		/* Depth correction factor. */
		float depth_delta = depth_view - sample_depth;
		float s = clamp(1.0 - exp(-(depth_delta * depth_delta) / (2.0 * sss_data.a)), 0.0, 1.0);

		/* Out of view samples. */
		if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
			s = 1.0;
		}

		accum += kernel[i].rgb * mix(color, sss_data.rgb, s);
	}

#ifdef FIRST_PASS
	FragColor = vec4(accum, sss_data.a);
#else /* SECOND_PASS */
# ifdef USE_SEP_ALBEDO
#  ifdef RESULT_ACCUM
	FragColor = vec4(accum, 1.0);
	sssColor = texture(sssAlbedo, uvs);
#  else
	FragColor = vec4(accum * texture(sssAlbedo, uvs).rgb, 1.0);
#  endif
# else
	FragColor = vec4(accum, 1.0);
# endif
#endif
}
