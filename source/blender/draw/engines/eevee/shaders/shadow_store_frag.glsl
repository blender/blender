
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	mat4 FaceViewMatrix[6];
	vec4 lampPosition;
	float cubeTexelSize;
	float storedTexelSize;
	float nearClip;
	float farClip;
	float shadowSampleCount;
	float shadowInvSampleCount;
};

#ifdef CSM
uniform sampler2DArray shadowTexture;
uniform int cascadeId;
#else
uniform samplerCube shadowTexture;
#endif
uniform float shadowFilterSize;

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

void make_orthonormal_basis(vec3 N, float rot, out vec3 T, out vec3 B)
{
	vec3 UpVector = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 nT = normalize(cross(UpVector, N));
	vec3 nB = cross(N, nT);

	/* Rotate tangent space */
	float angle = rot * 3.1415 * 2.0;
	vec2 dir = vec2(cos(angle), sin(angle));
	T =  dir.x * nT + dir.y * nB;
	B = -dir.y * nT + dir.x * nB;
}

float linear_depth(float z)
{
	return (nearClip  * farClip) / (z * (nearClip - farClip) + farClip);
}

#ifdef CSM
float get_cascade_world_distance(vec2 uvs)
{
	float zdepth = texture(shadowTexture, vec3(uvs, float(cascadeId))).r;
	return zdepth * abs(farClip - nearClip); /* Same factor as in shadow_cascade(). */
}
#else
float get_cube_radial_distance(vec3 cubevec)
{
	float zdepth = texture(shadowTexture, cubevec).r;
	float linear_zdepth = linear_depth(zdepth);
	cubevec = normalize(abs(cubevec));
	float cos_vec = max(cubevec.x, max(cubevec.y, cubevec.z));
	return linear_zdepth / cos_vec;
}
#endif

/* Marco Salvi's GDC 2008 presentation about shadow maps pre-filtering techniques slide 24 */
float ln_space_prefilter(float w0, float x, float w1, float y)
{
    return x + log(w0 + w1 * exp(y - x));
}

const int SAMPLE_NUM = 32;
const float INV_SAMPLE_NUM = 1.0 / float(SAMPLE_NUM);
const vec2 poisson[32] = vec2[32](
	vec2(-0.31889129888, 0.945170187163),
	vec2(0.0291070069348, 0.993645382622),
	vec2(0.453968568675, 0.882119488776),
	vec2(-0.59142811398, 0.775098624552),
	vec2(0.0672147039953, 0.677233646792),
	vec2(0.632546991242, 0.60080388224),
	vec2(-0.846282545004, 0.478266943968),
	vec2(-0.304563967348, 0.550414788876),
	vec2(0.343951542639, 0.482122717676),
	vec2(0.903371461134, 0.419225918868),
	vec2(-0.566433506581, 0.326544955645),
	vec2(-0.0174468029403, 0.345927250589),
	vec2(-0.970838848328, 0.131541221423),
	vec2(-0.317404956404, 0.102175571059),
	vec2(0.309107085158, 0.136502232088),
	vec2(0.67009683403, 0.198922062526),
	vec2(-0.62544683989, -0.0237682928336),
	vec2(0.0, 0.0),
	vec2(0.260779995092, -0.192490308513),
	vec2(0.555635503398, -0.0918935341973),
	vec2(0.989587880961, -0.03629312269),
	vec2(-0.93440130633, -0.213478602005),
	vec2(-0.615716455579, -0.335329659339),
	vec2(0.813589336772, -0.292544036149),
	vec2(-0.821106257666, -0.568279197395),
	vec2(-0.298092257627, -0.457929494012),
	vec2(0.263233114326, -0.515552889911),
	vec2(-0.0311374378304, -0.643310533036),
	vec2(0.785838482787, -0.615972502555),
	vec2(-0.444079211316, -0.836548440017),
	vec2(-0.0253421088433, -0.96112294526),
	vec2(0.350411908643, -0.89783206142)
);

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

