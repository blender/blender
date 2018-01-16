/* ------------ Refraction ------------ */

#define BTDF_BIAS 0.85

vec4 screen_space_refraction(vec3 viewPosition, vec3 N, vec3 V, float ior, float roughnessSquared, vec4 rand)
{
	float a2 = max(5e-6, roughnessSquared * roughnessSquared);

	/* Importance sampling bias */
	rand.x = mix(rand.x, 0.0, BTDF_BIAS);

	vec3 T, B;
	float NH;
	make_orthonormal_basis(N, T, B);
	vec3 H = sample_ggx(rand.xzw, a2, N, T, B, NH); /* Microfacet normal */
	float pdf = pdf_ggx_reflect(NH, a2);

	/* If ray is bad (i.e. going below the plane) regenerate. */
	if (F_eta(ior, dot(H, V)) < 1.0) {
		H = sample_ggx(rand.xzw * vec3(1.0, -1.0, -1.0), a2, N, T, B, NH); /* Microfacet normal */
		pdf = pdf_ggx_reflect(NH, a2);
	}

	vec3 vV = viewCameraVec;
	float eta = 1.0/ior;
	if (dot(H, V) < 0.0) {
		H = -H;
		eta = ior;
	}

	vec3 R = refract(-V, H, 1.0 / ior);

	R = transform_direction(ViewMatrix, R);

	vec3 hit_pos = raycast(-1, viewPosition, R * 1e16, ssrThickness, rand.y, ssrQuality, roughnessSquared, false);

	if ((hit_pos.z > 0.0) && (F_eta(ior, dot(H, V)) < 1.0)) {
		hit_pos = get_view_space_from_depth(hit_pos.xy, hit_pos.z);
		float hit_dist = distance(hit_pos, viewPosition);

		float cone_cos = cone_cosine(roughnessSquared);
		float cone_tan = sqrt(1 - cone_cos * cone_cos) / cone_cos;

		/* Empirical fit for refraction. */
		/* TODO find a better fit or precompute inside the LUT. */
		cone_tan *= 0.5 * fast_sqrt(f0_from_ior((ior < 1.0) ? 1.0 / ior : ior));

		float cone_footprint = hit_dist * cone_tan;

		/* find the offset in screen space by multiplying a point
		 * in camera space at the depth of the point by the projection matrix. */
		float homcoord = ProjectionMatrix[2][3] * hit_pos.z + ProjectionMatrix[3][3];
		/* UV space footprint */
		cone_footprint = BTDF_BIAS * 0.5 * max(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) * cone_footprint / homcoord;

		vec2 hit_uvs = project_point(ProjectionMatrix, hit_pos).xy * 0.5 + 0.5;

		/* Texel footprint */
		vec2 texture_size = vec2(textureSize(colorBuffer, 0).xy);
		float mip = clamp(log2(cone_footprint * max(texture_size.x, texture_size.y)), 0.0, 9.0);

		/* Correct UVs for mipmaping mis-alignment */
		hit_uvs *= mip_ratio_interp(mip);

		vec3 spec = textureLod(colorBuffer, hit_uvs, mip).xyz;
		float mask = screen_border_mask(hit_uvs);

		return vec4(spec, mask);
	}

	return vec4(0.0);
}
