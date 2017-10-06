
uniform int light_count;
uniform int probe_count;
uniform int grid_count;
uniform int planar_count;

uniform bool specToggle;
uniform bool ssrToggle;

uniform float refractionDepth;

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

in vec3 worldPosition;
in vec3 viewPosition;

#ifdef USE_FLAT_NORMAL
flat in vec3 worldNormal;
flat in vec3 viewNormal;
#else
in vec3 worldNormal;
in vec3 viewNormal;
#endif

uniform float maxRoughness;
uniform int rayCount;

/* ----------- default -----------  */

vec3 eevee_surface_lit(vec3 N, vec3 albedo, vec3 f0, float roughness, float ao, int ssr_id, out vec3 ssr_spec)
{
	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;
	}
#endif

	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	vec3 V = cameraVec;

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(V, N);
	norm_view = normalize(cross(norm_view, N)); /* Normal facing view */
#endif

	vec3 diff = vec3(0.0);
	vec3 spec = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, N, V, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		diff += l_color_vis * light_diffuse(ld, -norm_lamp, V, l_vector) * occlu_trans;
		spec += l_color_vis * light_specular(ld, N, view_vec, l_vector, roughnessSquared, f0) * occlu;
#else
		diff += l_color_vis * light_diffuse(ld, N, V, l_vector);
		spec += l_color_vis * light_specular(ld, N, V, l_vector, roughnessSquared, f0);
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = diff * albedo + spec * float(specToggle);

#ifdef HAIR_SHADER
	N = -norm_view;
#endif

	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 spec_accum = vec4(0.0);

	/* SSR lobe is applied later in a defered style */
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		/* Planar Reflections */
		for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
			PlanarData pd = planars_data[i];

			float fade = probe_attenuation_planar(pd, worldPosition, N, roughness);

			if (fade > 0.0) {
				vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, roughness, fade);
				accumulate_light(spec, fade, spec_accum);
			}
		}

		/* Specular probes */
		vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);

		/* Starts at 1 because 0 is world probe */
		for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
			CubeData cd = probes_data[i];

			float fade = probe_attenuation_cube(cd, worldPosition);

			if (fade > 0.0) {
				vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
				accumulate_light(spec, fade, spec_accum);
			}
		}

		/* World Specular */
		if (spec_accum.a < 0.999) {
			vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
			accumulate_light(spec, 1.0, spec_accum);
		}
	}

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	ssr_spec = F_ibl(f0, brdf_lut);
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		ssr_spec *= specular_occlusion(dot(N, V), final_ao, roughness);
	}
	out_light += spec_accum.rgb * ssr_spec * float(specToggle);

	/* ---------------- DIFFUSE ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 diff_accum = vec4(0.0);

	/* Start at 1 because 0 is world irradiance */
	for (int i = 1; i < MAX_GRID && i < grid_count && diff_accum.a < 0.999; ++i) {
		GridData gd = grids_data[i];

		vec3 localpos;
		float fade = probe_attenuation_grid(gd, worldPosition, localpos);

		if (fade > 0.0) {
			vec3 diff = probe_evaluate_grid(gd, worldPosition, bent_normal, localpos);
			accumulate_light(diff, fade, diff_accum);
		}
	}

	/* World Diffuse */
	if (diff_accum.a < 0.999 && grid_count > 0) {
		vec3 diff = probe_evaluate_world_diff(bent_normal);
		accumulate_light(diff, 1.0, diff_accum);
	}

	out_light += diff_accum.rgb * albedo * gtao_multibounce(final_ao, albedo);

	return out_light;
}

/* ----------- CLEAR COAT -----------  */

