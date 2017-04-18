
uniform samplerCube probeHdr;
uniform float roughnessSquared;
uniform float lodFactor;
uniform float lodMax;

in vec3 worldPosition;

out vec4 FragColor;

void main() {
	vec3 N, T, B, V;

	vec3 R = normalize(worldPosition);

	/* Isotropic assumption */
	N = V = R;

	make_orthonormal_basis(N, T, B); /* Generate tangent space */

	/* Noise to dither the samples */
	/* Note : ghosting is better looking than noise. */
	// setup_noise();

	/* Integrating Envmap */
	float weight = 0.0;
	vec3 out_radiance = vec3(0.0);
	for (float i = 0; i < sampleCount; i++) {
		vec3 H = sample_ggx(i, roughnessSquared, N, T, B); /* Microfacet normal */
		vec3 L = -reflect(V, H);
		float NL = dot(N, L);

		if (NL > 0.0) {
			float NH = max(1e-8, dot(N, H)); /* cosTheta */

			/* Coarse Approximation of the mapping distortion
			 * Unit Sphere -> Cubemap Face */
			const float dist = 4.0 * M_PI / 6.0;
			float pdf = pdf_ggx_reflect(NH, roughnessSquared);
			/* http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html : Equation 13 */
			float lod = clamp(lodFactor - 0.5 * log2(pdf * dist), 0.0, lodMax) ;

			out_radiance += textureLod(probeHdr, L, lod).rgb * NL;
			weight += NL;
		}
	}

	FragColor = vec4(out_radiance / weight, 1.0);
}