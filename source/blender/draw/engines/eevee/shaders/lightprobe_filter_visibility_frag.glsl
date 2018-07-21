
uniform samplerCube probeDepth;
uniform int outputSize;
uniform float lodFactor;
uniform float storedTexelSize;
uniform float lodMax;
uniform float nearClip;
uniform float farClip;
uniform float visibilityRange;
uniform float visibilityBlur;

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

float linear_depth(float z)
{
	return (nearClip  * farClip) / (z * (nearClip - farClip) + farClip);
}

float get_world_distance(float depth, vec3 cos)
{
	float is_background = step(1.0, depth);
	depth = linear_depth(depth);
	depth += 1e1 * is_background;
	cos = normalize(abs(cos));
	float cos_vec = max(cos.x, max(cos.y, cos.z));
	return depth / cos_vec;
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy) % ivec2(outputSize);

	vec3 cos;

	cos.xy = (vec2(texel) + 0.5) * storedTexelSize;

	/* add a 2 pixel border to ensure filtering is correct */
	cos.xy = (cos.xy - storedTexelSize) / (1.0 - 2.0 * storedTexelSize);

	float pattern = 1.0;

	/* edge mirroring : only mirror if directly adjacent
	 * (not diagonally adjacent) */
	vec2 m = abs(cos.xy - 0.5) + 0.5;
	vec2 f = floor(m);
	if (f.x - f.y != 0.0) {
		cos.xy = 1.0 - cos.xy;
	}

	/* clamp to [0-1] */
	cos.xy = fract(cos.xy);

	/* get cubemap vector */
	cos = normalize(octahedral_to_cubemap_proj(cos.xy));

	vec3 T, B;
	make_orthonormal_basis(cos, T, B); /* Generate tangent space */

	vec2 accum = vec2(0.0);

	for (float i = 0; i < sampleCount; i++) {
		vec3 sample = sample_cone(i, M_PI_2 * visibilityBlur, cos, T, B);
		float depth = texture(probeDepth, sample).r;
		depth = get_world_distance(depth, sample);
		accum += vec2(depth, depth * depth);
	}

	accum *= invSampleCount;
	accum = abs(accum);

	/* Encode to normalized RGBA 8 */
	FragColor = visibility_encode(accum, visibilityRange);
}
