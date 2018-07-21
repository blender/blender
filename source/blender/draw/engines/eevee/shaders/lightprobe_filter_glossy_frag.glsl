
uniform samplerCube probeHdr;
uniform float roughnessSquared;
uniform float texelSize;
uniform float lodFactor;
uniform float lodMax;
uniform float paddingSize;
uniform float intensityFac;

in vec3 worldPosition;

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

void main() {
	vec2 uvs = gl_FragCoord.xy * texelSize;

	/* Add a N pixel border to ensure filtering is correct
	 * for N mipmap levels. */
	uvs = (uvs - paddingSize) / (1.0 - 2.0 * paddingSize);

	/* edge mirroring : only mirror if directly adjacent
	 * (not diagonally adjacent) */
	vec2 m = abs(uvs - 0.5) + 0.5;
	vec2 f = floor(m);
	if (f.x - f.y != 0.0) {
		uvs = 1.0 - uvs;
	}

	/* clamp to [0-1] */
	uvs = fract(uvs);

	/* get cubemap vector */
	vec3 cubevec = octahedral_to_cubemap_proj(uvs);

	vec3 N, T, B, V;

	vec3 R = normalize(cubevec);

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

	FragColor = vec4(intensityFac * out_radiance / weight, 1.0);
}
