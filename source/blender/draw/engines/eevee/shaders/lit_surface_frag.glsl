
uniform int light_count;
uniform int probe_count;
uniform int grid_count;
uniform int planar_count;
uniform mat4 ViewMatrix;
uniform mat4 ViewMatrixInverse;

uniform sampler2DArray probePlanars;

uniform sampler2DArray probeCubes;
uniform float lodMax;
uniform bool specToggle;

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

uniform sampler2DArray shadowCubes;
uniform sampler2DArrayShadow shadowCascades;

layout(std140) uniform probe_block {
	ProbeData probes_data[MAX_PROBE];
};

layout(std140) uniform grid_block {
	GridData grids_data[MAX_GRID];
};

layout(std140) uniform planar_block {
	PlanarData planars_data[MAX_PLANAR];
};

layout(std140) uniform light_block {
	LightData lights_data[MAX_LIGHT];
};

layout(std140) uniform shadow_block {
	ShadowCubeData    shadows_cube_data[MAX_SHADOW_CUBE];
	ShadowMapData     shadows_map_data[MAX_SHADOW_MAP];
	ShadowCascadeData shadows_cascade_data[MAX_SHADOW_CASCADE];
};

in vec3 worldPosition;
in vec3 viewPosition;

#ifdef USE_FLAT_NORMAL
flat in vec3 worldNormal;
flat in vec3 viewNormal;
#else
in vec3 worldNormal;
in vec3 viewNormal;
#endif

#define cameraForward   normalize(ViewMatrixInverse[2].xyz)
#define cameraPos       ViewMatrixInverse[3].xyz

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

#ifdef HAIR_SHADER
vec3 light_diffuse(LightData ld, ShadingData sd, vec3 albedo)
{
       if (ld.l_type == SUN) {
               return direct_diffuse_sun(ld, sd) * albedo;
       }
       else if (ld.l_type == AREA) {
               return direct_diffuse_rectangle(ld, sd) * albedo;
       }
       else {
               return direct_diffuse_sphere(ld, sd) * albedo;
       }
}

vec3 light_specular(LightData ld, ShadingData sd, float roughness, vec3 f0)
{
       if (ld.l_type == SUN) {
               return direct_ggx_sun(ld, sd, roughness, f0);
       }
       else if (ld.l_type == AREA) {
               return direct_ggx_rectangle(ld, sd, roughness, f0);
       }
       else {
               return direct_ggx_sphere(ld, sd, roughness, f0);
       }
}

void light_shade(
        LightData ld, ShadingData sd, vec3 albedo, float roughness, vec3 f0,
        out vec3 diffuse, out vec3 specular)
{
       const float transmission = 0.3; /* Uniform internal scattering factor */
       ShadingData sd_new = sd;

       vec3 lamp_vec;

      if (ld.l_type == SUN || ld.l_type == AREA) {
               lamp_vec = ld.l_forward;
       }
       else {
               lamp_vec = -sd.l_vector;
       }

       vec3 norm_view = cross(sd.V, sd.N);
       norm_view = normalize(cross(norm_view, sd.N)); /* Normal facing view */

       vec3 norm_lamp = cross(lamp_vec, sd.N);
       norm_lamp = normalize(cross(sd.N, norm_lamp)); /* Normal facing lamp */

       /* Rotate view vector onto the cross(tangent, light) plane */
       vec3 view_vec = normalize(norm_lamp * dot(norm_view, sd.V) + sd.N * dot(sd.N, sd.V));

       float occlusion = (dot(norm_view, norm_lamp) * 0.5 + 0.5);
       float occltrans = transmission + (occlusion * (1.0 - transmission)); /* Includes transmission component */

       sd_new.N = -norm_lamp;

       diffuse = light_diffuse(ld, sd_new, albedo) * occltrans;

       sd_new.V = view_vec;

       specular = light_specular(ld, sd_new, roughness, f0) * occlusion;
}
#else
void light_shade(
        LightData ld, ShadingData sd, vec3 albedo, float roughness, vec3 f0,
        out vec3 diffuse, out vec3 specular)
{
#ifdef USE_LTC
	if (ld.l_type == SUN) {
		/* TODO disk area light */
		diffuse = direct_diffuse_sun(ld, sd) * albedo;
		specular = direct_ggx_sun(ld, sd, roughness, f0);
	}
	else if (ld.l_type == AREA) {
		diffuse =  direct_diffuse_rectangle(ld, sd) * albedo;
		specular =  direct_ggx_rectangle(ld, sd, roughness, f0);
	}
	else {
		diffuse =  direct_diffuse_sphere(ld, sd) * albedo;
		specular =  direct_ggx_sphere(ld, sd, roughness, f0);
	}
#else
	if (ld.l_type == SUN) {
		diffuse = direct_diffuse_sun(ld, sd) * albedo;
		specular = direct_ggx_sun(ld, sd, roughness, f0);
	}
	else {
		diffuse = direct_diffuse_point(ld, sd) * albedo;
		specular = direct_ggx_point(sd, roughness, f0);
	}
#endif

	specular *= float(specToggle);
}
#endif

