
#ifndef LIT_SURFACE_UNIFORM
#define LIT_SURFACE_UNIFORM

uniform float refractionDepth;

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)
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

#ifdef HAIR_SHADER
in vec3 hairTangent; /* world space */
in float hairThickTime;
in float hairThickness;
in float hairTime;
flat in int hairStrandID;

uniform int hairThicknessRes = 1;
#endif

#endif /* LIT_SURFACE_UNIFORM */

/** AUTO CONFIG
 * We include the file multiple times each time with a different configuration.
 * This leads to a lot of deadcode. Better idea would be to only generate the one needed.
 */
#if !defined(SURFACE_DEFAULT)
	#define SURFACE_DEFAULT
	#define CLOSURE_NAME eevee_closure_default
	#define CLOSURE_DIFFUSE
	#define CLOSURE_GLOSSY
#endif /* SURFACE_DEFAULT */

#if !defined(SURFACE_PRINCIPLED) && !defined(CLOSURE_NAME)
	#define SURFACE_PRINCIPLED
	#define CLOSURE_NAME eevee_closure_principled
	#define CLOSURE_DIFFUSE
	#define CLOSURE_GLOSSY
	#define CLOSURE_CLEARCOAT
	#define CLOSURE_REFRACTION
	#define CLOSURE_SUBSURFACE
#endif /* SURFACE_PRINCIPLED */

#if !defined(SURFACE_CLEARCOAT) && !defined(CLOSURE_NAME)
	#define SURFACE_CLEARCOAT
	#define CLOSURE_NAME eevee_closure_clearcoat
	#define CLOSURE_GLOSSY
	#define CLOSURE_CLEARCOAT
#endif /* SURFACE_CLEARCOAT */

#if !defined(SURFACE_DIFFUSE) && !defined(CLOSURE_NAME)
	#define SURFACE_DIFFUSE
	#define CLOSURE_NAME eevee_closure_diffuse
	#define CLOSURE_DIFFUSE
#endif /* SURFACE_DIFFUSE */

#if !defined(SURFACE_SUBSURFACE) && !defined(CLOSURE_NAME)
	#define SURFACE_SUBSURFACE
	#define CLOSURE_NAME eevee_closure_subsurface
	#define CLOSURE_DIFFUSE
	#define CLOSURE_SUBSURFACE
#endif /* SURFACE_SUBSURFACE */

#if !defined(SURFACE_SKIN) && !defined(CLOSURE_NAME)
	#define SURFACE_SKIN
	#define CLOSURE_NAME eevee_closure_skin
	#define CLOSURE_DIFFUSE
	#define CLOSURE_SUBSURFACE
	#define CLOSURE_GLOSSY
#endif /* SURFACE_SKIN */

#if !defined(SURFACE_GLOSSY) && !defined(CLOSURE_NAME)
	#define SURFACE_GLOSSY
	#define CLOSURE_NAME eevee_closure_glossy
	#define CLOSURE_GLOSSY
#endif /* SURFACE_GLOSSY */

#if !defined(SURFACE_REFRACT) && !defined(CLOSURE_NAME)
	#define SURFACE_REFRACT
	#define CLOSURE_NAME eevee_closure_refraction
	#define CLOSURE_REFRACTION
#endif /* SURFACE_REFRACT */

#if !defined(SURFACE_GLASS) && !defined(CLOSURE_NAME)
	#define SURFACE_GLASS
	#define CLOSURE_NAME eevee_closure_glass
	#define CLOSURE_GLOSSY
	#define CLOSURE_REFRACTION
#endif /* SURFACE_GLASS */

/* Safety : CLOSURE_CLEARCOAT implies CLOSURE_GLOSSY */
#ifdef CLOSURE_CLEARCOAT
	#ifndef CLOSURE_GLOSSY
		#define CLOSURE_GLOSSY
	#endif
#endif /* CLOSURE_CLEARCOAT */