vec3 eevee_surface_clearcoat_lit(
        vec3 N, vec3 albedo, vec3 f0, float roughness,
        vec3 C_N, float C_intensity, float C_roughness, /* Clearcoat params */
        float ao, int ssr_id, out vec3 ssr_spec)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;
	C_roughness = clamp(C_roughness, 1e-8, 0.9999);
	float C_roughnessSquared = C_roughness * C_roughness;

	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
	C_N = normalize(C_N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;

		len = length(C_N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		C_N /= len;
	}
#endif

	vec3 V = cameraVec;

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(V, N);
	norm_view = normalize(cross(norm_view, N)); /* Normal facing view */
#endif

	vec3 diff = vec3(0.0);
	vec3 spec = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, N, V, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		diff += l_color_vis * light_diffuse(ld, -norm_lamp, V, l_vector) * occlu_trans;
		spec += l_color_vis * light_specular(ld, N, view_vec, l_vector, roughnessSquared, f0) * occlu;
		spec += l_color_vis * light_specular(ld, C_N, view_vec, l_vector, C_roughnessSquared, f0) * C_intensity * occlu;
#else
		diff += l_color_vis * light_diffuse(ld, N, V, l_vector);
		spec += l_color_vis * light_specular(ld, N, V, l_vector, roughnessSquared, f0);
		spec += l_color_vis * light_specular(ld, C_N, V, l_vector, C_roughnessSquared, f0) * C_intensity;
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = diff * albedo + spec * float(specToggle);

#ifdef HAIR_SHADER
	N = -norm_view;
#endif

	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 spec_accum = vec4(0.0);
	vec4 C_spec_accum = vec4(0.0);

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		/* Fade on geometric normal. */
		float fade = probe_attenuation_planar(pd, worldPosition, worldNormal, roughness);

		if (fade > 0.0) {
			if (!(ssrToggle && ssr_id == outputSsrId)) {
				vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, roughness, fade);
				accumulate_light(spec, fade, spec_accum);
			}

			vec3 C_spec = probe_evaluate_planar(float(i), pd, worldPosition, C_N, V, C_roughness, fade);
			accumulate_light(C_spec, fade, C_spec_accum);
		}
	}

	/* Specular probes */
	vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);
	vec3 C_spec_dir = get_specular_reflection_dominant_dir(C_N, V, C_roughnessSquared);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, worldPosition);

		if (fade > 0.0) {
			if (!(ssrToggle && ssr_id == outputSsrId)) {
				vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
				accumulate_light(spec, fade, spec_accum);
			}

			vec3 C_spec = probe_evaluate_cube(float(i), cd, worldPosition, C_spec_dir, C_roughness);
			accumulate_light(C_spec, fade, C_spec_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		if (!(ssrToggle && ssr_id == outputSsrId)) {
			vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
			accumulate_light(spec, 1.0, spec_accum);
		}

		vec3 C_spec = probe_evaluate_world_spec(C_spec_dir, C_roughness);
		accumulate_light(C_spec, 1.0, C_spec_accum);
	}

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	ssr_spec = F_ibl(f0, brdf_lut);
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		ssr_spec *= specular_occlusion(dot(N, V), final_ao, roughness);
	}
	out_light += spec_accum.rgb * ssr_spec * float(specToggle);

	uv = lut_coords(dot(C_N, V), C_roughness);
	brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	out_light += C_spec_accum.rgb * F_ibl(vec3(0.04), brdf_lut) * specular_occlusion(dot(C_N, V), final_ao, C_roughness) * float(specToggle) * C_intensity;

	/* ---------------- DIFFUSE ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 diff_accum = vec4(0.0);

	/* Start at 1 because 0 is world irradiance */
	for (int i = 1; i < MAX_GRID && i < grid_count && diff_accum.a < 0.999; ++i) {
		GridData gd = grids_data[i];

		vec3 localpos;
		float fade = probe_attenuation_grid(gd, worldPosition, localpos);

		if (fade > 0.0) {
			vec3 diff = probe_evaluate_grid(gd, worldPosition, bent_normal, localpos);
			accumulate_light(diff, fade, diff_accum);
		}
	}

	/* World Diffuse */
	if (diff_accum.a < 0.999 && grid_count > 0) {
		vec3 diff = probe_evaluate_world_diff(bent_normal);
		accumulate_light(diff, 1.0, diff_accum);
	}

	out_light += diff_accum.rgb * albedo * gtao_multibounce(final_ao, albedo);

	return out_light;
}

/* ----------- Diffuse -----------  */

vec3 eevee_surface_diffuse_lit(vec3 N, vec3 albedo, float ao)
{
	vec3 V = cameraVec;

	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;
	}
#endif

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(V, N);
	norm_view = normalize(cross(norm_view, N)); /* Normal facing view */
#endif

	vec3 diff = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, N, V, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		diff += l_color_vis * light_diffuse(ld, -norm_lamp, V, l_vector) * occlu_trans;
#else
		diff += l_color_vis * light_diffuse(ld, N, V, l_vector);
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = diff * albedo;

#ifdef HAIR_SHADER
	N = -norm_view;
#endif

	/* ---------------- DIFFUSE ENVIRONMENT LIGHTING ----------------- */

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 diff_accum = vec4(0.0);

	/* Start at 1 because 0 is world irradiance */
	for (int i = 1; i < MAX_GRID && i < grid_count && diff_accum.a < 0.999; ++i) {
		GridData gd = grids_data[i];

		vec3 localpos;
		float fade = probe_attenuation_grid(gd, worldPosition, localpos);

		if (fade > 0.0) {
			vec3 diff = probe_evaluate_grid(gd, worldPosition, bent_normal, localpos);
			accumulate_light(diff, fade, diff_accum);
		}
	}

	/* World Diffuse */
	if (diff_accum.a < 0.999 && grid_count > 0) {
		vec3 diff = probe_evaluate_world_diff(bent_normal);
		accumulate_light(diff, 1.0, diff_accum);
	}

	out_light += diff_accum.rgb * albedo * gtao_multibounce(final_ao, albedo);

	return out_light;
}