void light_visibility(LightData ld, ShadingData sd, out float vis)
{
	vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, sd.l_vector);
		vec3 lL = sd.l_vector / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
		vis *= step(0.0, -dot(sd.l_vector, ld.l_forward));
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(sd.l_vector, ld.l_forward));
	}

	/* shadowing */
	if (ld.l_shadowid >= (MAX_SHADOW_MAP + MAX_SHADOW_CUBE)) {
		/* Shadow Cascade */
		float shid = ld.l_shadowid - (MAX_SHADOW_CUBE + MAX_SHADOW_MAP);
		ShadowCascadeData smd = shadows_cascade_data[int(shid)];

		/* Finding Cascade index */
		vec4 z = vec4(-dot(cameraPos - worldPosition, cameraForward));
		vec4 comp = step(z, smd.split_distances);
		float cascade = dot(comp, comp);
		mat4 shadowmat;
		float bias;

		/* Manual Unrolling of a loop for better performance.
		 * Doing fetch directly with cascade index leads to
		 * major performance impact. (0.27ms -> 10.0ms for 1 light) */
		if (cascade == 0.0) {
			shadowmat = smd.shadowmat[0];
			bias = smd.bias[0];
		}
		else if (cascade == 1.0) {
			shadowmat = smd.shadowmat[1];
			bias = smd.bias[1];
		}
		else if (cascade == 2.0) {
			shadowmat = smd.shadowmat[2];
			bias = smd.bias[2];
		}
		else {
			shadowmat = smd.shadowmat[3];
			bias = smd.bias[3];
		}

		vec4 shpos = shadowmat * vec4(sd.W, 1.0);
		shpos.z -= bias * shpos.w;
		shpos.xyz /= shpos.w;

		vis *= texture(shadowCascades, vec4(shpos.xy, shid * float(MAX_CASCADE_NUM) + cascade, shpos.z));
	}
	else if (ld.l_shadowid >= 0.0) {
		/* Shadow Cube */
		float shid = ld.l_shadowid;
		ShadowCubeData scd = shadows_cube_data[int(shid)];

		vec3 cubevec = sd.W - ld.l_position;
		float dist = length(cubevec) - scd.sh_cube_bias;

		float z = texture_octahedron(shadowCubes, vec4(cubevec, shid)).r;

		float esm_test = saturate(exp(scd.sh_cube_exp * (z - dist)));
		float sh_test = step(0, z - dist);

		vis *= esm_test;
	}
}

vec3 probe_parallax_correction(vec3 W, vec3 spec_dir, ProbeData pd, inout float roughness)
{
	vec3 localpos = (pd.parallaxmat * vec4(W, 1.0)).xyz;
	vec3 localray = (pd.parallaxmat * vec4(spec_dir, 0.0)).xyz;

	float dist;
	if (pd.p_parallax_type == PROBE_PARALLAX_BOX) {
		dist = line_unit_box_intersect_dist(localpos, localray);
	}
	else {
		dist = line_unit_sphere_intersect_dist(localpos, localray);
	}

	/* Use Distance in WS directly to recover intersection */
	vec3 intersection = W + spec_dir * dist - pd.p_position;

	/* From Frostbite PBR Course
	 * Distance based roughness
	 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
	float original_roughness = roughness;
	float linear_roughness = sqrt(roughness);
	float distance_roughness = saturate(dist * linear_roughness / length(intersection));
	linear_roughness = mix(distance_roughness, linear_roughness, linear_roughness);
	roughness = linear_roughness * linear_roughness;

	float fac = saturate(original_roughness * 2.0 - 1.0);
	return mix(intersection, spec_dir, fac * fac);
}

float probe_attenuation(vec3 W, ProbeData pd)
{
	vec3 localpos = (pd.influencemat * vec4(W, 1.0)).xyz;

	float fac;
	if (pd.p_atten_type == PROBE_ATTENUATION_BOX) {
		vec3 axes_fac = saturate(pd.p_atten_fac - pd.p_atten_fac * abs(localpos));
		fac = min_v3(axes_fac);
	}
	else {
		fac = saturate(pd.p_atten_fac - pd.p_atten_fac * length(localpos));
	}

	return fac;
}

float planar_attenuation(vec3 W, vec3 N, PlanarData pd)
{
	float fac;

	/* Normal Facing */
	fac = saturate(dot(pd.pl_normal, N) * pd.pl_facing_scale + pd.pl_facing_bias);

	/* Distance from plane */
	fac *= saturate(abs(dot(pd.pl_plane_eq, vec4(W, 1.0))) * pd.pl_fade_scale + pd.pl_fade_bias);

	/* Fancy fast clipping calculation */
	vec2 dist_to_clip;
	dist_to_clip.x = dot(pd.pl_clip_pos_x, W);
	dist_to_clip.y = dot(pd.pl_clip_pos_y, W);
	fac *= step(2.0, dot(step(pd.pl_clip_edges, dist_to_clip.xxyy), vec2(-1.0, 1.0).xyxy)); /* compare and add all tests */

	return fac;
}