void CLOSURE_NAME(
        vec3 N
#ifdef CLOSURE_DIFFUSE
        , vec3 albedo
#endif
#ifdef CLOSURE_GLOSSY
        , vec3 f0, int ssr_id
#endif
#if defined(CLOSURE_GLOSSY) || defined(CLOSURE_REFRACTION)
        , float roughness
#endif
#ifdef CLOSURE_CLEARCOAT
        , vec3 C_N, float C_intensity, float C_roughness
#endif
#if defined(CLOSURE_GLOSSY) || defined(CLOSURE_DIFFUSE)
        , float ao
#endif
#ifdef CLOSURE_SUBSURFACE
        , float sss_scale
#endif
#ifdef CLOSURE_REFRACTION
        , float ior
#endif
#ifdef CLOSURE_DIFFUSE
        , out vec3 out_diff
#endif
#ifdef CLOSURE_SUBSURFACE
        , out vec3 out_trans
#endif
#ifdef CLOSURE_GLOSSY
        , out vec3 out_spec
#endif
#ifdef CLOSURE_REFRACTION
        , out vec3 out_refr
#endif
#ifdef CLOSURE_GLOSSY
        , out vec3 ssr_spec
#endif
        )
{
#ifdef CLOSURE_DIFFUSE
	out_diff = vec3(0.0);
#endif

#ifdef CLOSURE_SUBSURFACE
	out_trans = vec3(0.0);
#endif

#ifdef CLOSURE_GLOSSY
	out_spec = vec3(0.0);
#endif

#ifdef CLOSURE_REFRACTION
	out_refr = vec3(0.0);
#endif

	/* Zero length vectors cause issues, see: T51979. */
	float len = length(N);
	if (isnan(len)) {
		return;
	}
	N /= len;

#ifdef CLOSURE_CLEARCOAT
	len = length(C_N);
	if (isnan(len)) {
		return;
	}
	C_N /= len;
#endif

#if defined(CLOSURE_GLOSSY) || defined(CLOSURE_REFRACTION)
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;
#endif

#ifdef CLOSURE_CLEARCOAT
	C_roughness = clamp(C_roughness, 1e-8, 0.9999);
	float C_roughnessSquared = C_roughness * C_roughness;
#endif

	vec3 V = cameraVec;

	vec4 rand = texelFetch(utilTex, ivec3(ivec2(gl_FragCoord.xy) % LUT_SIZE, 2.0), 0);

	/* ---------------------------------------------------------------- */
	/* -------------------- SCENE LAMPS LIGHTING ---------------------- */
	/* ---------------------------------------------------------------- */

#ifdef CLOSURE_GLOSSY
	vec2 lut_uv = lut_coords(dot(N, V), roughness);
	vec4 ltc_mat = texture(utilTex, vec3(lut_uv, 0.0)).rgba;
#endif

#ifdef CLOSURE_CLEARCOAT
	vec2 lut_uv_clear = lut_coords(dot(C_N, V), C_roughness);
	vec4 ltc_mat_clear = texture(utilTex, vec3(lut_uv_clear, 0.0)).rgba;
	vec3 out_spec_clear = vec3(0.0);
#endif

	for (int i = 0; i < MAX_LIGHT && i < laNumLight; ++i) {
		LightData ld = lights_data[i];

		vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
		l_vector.xyz = ld.l_position - worldPosition;
		l_vector.w = length(l_vector.xyz);

		vec3 l_color_vis = ld.l_color * light_visibility(ld, worldPosition, viewPosition, viewNormal, l_vector);

	#ifdef CLOSURE_DIFFUSE
		out_diff += l_color_vis * light_diffuse(ld, N, V, l_vector);
	#endif

	#ifdef CLOSURE_SUBSURFACE
		out_trans += ld.l_color * light_translucent(ld, worldPosition, -N, l_vector, sss_scale);
	#endif

	#ifdef CLOSURE_GLOSSY
		out_spec += l_color_vis * light_specular(ld, ltc_mat, N, V, l_vector) * ld.l_spec;
	#endif

	#ifdef CLOSURE_CLEARCOAT
		out_spec_clear += l_color_vis * light_specular(ld, ltc_mat_clear, C_N, V, l_vector) * C_intensity * ld.l_spec;
	#endif
	}

#ifdef CLOSURE_GLOSSY
	vec3 brdf_lut_lamps = texture(utilTex, vec3(lut_uv, 1.0)).rgb;
	out_spec *= F_area(f0, brdf_lut_lamps.xy) * brdf_lut_lamps.z;
#endif

#ifdef CLOSURE_CLEARCOAT
	vec3 brdf_lut_lamps_clear = texture(utilTex, vec3(lut_uv_clear, 1.0)).rgb;
	out_spec_clear *= F_area(f0, brdf_lut_lamps_clear.xy) * brdf_lut_lamps_clear.z;
	out_spec += out_spec_clear;
#endif

	/* ---------------------------------------------------------------- */
	/* ---------------- SPECULAR ENVIRONMENT LIGHTING ----------------- */
	/* ---------------------------------------------------------------- */

	/* Accumulate incomming light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
#ifdef CLOSURE_GLOSSY
	vec4 spec_accum = vec4(0.0);
#endif

#ifdef CLOSURE_CLEARCOAT
	vec4 C_spec_accum = vec4(0.0);
#endif

#ifdef CLOSURE_REFRACTION
	vec4 refr_accum = vec4(0.0);
#endif

#ifdef CLOSURE_GLOSSY
	/* ---------------------------- */
	/*      Planar Reflections      */
	/* ---------------------------- */

	for (int i = 0; i < MAX_PLANAR && i < prbNumPlanar && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		/* Fade on geometric normal. */
		float fade = probe_attenuation_planar(pd, worldPosition, (gl_FrontFacing) ? worldNormal : -worldNormal, roughness);

		if (fade > 0.0) {
			if (!(ssrToggle && ssr_id == outputSsrId)) {
				vec3 spec = probe_evaluate_planar(float(i), pd, worldPosition, N, V, roughness, fade);
				accumulate_light(spec, fade, spec_accum);
			}

	#ifdef CLOSURE_CLEARCOAT
			vec3 C_spec = probe_evaluate_planar(float(i), pd, worldPosition, C_N, V, C_roughness, fade);
			accumulate_light(C_spec, fade, C_spec_accum);
	#endif

		}
	}
#endif


#ifdef CLOSURE_GLOSSY
	vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);
