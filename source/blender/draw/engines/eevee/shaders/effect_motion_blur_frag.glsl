
uniform sampler2D colorBuffer;
uniform sampler2D depthBuffer;


/* current frame */
uniform mat4 currInvViewProjMatrix;

/* past frame frame */
uniform mat4 pastViewProjMatrix;

in vec4 uvcoordsvar;

out vec4 FragColor;

#define MAX_SAMPLE 64

uniform int samples;

float wang_hash_noise(uint s)
{
	uint seed = (uint(gl_FragCoord.x) * 1664525u + uint(gl_FragCoord.y)) + s;

	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= 0x27d4eb2du;
	seed = seed ^ (seed >> 15u);

	float value = float(seed);
	value *= 1.0 / 4294967296.0;
	return fract(value);
}

void main()
{
	vec3 ndc_pos;
	ndc_pos.xy = uvcoordsvar.xy;
	ndc_pos.z = texture(depthBuffer, uvcoordsvar.xy).x;

	float inv_samples = 1.0 / float(samples);
	float noise = 2.0 * wang_hash_noise(0u) * inv_samples;

	/* Normalize Device Coordinates are [-1, +1]. */
	ndc_pos = ndc_pos * 2.0 - 1.0;

	vec4 p = currInvViewProjMatrix * vec4(ndc_pos, 1.0);
	vec3 world_pos = p.xyz / p.w; /* Perspective divide */

	/* Now find where was this pixel position
	 * inside the past camera viewport */
	vec4 old_ndc = pastViewProjMatrix * vec4(world_pos, 1.0);
	old_ndc.xyz /= old_ndc.w; /* Perspective divide */

	vec2 motion = (ndc_pos.xy - old_ndc.xy) * 0.25; /* 0.25 fit cycles ref */

	float inc = 2.0 * inv_samples;
	float i = -1.0 + noise;

	FragColor = vec4(0.0);
	for (int j = 0; j < samples && j < MAX_SAMPLE; j++) {
		FragColor += textureLod(colorBuffer, uvcoordsvar.xy + motion * i, 0.0) * inv_samples;
		i += inc;
	}
}
