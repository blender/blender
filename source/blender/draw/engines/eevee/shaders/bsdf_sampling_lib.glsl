
uniform sampler1D texHammersley;
uniform sampler2D texJitter;
uniform float sampleCount;
uniform float invSampleCount;

vec2 jitternoise = vec2(0.0);

void setup_noise(void)
{
	jitternoise = texture(texJitter, gl_FragCoord.xy / NOISE_SIZE).rg; /* Global variable */
}

vec3 hammersley_3d(float i, float invsamplenbr)
{
	vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

	Xi.x = i * invsamplenbr; /* i/samples */
	Xi.x = fract(Xi.x + jitternoise.x);

	int u = int(mod(i + jitternoise.y * HAMMERSLEY_SIZE, HAMMERSLEY_SIZE));

	Xi.yz = texelFetch(texHammersley, u, 0).rg;

	return Xi;
}

vec3 hammersley_3d(float i)
{
	return hammersley_3d(i, invSampleCount);
}

/* -------------- BSDFS -------------- */

float pdf_ggx_reflect(float NH, float a2)
{
	return NH * a2 / D_ggx_opti(NH, a2);
}

float pdf_hemisphere()
{
	return 0.5 * M_1_PI;
}

vec3 sample_ggx(float nsample, float a2, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	/* Theta is the aperture angle of the cone */
	float z = sqrt( (1.0 - Xi.x) / ( 1.0 + a2 * Xi.x - Xi.x ) ); /* cos theta */
	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	/* Microfacet Normal */
	vec3 Ht = vec3(x, y, z);

	return tangent_to_world(Ht, N, T, B);
}

vec3 sample_hemisphere(float nsample, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float z = Xi.x; /* cos theta */
	float r = sqrt( 1.0f - z*z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	vec3 Ht = vec3(x, y, z);

	return tangent_to_world(Ht, N, T, B);
}