#endif

#ifdef CLOSURE_CLEARCOAT
	vec3 C_spec_dir = get_specular_reflection_dominant_dir(C_N, V, C_roughnessSquared);
#endif

#ifdef CLOSURE_REFRACTION
	/* Refract the view vector using the depth heuristic.
	 * Then later Refract a second time the already refracted
	 * ray using the inverse ior. */
	float final_ior = (refractionDepth > 0.0) ? 1.0 / ior : ior;
	vec3 refr_V = (refractionDepth > 0.0) ? -refract(-V, N, final_ior) : V;
	vec3 refr_pos = (refractionDepth > 0.0) ? line_plane_intersect(worldPosition, refr_V, worldPosition - N * refractionDepth, N) : worldPosition;
	vec3 refr_dir = get_specular_refraction_dominant_dir(N, refr_V, roughness, final_ior);
#endif


#ifdef CLOSURE_REFRACTION
	/* ---------------------------- */
	/*   Screen Space Refraction    */
	/* ---------------------------- */
	#ifdef USE_REFRACTION
	if (ssrToggle && roughness < ssrMaxRoughness + 0.2) {
		/* Find approximated position of the 2nd refraction event. */
		vec3 refr_vpos = (refractionDepth > 0.0) ? transform_point(ViewMatrix, refr_pos) : viewPosition;
		vec4 trans = screen_space_refraction(refr_vpos, N, refr_V, final_ior, roughnessSquared, rand);
		trans.a *= smoothstep(ssrMaxRoughness + 0.2, ssrMaxRoughness, roughness);
		accumulate_light(trans.rgb, trans.a, refr_accum);
	}
	#endif

#endif


	/* ---------------------------- */
	/*       Specular probes        */
	/* ---------------------------- */
#if defined(CLOSURE_GLOSSY) || defined(CLOSURE_REFRACTION)

	#ifdef CLOSURE_REFRACTION
		#define ACCUM refr_accum
	#else
		#define ACCUM spec_accum
	#endif

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; ACCUM.a < 0.999 && i < prbNumRenderCube && i < MAX_PROBE; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, worldPosition);

		if (fade > 0.0) {

	#ifdef CLOSURE_GLOSSY
			if (!(ssrToggle && ssr_id == outputSsrId)) {
				vec3 spec = probe_evaluate_cube(float(i), cd, worldPosition, spec_dir, roughness);
				accumulate_light(spec, fade, spec_accum);
			}
	#endif

	#ifdef CLOSURE_CLEARCOAT
			vec3 C_spec = probe_evaluate_cube(float(i), cd, worldPosition, C_spec_dir, C_roughness);
			accumulate_light(C_spec, fade, C_spec_accum);
	#endif

	#ifdef CLOSURE_REFRACTION
			vec3 trans = probe_evaluate_cube(float(i), cd, refr_pos, refr_dir, roughnessSquared);
			accumulate_light(trans, fade, refr_accum);
	#endif
		}
	}

	#undef ACCUM

	/* ---------------------------- */
	/*          World Probe         */
	/* ---------------------------- */
	#ifdef CLOSURE_GLOSSY
	if (spec_accum.a < 0.999) {
		if (!(ssrToggle && ssr_id == outputSsrId)) {
			vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
			accumulate_light(spec, 1.0, spec_accum);
		}

		#ifdef CLOSURE_CLEARCOAT
		vec3 C_spec = probe_evaluate_world_spec(C_spec_dir, C_roughness);
		accumulate_light(C_spec, 1.0, C_spec_accum);
		#endif

	}
	#endif

	#ifdef CLOSURE_REFRACTION
	if (refr_accum.a < 0.999) {
		vec3 trans = probe_evaluate_world_spec(refr_dir, roughnessSquared);
		accumulate_light(trans, 1.0, refr_accum);
	}
	#endif