/* ----------- Glossy -----------  */

vec3 eevee_surface_glossy_lit(vec3 N, vec3 f0, float roughness, float ao, int ssr_id, out vec3 ssr_spec)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	vec3 V = cameraVec;

	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;
	}
#endif

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(V, N);
	norm_view = normalize(cross(norm_view, N)); /* Normal facing view */
#endif

	vec3 spec = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, N, V, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		spec += l_color_vis * light_specular(ld, N, view_vec, l_vector, roughnessSquared, f0) * occlu;
#else
		spec += l_color_vis * light_specular(ld, N, V, l_vector, roughnessSquared, f0);
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = spec * float(specToggle);

#ifdef HAIR_SHADER
	N = -norm_view;
#endif

	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 spec_accum = vec4(0.0);

	if (!(ssrToggle && ssr_id == outputSsrId)) {
		/* Planar Reflections */
		for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
			PlanarData pd = planars_data[i];

			float fade = probe_attenuation_planar(pd, worldPosition, N, roughness);

			if (fade > 0.0) {
				vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, roughness, fade);
				accumulate_light(spec, fade, spec_accum);
			}
		}

		/* Specular probes */
		vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);

		/* Starts at 1 because 0 is world probe */
		for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
			CubeData cd = probes_data[i];

			float fade = probe_attenuation_cube(cd, worldPosition);

			if (fade > 0.0) {
				vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
				accumulate_light(spec, fade, spec_accum);
			}
		}

		/* World Specular */
		if (spec_accum.a < 0.999) {
			vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
			accumulate_light(spec, 1.0, spec_accum);
		}
	}

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	ssr_spec = F_ibl(f0, brdf_lut);
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		/* Ambient Occlusion */
		vec3 bent_normal;
		float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

		ssr_spec *= specular_occlusion(dot(N, V), final_ao, roughness);
	}
	out_light += spec_accum.rgb * ssr_spec * float(specToggle);

	return out_light;
}

/* ----------- Transmission -----------  */

vec3 eevee_surface_refraction(vec3 N, vec3 f0, float roughness, float ior)
{
	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;
	}
#endif
	vec3 V = cameraVec;
	ior = (gl_FrontFacing) ? ior : 1.0 / ior;

	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

	/* No support for now. Supporting LTCs mean having a 3D LUT.
	 * We could support point lights easily though. */

	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 trans_accum = vec4(0.0);

	/* Refract the view vector using the depth heuristic.
	 * Then later Refract a second time the already refracted
	 * ray using the inverse ior. */
	float final_ior = (refractionDepth > 0.0) ? 1.0 / ior : ior;
	vec3 refr_V = (refractionDepth > 0.0) ? -refract(-V, N, final_ior) : V;
	vec3 refr_pos = (refractionDepth > 0.0) ? line_plane_intersect(worldPosition, refr_V, worldPosition - N * refractionDepth, N) : worldPosition;

#ifdef USE_REFRACTION
	/* Screen Space Refraction */
	if (ssrToggle && roughness < maxRoughness + 0.2) {
		vec3 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0)).xzw;

		/* Find approximated position of the 2nd refraction event. */
		vec3 refr_vpos = (refractionDepth > 0.0) ? transform_point(ViewMatrix, refr_pos) : viewPosition;

		float ray_ofs = 1.0 / float(rayCount);
		vec4 spec = screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand, 0.0);
		if (rayCount > 1) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xyz * vec3(1.0, -1.0, -1.0), 1.0 * ray_ofs);
		if (rayCount > 2) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xzy * vec3(1.0,  1.0, -1.0), 2.0 * ray_ofs);
		if (rayCount > 3) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xzy * vec3(1.0, -1.0,  1.0), 3.0 * ray_ofs);
		spec /= float(rayCount);
		spec.a *= smoothstep(maxRoughness + 0.2, maxRoughness, roughness);
		accumulate_light(spec.rgb, spec.a, trans_accum);
	}
#endif

	/* Specular probes */
	/* NOTE: This bias the IOR */
	vec3 refr_dir = get_specular_refraction_dominant_dir(N, refr_V, roughness, final_ior);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && trans_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, worldPosition);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_cube(float(i), cd, refr_pos, refr_dir, roughnessSquared);
			accumulate_light(spec, fade, trans_accum);
		}
	}

	/* World Specular */
	if (trans_accum.a < 0.999) {
		vec3 spec = probe_evaluate_world_spec(refr_dir, roughnessSquared);
		accumulate_light(spec, 1.0, trans_accum);
	}

	float btdf = get_btdf_lut(utilTex, dot(N, V), roughness, ior);

	return trans_accum.rgb * btdf;
}