#ifdef CSM
void main() {
	vec2 uvs = gl_FragCoord.xy * storedTexelSize;

	vec2 X, Y;
	X.x = cos(wang_hash_noise(0u) * 3.1415 * 2.0);
	X.y = sqrt(1.0 - X.x * X.x);

	Y = vec2(-X.y, X.x);

	X *= shadowFilterSize;
	Y *= shadowFilterSize;

/* TODO Can be optimized by groupping fetches
 * and by converting to world distance beforehand. */
#if defined(ESM) || defined(VSM)
#ifdef ESM
	float accum = 0.0;

	/* Poisson disc blur in log space. */
	float depth1 = get_cascade_world_distance(uvs + X * poisson[0].x + Y * poisson[0].y);
	float depth2 = get_cascade_world_distance(uvs + X * poisson[1].x + Y * poisson[1].y);
	accum = ln_space_prefilter(INV_SAMPLE_NUM, depth1, INV_SAMPLE_NUM, depth2);

	for (int i = 2; i < SAMPLE_NUM; ++i) {
		depth1 = get_cascade_world_distance(uvs + X * poisson[i].x + Y * poisson[i].y);
		accum = ln_space_prefilter(1.0, accum, INV_SAMPLE_NUM, depth1);
	}

	FragColor = vec4(accum);
#else /* VSM */
	vec2 accum = vec2(0.0);

	/* Poisson disc blur. */
	for (int i = 0; i < SAMPLE_NUM; ++i) {
		float dist = get_cascade_world_distance(uvs + X * poisson[i].x + Y * poisson[i].y);
		float dist_sqr = dist * dist;
		accum += vec2(dist, dist_sqr);
	}

	FragColor = accum.xyxy * shadowInvSampleCount;
#endif /* Prefilter */
#else /* PCF (no prefilter) */
	FragColor = vec4(get_cascade_world_distance(uvs));
#endif
}

#else /* CUBEMAP */

void main() {
	vec2 uvs = gl_FragCoord.xy * storedTexelSize;

	/* add a 2 pixel border to ensure filtering is correct */
	uvs.xy *= 1.0 + storedTexelSize * 2.0;
	uvs.xy -= storedTexelSize;

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
	vec3 cubevec = normalize(octahedral_to_cubemap_proj(uvs.xy));

/* TODO Can be optimized by groupping fetches
 * and by converting to radial distance beforehand. */
#if defined(ESM) || defined(VSM)
	vec3 T, B;
	make_orthonormal_basis(cubevec, wang_hash_noise(0u), T, B);

	T *= shadowFilterSize;
	B *= shadowFilterSize;

#ifdef ESM
	float accum = 0.0;

	/* Poisson disc blur in log space. */
	float depth1 = get_cube_radial_distance(cubevec + poisson[0].x * T + poisson[0].y * B);
	float depth2 = get_cube_radial_distance(cubevec + poisson[1].x * T + poisson[1].y * B);
	accum = ln_space_prefilter(INV_SAMPLE_NUM, depth1, INV_SAMPLE_NUM, depth2);

	for (int i = 2; i < SAMPLE_NUM; ++i) {
		depth1 = get_cube_radial_distance(cubevec + poisson[i].x * T + poisson[i].y * B);
		accum = ln_space_prefilter(1.0, accum, INV_SAMPLE_NUM, depth1);
	}

	FragColor = vec4(accum);
#else /* VSM */
	vec2 accum = vec2(0.0);

	/* Poisson disc blur. */
	for (int i = 0; i < SAMPLE_NUM; ++i) {
		float dist = get_cube_radial_distance(cubevec + poisson[i].x * T + poisson[i].y * B);
		float dist_sqr = dist * dist;
		accum += vec2(dist, dist_sqr);
	}

	FragColor = accum.xyxy * shadowInvSampleCount;
#endif /* Prefilter */
#else /* PCF (no prefilter) */
	FragColor = vec4(get_cube_radial_distance(cubevec));
#endif
}
#endif