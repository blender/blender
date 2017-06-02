
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	vec4 lampPosition;
	int layer;
	float exponent;
};

uniform samplerCube shadowCube;

out vec4 FragColor;

vec3 octahedral_to_cubemap_proj(vec2 co)
{
	co = co * 2.0 - 1.0;

	vec2 abs_co = abs(co);
	vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

	if ( abs_co.x + abs_co.y > 1.0 ) {
		v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
	}

	return v;
}

void make_orthonormal_basis(vec3 N, out vec3 T, out vec3 B)
{
	vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
	T = normalize( cross(UpVector, N) );
	B = cross(N, T);
}

#define NUM_SAMPLE 32
vec2 poisson_disc[32] = vec2[32](
	vec2( 0.476,  0.854), vec2(-0.659, -0.670),
	vec2( 0.905, -0.270), vec2( 0.215, -0.133),
	vec2(-0.595,  0.242), vec2(-0.146,  0.519),
	vec2( 0.108, -0.930), vec2( 0.807,  0.449),

	vec2(-0.476, -0.854), vec2( 0.659,  0.670),
	vec2(-0.905,  0.270), vec2(-0.215,  0.133),
	vec2( 0.595, -0.242), vec2( 0.146, -0.519),
	vec2(-0.108,  0.930), vec2(-0.807, -0.449),

	vec2(-0.854,  0.476), vec2( 0.670, -0.659),
	vec2( 0.270,  0.905), vec2( 0.133,  0.215),
	vec2(-0.242, -0.595), vec2(-0.519, -0.146),
	vec2( 0.930,  0.108), vec2(-0.449,  0.807),

	vec2( 0.854, -0.476), vec2(-0.670,  0.659),
	vec2(-0.270, -0.905), vec2(-0.133, -0.215),
	vec2( 0.242,  0.595), vec2( 0.519,  0.146),
	vec2(-0.930, -0.108), vec2( 0.449, -0.807)
);

/* Marco Salvi's GDC 2008 presentation about shadow maps pre-filtering techniques slide 24 */
float ln_space_prefilter(float w0, float x, float w1, float y)
{
    return x + log(w0 + w1 * exp(y - x));
}

void main() {
	const vec2 texelSize = vec2(1.0 / 512.0);

	vec2 uvs = gl_FragCoord.xy * texelSize;

	/* add a 2 pixel border to ensure filtering is correct */
	uvs.xy *= 1.0 + texelSize * 2.0;
	uvs.xy -= texelSize;

	float pattern = 1.0;

	/* edge mirroring : only mirror if directly adjacent
	 * (not diagonally adjacent) */
	vec2 m = abs(uvs - 0.5) + 0.5;
	vec2 f = floor(m);
	if (f.x - f.y != 0.0) {
		uvs.xy = 1.0 - uvs.xy;
	}

	/* clamp to [0-1] */
	uvs.xy = fract(uvs.xy);

	/* get cubemap vector */
	vec3 cubevec = octahedral_to_cubemap_proj(uvs.xy);

	vec3 T, B;
	make_orthonormal_basis(cubevec, T, B);

	/* get cubemap shadow value */
	const float blur_radius = 5.0 / 512.0; /* Totally arbitrary */
	const float weight = 1.0 / float(NUM_SAMPLE);
	float accum = 0.0;

	/* Poisson disc blur in log space. */
	vec2 offsetvec = poisson_disc[0].xy * blur_radius;
	float depth1 = texture(shadowCube, cubevec + offsetvec.x * T + offsetvec.y * B).r;

	offsetvec = poisson_disc[1].xy * blur_radius;
	float depth2 = texture(shadowCube, cubevec + offsetvec.x * T + offsetvec.y * B).r;

	accum = ln_space_prefilter(weight, depth1, weight, depth2);

	for (int i = 2; i < NUM_SAMPLE; ++i) {
		vec2 offsetvec = poisson_disc[i].xy * blur_radius;
		depth1 = texture(shadowCube, cubevec + offsetvec.x * T + offsetvec.y * B).r;
		accum = ln_space_prefilter(1.0, accum, weight, depth1);
	}

	FragColor = vec4(accum, accum, accum, 1.0);
}