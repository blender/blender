/* ----------- Uniforms --------- */

uniform sampler2DArray probePlanars;
uniform sampler2DArray probeCubes;

/* ----------- Structures --------- */

struct CubeData {
	vec4 position_type;
	vec4 attenuation_fac_type;
	mat4 influencemat;
	mat4 parallaxmat;
};

#define PROBE_PARALLAX_BOX    1.0
#define PROBE_ATTENUATION_BOX 1.0

#define p_position      position_type.xyz
#define p_parallax_type position_type.w
#define p_atten_fac     attenuation_fac_type.x
#define p_atten_type    attenuation_fac_type.y

struct PlanarData {
	vec4 plane_equation;
	vec4 clip_vec_x_fade_scale;
	vec4 clip_vec_y_fade_bias;
	vec4 clip_edges;
	vec4 facing_scale_bias;
	mat4 reflectionmat; /* transform world space into reflection texture space */
	mat4 unused;
};

#define pl_plane_eq      plane_equation
#define pl_normal        plane_equation.xyz
#define pl_facing_scale  facing_scale_bias.x
#define pl_facing_bias   facing_scale_bias.y
#define pl_fade_scale    clip_vec_x_fade_scale.w
#define pl_fade_bias     clip_vec_y_fade_bias.w
#define pl_clip_pos_x    clip_vec_x_fade_scale.xyz
#define pl_clip_pos_y    clip_vec_y_fade_bias.xyz
#define pl_clip_edges    clip_edges

struct GridData {
	mat4 localmat;
	ivec4 resolution_offset;
	vec4 ws_corner_atten_scale; /* world space corner position */
	vec4 ws_increment_x_atten_bias; /* world space vector between 2 opposite cells */
	vec4 ws_increment_y_lvl_bias;
	vec4 ws_increment_z;
	vec4 vis_bias_bleed_range;
};

#define g_corner        ws_corner_atten_scale.xyz
#define g_atten_scale   ws_corner_atten_scale.w
#define g_atten_bias    ws_increment_x_atten_bias.w
#define g_level_bias    ws_increment_y_lvl_bias.w
#define g_increment_x   ws_increment_x_atten_bias.xyz
#define g_increment_y   ws_increment_y_lvl_bias.xyz
#define g_increment_z   ws_increment_z.xyz
#define g_resolution    resolution_offset.xyz
#define g_offset        resolution_offset.w
#define g_vis_bias      vis_bias_bleed_range.x
#define g_vis_bleed     vis_bias_bleed_range.y
#define g_vis_range     vis_bias_bleed_range.z

#ifndef MAX_PROBE
#define MAX_PROBE 1
#endif
#ifndef MAX_GRID
#define MAX_GRID 1
#endif
#ifndef MAX_PLANAR
#define MAX_PLANAR 1
#endif

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)
#endif /* UTIL_TEX */

layout(std140) uniform probe_block {
	CubeData probes_data[MAX_PROBE];
};

layout(std140) uniform grid_block {
	GridData grids_data[MAX_GRID];
};

layout(std140) uniform planar_block {
	PlanarData planars_data[MAX_PLANAR];
};

/* ----------- Functions --------- */

