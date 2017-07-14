
uniform int light_count;
uniform int probe_count;
uniform int grid_count;
uniform int planar_count;

uniform bool specToggle;

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

/* ----------- default -----------  */

vec3 eevee_surface_lit(vec3 N, vec3 albedo, vec3 f0, float roughness, float ao)
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

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

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

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, l_vector);

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

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		float fade = probe_attenuation_planar(pd, worldPosition, N);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, rand.r, cameraPos, roughness, fade);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* Specular probes */
	vec3 spec_dir = get_specular_dominant_dir(N, V, roughnessSquared);

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

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	out_light += spec_accum.rgb * F_ibl(f0, brdf_lut) * specular_occlusion(dot(N, V), final_ao, roughness) * float(specToggle);

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
        float ao)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;
	C_roughness = clamp(C_roughness, 1e-8, 0.9999);
	float C_roughnessSquared = C_roughness * C_roughness;

	vec3 V = cameraVec;
	N = normalize(N);
	C_N = normalize(C_N);

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

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

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, l_vector);

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
		float fade = probe_attenuation_planar(pd, worldPosition, worldNormal);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, rand.r, cameraPos, roughness, fade);
			accumulate_light(spec, fade, spec_accum);

			vec3 C_spec = probe_evaluate_planar(float(i), pd, worldPosition, C_N, V, rand.r, cameraPos, C_roughness, fade);
			accumulate_light(C_spec, fade, C_spec_accum);
		}
	}

	/* Specular probes */
	vec3 spec_dir = get_specular_dominant_dir(N, V, roughnessSquared);
	vec3 C_spec_dir = get_specular_dominant_dir(C_N, V, C_roughnessSquared);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, worldPosition);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
			accumulate_light(spec, fade, spec_accum);

			vec3 C_spec = probe_evaluate_cube(float(i), cd, worldPosition, C_spec_dir, C_roughness);
			accumulate_light(C_spec, fade, C_spec_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
		accumulate_light(spec, 1.0, spec_accum);

		vec3 C_spec = probe_evaluate_world_spec(C_spec_dir, C_roughness);
		accumulate_light(C_spec, 1.0, C_spec_accum);
	}

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	out_light += spec_accum.rgb * F_ibl(f0, brdf_lut) * specular_occlusion(dot(N, V), final_ao, roughness) * float(specToggle);

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
	N = normalize(N);

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

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

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, l_vector);

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

vec3 eevee_surface_glossy_lit(vec3 N, vec3 f0, float roughness, float ao)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	vec3 V = cameraVec;
	N = normalize(N);

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

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

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, l_vector);

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

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		float fade = probe_attenuation_planar(pd, worldPosition, N);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, rand.r, cameraPos, roughness, fade);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* Specular probes */
	vec3 spec_dir = get_specular_dominant_dir(N, V, roughnessSquared);

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

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(N, V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	out_light += spec_accum.rgb * F_ibl(f0, brdf_lut) * specular_occlusion(dot(N, V), final_ao, roughness) * float(specToggle);

	return out_light;
}