float compute_occlusion(vec3 N, float micro_occlusion, vec2 randuv, out vec3 bent_normal)
{
#ifdef USE_AO /* Screen Space Occlusion */

	float macro_occlusion;
	vec3 vnor = mat3(ViewMatrix) * N;

#ifdef USE_BENT_NORMAL
	gtao(vnor, viewPosition, randuv, macro_occlusion, bent_normal);
	bent_normal = mat3(ViewMatrixInverse) * bent_normal;
#else
	gtao(vnor, viewPosition, randuv, macro_occlusion);
	bent_normal = N;
#endif
	return min(macro_occlusion, micro_occlusion);

#else /* No added Occlusion. */

	bent_normal = N;
	return micro_occlusion;

#endif
}

vec3 eevee_surface_lit(vec3 world_normal, vec3 albedo, vec3 f0, float roughness, float ao)
{
	roughness = clamp(roughness, 1e-8, 0.9999);
	float roughnessSquared = roughness * roughness;

	ShadingData sd;
	sd.N = normalize(world_normal);
	sd.V = (ProjectionMatrix[3][3] == 0.0) /* if perspective */
	            ? normalize(cameraPos - worldPosition)
	            : cameraForward;
	sd.W = worldPosition;

	vec3 radiance = vec3(0.0);

#ifdef HAIR_SHADER
       /* View facing normal */
       vec3 norm_view = cross(sd.V, sd.N);
       norm_view = normalize(cross(norm_view, sd.N)); /* Normal facing view */
#endif


	/* Analytic Lights */
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];
		vec3 diff, spec;
		float vis = 1.0;

		sd.l_vector = ld.l_position - worldPosition;
		light_visibility(ld, sd, vis);
		light_shade(ld, sd, albedo, roughnessSquared, f0, diff, spec);

		radiance += vis * (diff + spec) * ld.l_color;
	}

#ifdef HAIR_SHADER
	sd.N = -norm_view;