#endif /* Specular probes */


	/* ---------------------------- */
	/*       Ambient Occlusion      */
	/* ---------------------------- */
#if defined(CLOSURE_GLOSSY) || defined(CLOSURE_DIFFUSE)
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, ao, rand, bent_normal);
#endif


	/* ---------------------------- */
	/*        Specular Output       */
	/* ---------------------------- */
	float NV = dot(N, V);
#ifdef CLOSURE_GLOSSY
	vec2 uv = lut_coords(NV, roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	/* This factor is outputed to be used by SSR in order
	 * to match the intensity of the regular reflections. */
	ssr_spec = F_ibl(f0, brdf_lut);
	float spec_occlu = specular_occlusion(NV, final_ao, roughness);

	/* The SSR pass recompute the occlusion to not apply it to the SSR */
	if (ssrToggle && ssr_id == outputSsrId) {
		spec_occlu = 1.0;
	}

	out_spec += spec_accum.rgb * ssr_spec * spec_occlu;
#endif

#ifdef CLOSURE_REFRACTION
	float btdf = get_btdf_lut(utilTex, NV, roughness, ior);

	out_refr += refr_accum.rgb * btdf;
#endif

#ifdef CLOSURE_CLEARCOAT
	NV = dot(C_N, V);
	vec2 C_uv = lut_coords(NV, C_roughness);
	vec2 C_brdf_lut = texture(utilTex, vec3(C_uv, 1.0)).rg;
	vec3 C_fresnel = F_ibl(vec3(0.04), brdf_lut) * specular_occlusion(NV, final_ao, C_roughness);

	out_spec += C_spec_accum.rgb * C_fresnel * C_intensity;
#endif

#ifdef CLOSURE_GLOSSY
	/* Global toggle for lightprobe baking. */
	out_spec *= float(specToggle);
#endif

	/* ---------------------------------------------------------------- */
	/* ---------------- DIFFUSE ENVIRONMENT LIGHTING ------------------ */
	/* ---------------------------------------------------------------- */

	/* Accumulate light from all sources until accumulator is full. Then apply Occlusion and BRDF. */
#ifdef CLOSURE_DIFFUSE
	vec4 diff_accum = vec4(0.0);

	/* ---------------------------- */
	/*       Irradiance Grids       */
	/* ---------------------------- */
	/* Start at 1 because 0 is world irradiance */
	for (int i = 1; i < MAX_GRID && i < prbNumRenderGrid && diff_accum.a < 0.999; ++i) {
		GridData gd = grids_data[i];

		vec3 localpos;
		float fade = probe_attenuation_grid(gd, worldPosition, localpos);

		if (fade > 0.0) {
			vec3 diff = probe_evaluate_grid(gd, worldPosition, bent_normal, localpos);
			accumulate_light(diff, fade, diff_accum);
		}
	}

	/* ---------------------------- */
	/*        World Diffuse         */
	/* ---------------------------- */
	if (diff_accum.a < 0.999 && prbNumRenderGrid > 0) {
		vec3 diff = probe_evaluate_world_diff(bent_normal);
		accumulate_light(diff, 1.0, diff_accum);
	}

	out_diff += diff_accum.rgb * gtao_multibounce(final_ao, albedo);
#endif
}

/* Cleanup for next configuration */
#undef CLOSURE_NAME

#ifdef CLOSURE_DIFFUSE
	#undef CLOSURE_DIFFUSE
#endif

#ifdef CLOSURE_GLOSSY
	#undef CLOSURE_GLOSSY
#endif

#ifdef CLOSURE_CLEARCOAT
	#undef CLOSURE_CLEARCOAT
#endif

#ifdef CLOSURE_REFRACTION
	#undef CLOSURE_REFRACTION
#endif

#ifdef CLOSURE_SUBSURFACE
	#undef CLOSURE_SUBSURFACE
#endif
