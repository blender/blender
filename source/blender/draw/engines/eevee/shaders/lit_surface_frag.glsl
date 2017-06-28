
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


vec3 eevee_surface_lit(vec3 world_normal, vec3 albedo, vec3 f0, float roughness, float ao)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	ShadingData sd = ShadingData(cameraVec, normalize(world_normal));

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));

	/* ---------------- SCENE LAMPS LIGHTING ----------------- */

#ifdef HAIR_SHADER
	vec3 norm_view = cross(sd.V, sd.N);
	norm_view = normalize(cross(norm_view, sd.N)); /* Normal facing view */
#endif

	vec3 diff = vec3(0.0);
	vec3 spec = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, l_vector.xyz);

#ifdef HAIR_SHADER
		vec3 norm_lamp, view_vec;
		float occlu_trans, occlu;
		light_hair_common(ld, sd, l_vector, norm_view, occlu_trans, occlu, norm_lamp, view_vec);

		ShadingData hsd = sd;
		hsd.N = -norm_lamp;
		diff += l_color_vis * light_diffuse(ld, hsd, l_vector) * occlu_trans;
		hsd.V = view_vec;
		spec += l_color_vis * light_specular(ld, hsd, l_vector, roughnessSquared, f0) * occlu;
#else
		diff += l_color_vis * light_diffuse(ld, sd, l_vector);
		spec += l_color_vis * light_specular(ld, sd, l_vector, roughnessSquared, f0);
#endif
	}

	/* Accumulate outgoing radiance */
	vec3 out_light = diff * albedo + spec * float(specToggle);

#ifdef HAIR_SHADER
	sd.N = -norm_view;
#endif

	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */

	/* Envmaps */
	vec3 spec_dir = get_specular_dominant_dir(sd.N, sd.V, roughnessSquared);

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
	vec4 spec_accum = vec4(0.0);

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		float fade = probe_attenuation_planar(pd, worldPosition, sd.N);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, sd.N, sd.V, rand.a, cameraPos, roughness, fade);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* Specular probes */
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
	if (spec_accum.a < 1.0) {
		vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
		accumulate_light(spec, 1.0, spec_accum);
	}

	/* Ambient Occlusion */
	vec3 bent_normal;
	float final_ao = occlusion_compute(sd.N, viewPosition, ao, rand.rg, bent_normal);

	/* Get Brdf intensity */
	vec2 uv = lut_coords(dot(sd.N, sd.V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	out_light += spec_accum.rgb * F_ibl(f0, brdf_lut) * specular_occlusion(dot(sd.N, sd.V), final_ao, roughness) * float(specToggle);

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
	if (diff_accum.a < 1.0 && grid_count > 0) {
		vec3 diff = probe_evaluate_world_diff(bent_normal);
		accumulate_light(diff, 1.0, diff_accum);
	}

	out_light += diff_accum.rgb * albedo * gtao_multibounce(final_ao, albedo);

	return out_light;
}