float probe_attenuation_cube(CubeData pd, vec3 W)
{
	vec3 localpos = transform_point(pd.influencemat, W);

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

float probe_attenuation_planar(PlanarData pd, vec3 W, vec3 N, float roughness)
{
	/* Normal Facing */
	float fac = saturate(dot(pd.pl_normal, N) * pd.pl_facing_scale + pd.pl_facing_bias);

	/* Distance from plane */
	fac *= saturate(abs(dot(pd.pl_plane_eq, vec4(W, 1.0))) * pd.pl_fade_scale + pd.pl_fade_bias);

	/* Fancy fast clipping calculation */
	vec2 dist_to_clip;
	dist_to_clip.x = dot(pd.pl_clip_pos_x, W);
	dist_to_clip.y = dot(pd.pl_clip_pos_y, W);
	/* compare and add all tests */
	fac *= step(2.0, dot(step(pd.pl_clip_edges, dist_to_clip.xxyy), vec2(-1.0, 1.0).xyxy));

	/* Decrease influence for high roughness */
	fac *= saturate(1.0 - roughness * 10.0);

	return fac;
}

float probe_attenuation_grid(GridData gd, vec3 W, out vec3 localpos)
{
	localpos = transform_point(gd.localmat, W);

	float fade = min(1.0, min_v3(1.0 - abs(localpos)));
	return saturate(fade * gd.g_atten_scale + gd.g_atten_bias);
}

vec3 probe_evaluate_cube(float id, CubeData cd, vec3 W, vec3 R, float roughness)
{
	/* Correct reflection ray using parallax volume intersection. */
	vec3 localpos = transform_point(cd.parallaxmat, W);
	vec3 localray = transform_direction(cd.parallaxmat, R);

	float dist;
	if (cd.p_parallax_type == PROBE_PARALLAX_BOX) {
		dist = line_unit_box_intersect_dist(localpos, localray);
	}
	else {
		dist = line_unit_sphere_intersect_dist(localpos, localray);
	}

	/* Use Distance in WS directly to recover intersection */
	vec3 intersection = W + R * dist - cd.p_position;

	/* From Frostbite PBR Course
	 * Distance based roughness
	 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
	float original_roughness = roughness;
	float linear_roughness = sqrt(roughness);
	float distance_roughness = saturate(dist * linear_roughness / length(intersection));
	linear_roughness = mix(distance_roughness, linear_roughness, linear_roughness);
	roughness = linear_roughness * linear_roughness;

	float fac = saturate(original_roughness * 2.0 - 1.0);
	R = mix(intersection, R, fac * fac);

	return textureLod_octahedron(probeCubes, vec4(R, id), roughness * prbLodCubeMax, prbLodCubeMax).rgb;
}

vec3 probe_evaluate_world_spec(vec3 R, float roughness)
{
	return textureLod_octahedron(probeCubes, vec4(R, 0.0), roughness * prbLodCubeMax, prbLodCubeMax).rgb;
}

vec3 probe_evaluate_planar(
        float id, PlanarData pd, vec3 W, vec3 N, vec3 V,
        float roughness, inout float fade)
{
	/* Find view vector / reflection plane intersection. */
	vec3 point_on_plane = line_plane_intersect(W, V, pd.pl_plane_eq);

	/* How far the pixel is from the plane. */
	float ref_depth = 1.0; /* TODO parameter */

	/* Compute distorded reflection vector based on the distance to the reflected object.
	 * In other words find intersection between reflection vector and the sphere center
	 * around point_on_plane. */
	vec3 proj_ref = reflect(reflect(-V, N) * ref_depth, pd.pl_normal);

	/* Final point in world space. */
	vec3 ref_pos = point_on_plane + proj_ref;

	/* Reproject to find texture coords. */
	vec4 refco = ViewProjectionMatrix * vec4(ref_pos, 1.0);
	refco.xy /= refco.w;

	/* TODO: If we support non-ssr planar reflection, we should blur them with gaussian
	 * and chose the right mip depending on the cone footprint after projection */
	vec3 sample = textureLod(probePlanars, vec3(refco.xy * 0.5 + 0.5, id), 0.0).rgb;

	return sample;
}

void fallback_cubemap(
        vec3 N, vec3 V, vec3 W, vec3 viewPosition, float roughness, float roughnessSquared, inout vec4 spec_accum)
{
	/* Specular probes */
	vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);

	vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
	vec3 bent_normal;
#ifdef SSR_AO
	float final_ao = occlusion_compute(N, viewPosition, 1.0, rand, bent_normal);
	final_ao = specular_occlusion(dot(N, V), final_ao, roughness);
#else
	const float final_ao = 1.0;
#endif

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < prbNumRenderCube && spec_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, W);

		if (fade > 0.0) {
			vec3 spec = final_ao * probe_evaluate_cube(float(i), cd, W, spec_dir, roughness);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		vec3 spec = final_ao * probe_evaluate_world_spec(spec_dir, roughness);
		accumulate_light(spec, 1.0, spec_accum);
	}
}

#ifdef IRRADIANCE_LIB
vec3 probe_evaluate_grid(GridData gd, vec3 W, vec3 N, vec3 localpos)
{
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

		/* Keep in sync with update_irradiance_probe */
		ivec3 icell_cos = ivec3(gd.g_level_bias * floor(cell_cos / gd.g_level_bias));
		int cell = gd.g_offset + icell_cos.z
		                       + icell_cos.y * gd.g_resolution.z
		                       + icell_cos.x * gd.g_resolution.z * gd.g_resolution.y;

		vec3 color = irradiance_from_cell_get(cell, N);

		/* We need this because we render probes in world space (so we need light vector in WS).
		 * And rendering them in local probe space is too much problem. */
		vec3 ws_cell_location = gd.g_corner +
			(gd.g_increment_x * cell_cos.x +
			 gd.g_increment_y * cell_cos.y +
			 gd.g_increment_z * cell_cos.z);

		vec3 ws_point_to_cell = ws_cell_location - W;
		float ws_dist_point_to_cell = length(ws_point_to_cell);
		vec3 ws_light = ws_point_to_cell / ws_dist_point_to_cell;

		vec3 trilinear = mix(1 - trilinear_weight, trilinear_weight, offset);
		float weight = trilinear.x * trilinear.y * trilinear.z;

		/* Precomputed visibility */
		weight *= load_visibility_cell(cell, ws_light, ws_dist_point_to_cell, gd.g_vis_bias, gd.g_vis_bleed, gd.g_vis_range);

		/* Smooth backface test */
		weight *= sqrt(max(0.002, dot(ws_light, N)));

		/* Avoid zero weight */
		weight = max(0.00001, weight);

		weight_accum += weight;
		irradiance_accum += color * weight;
	}

	return irradiance_accum / weight_accum;
}

vec3 probe_evaluate_world_diff(vec3 N)
{
	return irradiance_from_cell_get(0, N);
}

#endif /* IRRADIANCE_LIB */