vec3 eevee_surface_glass(vec3 N, vec3 transmission_col, float roughness, float ior, int ssr_id, out vec3 ssr_spec)
{
	/* Zero length vectors cause issues, see: T51979. */
#if 0
	N = normalize(N);
#else
	{
		float len = length(N);
		if (isnan(len)) {
			return vec3(0.0);
		}
		N /= len;
	}
#endif
	vec3 V = cameraVec;
	ior = (gl_FrontFacing) ? ior : 1.0 / ior;

	if (!specToggle) return vec3(0.0);

	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(V, N);
	norm_view = normalize(cross(norm_view, N)); /* Normal facing view */
#endif

	vec3 spec = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, N, V, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		spec += l_color_vis * light_specular(ld, N, view_vec, l_vector, roughnessSquared, vec3(1.0)) * occlu;
#else
		spec += l_color_vis * light_specular(ld, N, V, l_vector, roughnessSquared, vec3(1.0));
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = spec;

#ifdef HAIR_SHADER
	N = -norm_view;
#endif


	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 spec_accum = vec4(0.0);

	/* Planar Reflections */
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999 && roughness < 0.1; ++i) {
			PlanarData pd = planars_data[i];

			float fade = probe_attenuation_planar(pd, worldPosition, N, roughness);

			if (fade > 0.0) {
				vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, roughness, fade);
				accumulate_light(spec, fade, spec_accum);
			}
		}
	}

	/* Refract the view vector using the depth heuristic.
	 * Then later Refract a second time the already refracted
	 * ray using the inverse ior. */
	float final_ior = (refractionDepth > 0.0) ? 1.0 / ior : ior;
	vec3 refr_V = (refractionDepth > 0.0) ? -refract(-V, N, final_ior) : V;
	vec3 refr_pos = (refractionDepth > 0.0) ? line_plane_intersect(worldPosition, refr_V, worldPosition - N * refractionDepth, N) : worldPosition;

	vec4 trans_accum = vec4(0.0);

#ifdef USE_REFRACTION
	/* Screen Space Refraction */
	if (ssrToggle && roughness < maxRoughness + 0.2) {
		vec3 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0)).xzw;

		/* Find approximated position of the 2nd refraction event. */
		vec3 refr_vpos = (refractionDepth > 0.0) ? transform_point(ViewMatrix, refr_pos) : viewPosition;

		float ray_ofs = 1.0 / float(rayCount);
		vec4 spec = screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand, 0.0);
		if (rayCount > 1) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xyz * vec3(1.0, -1.0, -1.0), 1.0 * ray_ofs);
		if (rayCount > 2) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xzy * vec3(1.0,  1.0, -1.0), 2.0 * ray_ofs);
		if (rayCount > 3) spec += screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand.xzy * vec3(1.0, -1.0,  1.0), 3.0 * ray_ofs);
		spec /= float(rayCount);
		spec.a *= smoothstep(maxRoughness + 0.2, maxRoughness, roughness);
		accumulate_light(spec.rgb, spec.a, trans_accum);
	}
#endif

	/* Specular probes */
	vec3 refr_dir = get_specular_refraction_dominant_dir(N, refr_V, roughness, final_ior);
	vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && (spec_accum.a < 0.999 || trans_accum.a < 0.999); ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, worldPosition);

		if (fade > 0.0) {
			if (!(ssrToggle && ssr_id == outputSsrId)) {
				vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
				accumulate_light(spec, fade, spec_accum);
			}

			spec = probe_evaluate_cube(float(i), cd, refr_pos, refr_dir, roughnessSquared);
			accumulate_light(spec, fade, trans_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		if (!(ssrToggle && ssr_id == outputSsrId)) {
			vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
			accumulate_light(spec, 1.0, spec_accum);
		}
	}

	if (trans_accum.a < 0.999) {
		spec = probe_evaluate_world_spec(refr_dir, roughnessSquared);
		accumulate_light(spec, 1.0, trans_accum);
	}

	/* Ambient Occlusion */
	/* TODO : when AO will be cheaper */
	float final_ao = 1.0;

	float NV = dot(N, V);
	/* Get Brdf intensity */
	vec2 uv = lut_coords(NV, roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	float fresnel = F_eta(ior, NV);

	/* Apply fresnel on lamps. */
	out_light *= vec3(fresnel);

	ssr_spec = vec3(fresnel) * F_ibl(vec3(1.0), brdf_lut);
	if (!(ssrToggle && ssr_id == outputSsrId)) {
		ssr_spec *= specular_occlusion(dot(N, V), final_ao, roughness);
	}
	out_light += spec_accum.rgb * ssr_spec;


	float btdf = get_btdf_lut(utilTex, NV, roughness, ior);

	out_light += vec3(1.0 - fresnel) * transmission_col * trans_accum.rgb * btdf;

	return out_light;
}