#endif

	vec3 bent_normal;
	vec4 rand = textureLod(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0), 0.0).rgba;
	float final_ao = compute_occlusion(sd.N, ao, rand.rg, bent_normal);

	/* Envmaps */
	vec3 R = reflect(-sd.V, sd.N);
	vec3 spec_dir = get_specular_dominant_dir(sd.N, R, roughnessSquared);
	vec2 uv = lut_coords(dot(sd.N, sd.V), roughness);
	vec2 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rg;

	vec4 spec_accum = vec4(0.0);
	vec4 diff_accum = vec4(0.0);

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count && spec_accum.a < 0.999; ++i) {
		PlanarData pd = planars_data[i];

		float influence = planar_attenuation(sd.W, sd.N, pd);

		if (influence > 0.0) {
			float influ_spec = min(influence, (1.0 - spec_accum.a));

			/* Sample reflection depth. */
			vec4 refco = pd.reflectionmat * vec4(sd.W, 1.0);
			refco.xy /= refco.w;
			float ref_depth = textureLod(probePlanars, vec3(refco.xy, i), 0.0).a;

			/* Find view vector / reflection plane intersection. (dist_to_plane is negative) */
			float dist_to_plane = line_plane_intersect_dist(cameraPos, sd.V, pd.pl_plane_eq);
			vec3 point_on_plane = cameraPos + sd.V * dist_to_plane;

			/* How far the pixel is from the plane. */
			ref_depth = ref_depth + dist_to_plane;

			/* Compute distorded reflection vector based on the distance to the reflected object.
			 * In other words find intersection between reflection vector and the sphere center
			 * around point_on_plane. */
			vec3 proj_ref = reflect(R * ref_depth, pd.pl_normal);

			/* Final point in world space. */
			vec3 ref_pos = point_on_plane + proj_ref;

			/* Reproject to find texture coords. */
			refco = pd.reflectionmat * vec4(ref_pos, 1.0);
			refco.xy /= refco.w;

			/* Distance to roughness */
			float linear_roughness = sqrt(roughness);
			float distance_roughness = min(linear_roughness, ref_depth * linear_roughness);
			linear_roughness = mix(distance_roughness, linear_roughness, linear_roughness);

			/* Decrease influence for high roughness */
			influ_spec *= saturate((1.0 - linear_roughness) * 5.0 - 2.0);

			float lod = linear_roughness * 2.5 * 5.0;
			vec3 sample = textureLod(probePlanars, vec3(refco.xy, i), lod).rgb;

			/* Use a second sample randomly rotated to blur out the lowres aspect */
			vec2 rot_sample = (1.0 / vec2(textureSize(probePlanars, 0).xy)) * vec2(cos(rand.a * M_2PI), sin(rand.a * M_2PI)) * lod;
			sample += textureLod(probePlanars, vec3(refco.xy + rot_sample, i), lod).rgb;
			sample *= 0.5;

			spec_accum.rgb += sample * influ_spec;
			spec_accum.a += influ_spec;
		}
	}

	/* Specular probes */
	/* Start at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
		ProbeData pd = probes_data[i];

		float dist_attenuation = probe_attenuation(sd.W, pd);

		if (dist_attenuation > 0.0) {
			float roughness_copy = roughness;

			vec3 sample_vec = probe_parallax_correction(sd.W, spec_dir, pd, roughness_copy);
			vec4 sample = textureLod_octahedron(probeCubes, vec4(sample_vec, i), roughness_copy * lodMax, lodMax).rgba;

			float influ_spec = min(dist_attenuation, (1.0 - spec_accum.a));

			spec_accum.rgb += sample.rgb * influ_spec;
			spec_accum.a += influ_spec;
		}
	}

	/* Start at 1 because 0 is world irradiance */
	for (int i = 1; i < MAX_GRID && i < grid_count && diff_accum.a < 0.999; ++i) {
		GridData gd = grids_data[i];

		vec3 localpos = (gd.localmat * vec4(sd.W, 1.0)).xyz;

		float fade = min(1.0, min_v3(1.0 - abs(localpos)));
		fade = saturate(fade * gd.g_atten_scale + gd.g_atten_bias);

		if (fade > 0.0) {
			localpos = localpos * 0.5 + 0.5;
			localpos = localpos * vec3(gd.g_resolution) - 0.5;

			vec3 localpos_floored = floor(localpos);
			vec3 trilinear_weight = fract(localpos);

			float weight_accum = 0.0;
			vec3 irradiance_accum = vec3(0.0);

			/* For each neighboor cells */
			for (int i = 0; i < 8; ++i) {
				ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
				vec3 cell_cos = clamp(localpos_floored + vec3(offset), vec3(0.0), vec3(gd.g_resolution) - 1.0);

				/* We need this because we render probes in world space (so we need light vector in WS).
				 * And rendering them in local probe space is too much problem. */
				vec3 ws_cell_location = gd.g_corner +
					(gd.g_increment_x * cell_cos.x +
					 gd.g_increment_y * cell_cos.y +
					 gd.g_increment_z * cell_cos.z);
				vec3 ws_point_to_cell = ws_cell_location - sd.W;
				vec3 ws_light = normalize(ws_point_to_cell);

				vec3 trilinear = mix(1 - trilinear_weight, trilinear_weight, offset);
				float weight = trilinear.x * trilinear.y * trilinear.z;

				/* Smooth backface test */
				// weight *= sqrt(max(0.002, dot(ws_light, sd.N)));

				/* Avoid zero weight */
				weight = max(0.00001, weight);

				vec3 color = get_cell_color(ivec3(cell_cos), gd.g_resolution, gd.g_offset, bent_normal);

				weight_accum += weight;
				irradiance_accum += color * weight;
			}

			vec3 indirect_diffuse = irradiance_accum / weight_accum;

			float influ_diff = min(fade, (1.0 - diff_accum.a));

			diff_accum.rgb += indirect_diffuse * influ_diff;
			diff_accum.a += influ_diff;

			/* For Debug purpose */
			// return texture(irradianceGrid, sd.W.xy).rgb;
		}
	}

	/* World probe */
	if (diff_accum.a < 1.0 && grid_count > 0) {
		IrradianceData ir_data = load_irradiance_cell(0, bent_normal);

		vec3 diff = compute_irradiance(bent_normal, ir_data);
		diff_accum.rgb += diff * (1.0 - diff_accum.a);
	}

	if (spec_accum.a < 1.0) {
		ProbeData pd = probes_data[0];

		vec3 spec = textureLod_octahedron(probeCubes, vec4(spec_dir, 0), roughness * lodMax, lodMax).rgb;
		spec_accum.rgb += spec * (1.0 - spec_accum.a);
	}

	vec3 indirect_radiance =
	        spec_accum.rgb * F_ibl(f0, brdf_lut) * float(specToggle) * specular_occlusion(dot(sd.N, sd.V), final_ao, roughness) +
	        diff_accum.rgb * albedo * gtao_multibounce(final_ao, albedo);

	return radiance + indirect_radiance;
}
