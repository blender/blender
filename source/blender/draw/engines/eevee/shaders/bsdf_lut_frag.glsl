
out vec4 FragColor;

void main() {
	vec3 N, T, B, V;

	float NV = ( 1.0 - (clamp(gl_FragCoord.y / BRDF_LUT_SIZE, 1e-4, 0.9999)));
	float sqrtRoughness = clamp(gl_FragCoord.x / BRDF_LUT_SIZE, 1e-4, 0.9999);
	float a = sqrtRoughness * sqrtRoughness;
	float a2 = a * a;

	N = vec3(0.0, 0.0, 1.0);
	T = vec3(1.0, 0.0, 0.0);
	B = vec3(0.0, 1.0, 0.0);
	V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

	setup_noise();

	/* Integrating BRDF */
	float brdf_accum = 0.0;
	float fresnel_accum = 0.0;
	for (float i = 0; i < sampleCount; i++) {
		vec3 H = sample_ggx(i, a2, N, T, B); /* Microfacet normal */
		vec3 L = -reflect(V, H);
		float NL = L.z;

		if (NL > 0.0) {
			float NH = max(H.z, 0.0);
			float VH = max(dot(V, H), 0.0);

			float G1_v = G1_Smith_GGX(NV, a2);
			float G1_l = G1_Smith_GGX(NL, a2);
			float G_smith = 4.0 * NV * NL / (G1_v * G1_l); /* See G1_Smith_GGX for explanations. */

			float brdf = (G_smith * VH) / (NH * NV);
			float Fc = pow(1.0 - VH, 5.0);

			brdf_accum += (1.0 - Fc) * brdf;
			fresnel_accum += Fc * brdf;
		}
	}
	brdf_accum /= sampleCount;
	fresnel_accum /= sampleCount;

	FragColor = vec4(brdf_accum, fresnel_accum, 0.0, 1.0);
}
