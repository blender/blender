#define NO_OBJECT_ID uint(0)
#define EPSILON 0.00001
#define M_PI 3.14159265358979323846

/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5) * (1.0 / 16.0) - 0.5) * (1.0 / 255.0))
const vec4 dither_mat4x4[4] = vec4[4](
	vec4( P(0.0),  P(8.0),  P(2.0), P(10.0)),
	vec4(P(12.0),  P(4.0), P(14.0),  P(6.0)),
	vec4( P(3.0), P(11.0),  P(1.0),  P(9.0)),
	vec4(P(15.0),  P(7.0), P(13.0),  P(5.0))
);

float bayer_dither_noise() {
	ivec2 tx1 = ivec2(gl_FragCoord.xy) % 4;
	ivec2 tx2 = ivec2(gl_FragCoord.xy) % 2;
	return dither_mat4x4[tx1.x][tx1.y];
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec3 normal_decode(vec2 enc)
{
	vec2 fenc = enc.xy * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(1.0 - f / 4.0);
	vec3 n;
	n.xy = fenc*g;
	n.z = 1 - f / 2;
	return n;
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec2 normal_encode(vec3 n)
{
	float p = sqrt(n.z * 8.0 + 8.0);
	return vec2(n.xy / p + 0.5);
}

void fresnel(vec3 I, vec3 N, float ior, out float kr)
{
	float cosi = clamp(dot(I, N), -1.0, 1.0);
	float etai = 1.0;
	float etat = ior;
	if (cosi > 0) {
		etat = 1.0;
		etai = ior;
	}

	// Compute sini using Snell's law
	float sint = etai / etat * sqrt(max(0.0, 1.0 - cosi * cosi));
	// Total internal reflection
	if (sint >= 1) {
		kr = 1;
	}
	else {
		float cost = sqrt(max(0.0, 1.0 - sint * sint));
		cosi = abs(cosi);
		float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
		float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
		kr = (Rs * Rs + Rp * Rp) / 2;
	}
	// As a consequence of the conservation of energy, transmittance is given by:
	// kt = 1 - kr;
}

float calculate_transparent_weight(float z, float alpha)
{
#if 0
	/* Eq 10 : Good for surfaces with varying opacity (like particles) */
	float a = min(1.0, alpha * 10.0) + 0.01;
	float b = -gl_FragCoord.z * 0.95 + 1.0;
	float w = a * a * a * 3e2 * b * b * b;
#else
	/* Eq 7 put more emphasis on surfaces closer to the view. */
	// float w = 10.0 / (1e-5 + pow(abs(z) / 5.0, 2.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 7 */
	// float w = 10.0 / (1e-5 + pow(abs(z) / 10.0, 3.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 8 */
	// float w = 10.0 / (1e-5 + pow(abs(z) / 200.0, 4.0)); /* Eq 9 */
	/* Same as eq 7, but optimized. */
	float a = abs(z) / 5.0;
	float b = abs(z) / 200.0;
	b *= b;
	float w = 10.0 / ((1e-5 + a * a) + b * (b * b)); /* Eq 7 */
#endif
	return alpha * clamp(w, 1e-2, 3e2);
}

/* Special function only to be used with calculate_transparent_weight(). */
float linear_zdepth(float depth, vec4 viewvecs[3], mat4 proj_mat)
{
	if (proj_mat[3][3] == 0.0) {
		float d = 2.0 * depth - 1.0;
		return -proj_mat[3][2] / (d + proj_mat[2][2]);
	}
	else {
		/* Return depth from near plane. */
		return depth * viewvecs[1].z;
	}
}

vec3 view_vector_from_screen_uv(vec2 uv, vec4 viewvecs[3], mat4 proj_mat)
{
	return (proj_mat[3][3] == 0.0)
	             ? normalize(viewvecs[0].xyz + vec3(uv, 0.0) * viewvecs[1].xyz)
	             : vec3(0.0, 0.0, 1.0);
}

vec2 matcap_uv_compute(vec3 I, vec3 N, bool flipped)
{
	/* Quick creation of an orthonormal basis */
	float a = 1.0 / (1.0 + I.z);
	float b = -I.x * I.y * a;
	vec3 b1 = vec3(1.0 - I.x * I.x * a, b, -I.x);
	vec3 b2 = vec3(b, 1.0 - I.y * I.y * a, -I.y);
	vec2 matcap_uv = vec2(dot(b1, N), dot(b2, N));
	if (flipped) {
		matcap_uv.x = -matcap_uv.x;
	}
	return matcap_uv * 0.496 + 0.5;
}
