/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(engine_eevee_legacy_shared.h)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(common_uniforms_lib.glsl)
#pragma BLENDER_REQUIRE(ambient_occlusion_lib.glsl)
#pragma BLENDER_REQUIRE(irradiance_lib.glsl)

/* ----------- Uniforms --------- */

#if !defined(USE_GPU_SHADER_CREATE_INFO)

uniform sampler2DArray probePlanars;
uniform samplerCubeArray probeCubes;

#endif

/* ----------- Structures --------- */

#define PROBE_PARALLAX_BOX 1.0
#define PROBE_ATTENUATION_BOX 1.0

#define p_position position_type.xyz
#define p_parallax_type position_type.w
#define p_atten_fac attenuation_fac_type.x
#define p_atten_type attenuation_fac_type.y

#define pl_plane_eq plane_equation
#define pl_normal plane_equation.xyz
#define pl_facing_scale facing_scale_bias.x
#define pl_facing_bias facing_scale_bias.y
#define pl_fade_scale clip_vec_x_fade_scale.w
#define pl_fade_bias clip_vec_y_fade_bias.w
#define pl_clip_pos_x clip_vec_x_fade_scale.xyz
#define pl_clip_pos_y clip_vec_y_fade_bias.xyz
#define pl_clip_edges clip_edges

#define g_corner ws_corner_atten_scale.xyz
#define g_atten_scale ws_corner_atten_scale.w
#define g_atten_bias ws_increment_x_atten_bias.w
#define g_level_bias ws_increment_y_lvl_bias.w
#define g_increment_x ws_increment_x_atten_bias.xyz
#define g_increment_y ws_increment_y_lvl_bias.xyz
#define g_increment_z ws_increment_z.xyz
#define g_resolution resolution_offset.xyz
#define g_offset resolution_offset.w
#define g_vis_bias vis_bias_bleed_range.x
#define g_vis_bleed vis_bias_bleed_range.y
#define g_vis_range vis_bias_bleed_range.z

#ifndef MAX_PROBE
#  define MAX_PROBE 1
#endif
#ifndef MAX_GRID
#  define MAX_GRID 1
#endif
#ifndef MAX_PLANAR
#  define MAX_PLANAR 1
#endif

#if !defined(USE_GPU_SHADER_CREATE_INFO)

layout(std140) uniform probe_block
{
  ProbeBlock _probe_block;
};

layout(std140) uniform grid_block
{
  GridBlock _grid_block;
};

layout(std140) uniform planar_block
{
  PlanarBlock _planar_block;
};

#  define probes_data _probe_block.probes_data
#  define grids_data _grid_block.grids_data
#  define planars_data _planar_block.planars_data

#endif
/* ----------- Functions --------- */

float probe_attenuation_cube(int pd_id, vec3 P)
{
  vec3 localpos = transform_point(probes_data[pd_id].influencemat, P);

  float probe_atten_fac = probes_data[pd_id].p_atten_fac;
  float fac;
  if (probes_data[pd_id].p_atten_type == PROBE_ATTENUATION_BOX) {
    vec3 axes_fac = saturate(probe_atten_fac - probe_atten_fac * abs(localpos));
    fac = min_v3(axes_fac);
  }
  else {
    fac = saturate(probe_atten_fac - probe_atten_fac * length(localpos));
  }

  return fac;
}

float probe_attenuation_planar(PlanarData pd, vec3 P)
{
  /* Distance from plane */
  float fac = saturate(abs(dot(pd.pl_plane_eq, vec4(P, 1.0))) * pd.pl_fade_scale +
                       pd.pl_fade_bias);
  /* Fancy fast clipping calculation */
  vec2 dist_to_clip;
  dist_to_clip.x = dot(pd.pl_clip_pos_x, P);
  dist_to_clip.y = dot(pd.pl_clip_pos_y, P);
  /* compare and add all tests */
  fac *= step(2.0, dot(step(pd.pl_clip_edges, dist_to_clip.xxyy), vec2(-1.0, 1.0).xyxy));
  return fac;
}

float probe_attenuation_planar_normal_roughness(PlanarData pd, vec3 N, float roughness)
{
  /* Normal Facing */
  float fac = saturate(dot(pd.pl_normal, N) * pd.pl_facing_scale + pd.pl_facing_bias);
  /* Decrease influence for high roughness */
  return fac * saturate(1.0 - roughness * 10.0);
}

float probe_attenuation_grid(GridData gd, vec3 P, out vec3 localpos)
{
  localpos = transform_point(gd.localmat, P);
  vec3 pos_to_edge = max(vec3(0.0), abs(localpos) - 1.0);
  float fade = length(pos_to_edge);
  return saturate(-fade * gd.g_atten_scale + gd.g_atten_bias);
}

vec3 probe_evaluate_cube(int pd_id, vec3 P, vec3 R, float roughness)
{
  /* Correct reflection ray using parallax volume intersection. */
  vec3 localpos = transform_point(probes_data[pd_id].parallaxmat, P);
  vec3 localray = transform_direction(probes_data[pd_id].parallaxmat, R);

  float dist;
  if (probes_data[pd_id].p_parallax_type == PROBE_PARALLAX_BOX) {
    dist = line_unit_box_intersect_dist(localpos, localray);
  }
  else {
    dist = line_unit_sphere_intersect_dist(localpos, localray);
  }

  /* Use Distance in WS directly to recover intersection */
  vec3 intersection = P + R * dist - probes_data[pd_id].p_position;

  /* From Frostbite PBR Course
   * Distance based roughness
   * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
   */
  float original_roughness = roughness;
  float linear_roughness = fast_sqrt(roughness);
  float distance_roughness = saturate(dist * linear_roughness / length(intersection));
  linear_roughness = mix(distance_roughness, linear_roughness, linear_roughness);
  roughness = linear_roughness * linear_roughness;

  float fac = saturate(original_roughness * 2.0 - 1.0);
  R = mix(intersection, R, fac * fac);

  float lod = linear_roughness * prbLodCubeMax;
  return textureLod(probeCubes, vec4(R, float(pd_id)), lod).rgb;
}

vec3 probe_evaluate_world_spec(vec3 R, float roughness)
{
  float lod = fast_sqrt(roughness) * prbLodCubeMax;
  return textureLod(probeCubes, vec4(R, 0.0), lod).rgb;
}

vec3 probe_evaluate_planar(int id, PlanarData pd, vec3 P, vec3 N, vec3 V, float roughness)
{
  /* Find view vector / reflection plane intersection. */
  vec3 point_on_plane = line_plane_intersect(P, V, pd.pl_plane_eq);

  /* How far the pixel is from the plane. */
  float ref_depth = 1.0; /* TODO: parameter. */

  /* Compute distorted reflection vector based on the distance to the reflected object.
   * In other words find intersection between reflection vector and the sphere center
   * around point_on_plane. */
  vec3 proj_ref = reflect(reflect(-V, N) * ref_depth, pd.pl_normal);

  /* Final point in world space. */
  vec3 ref_pos = point_on_plane + proj_ref;

  /* Reproject to find texture coords. */
  vec4 refco = ProjectionMatrix * (ViewMatrix * vec4(ref_pos, 1.0));
  refco.xy /= refco.w;

  /* TODO: If we support non-SSR planar reflection, we should blur them with gaussian
   * and chose the right mip depending on the cone footprint after projection */
  /* NOTE: X is inverted here to compensate inverted drawing. */
  vec3 radiance = textureLod(probePlanars, vec3(refco.xy * vec2(-0.5, 0.5) + 0.5, id), 0.0).rgb;

  return radiance;
}

void fallback_cubemap(vec3 N,
                      vec3 V,
                      vec3 P,
                      vec3 vP,
                      float roughness,
                      float roughnessSquared,
                      inout vec4 spec_accum)
{
  /* Specular probes */
  vec3 spec_dir = specular_dominant_dir(N, V, roughnessSquared);

  OcclusionData occlusion_data = occlusion_load(vP, 1.0);
  float final_ao = specular_occlusion(occlusion_data, V, N, roughness, spec_dir);

  /* Starts at 1 because 0 is world probe */
  for (int i = 1; i < MAX_PROBE && i < prbNumRenderCube && spec_accum.a < 0.999; i++) {
    float fade = probe_attenuation_cube(i, P);

    if (fade > 0.0) {
      vec3 spec = final_ao * probe_evaluate_cube(i, P, spec_dir, roughness);
      accumulate_light(spec, fade, spec_accum);
    }
  }

  /* World Specular */
  if (spec_accum.a < 0.999) {
    vec3 spec = final_ao * probe_evaluate_world_spec(spec_dir, roughness);
    accumulate_light(spec, 1.0, spec_accum);
  }
}

vec3 probe_evaluate_grid(GridData gd, vec3 P, vec3 N, vec3 localpos)
{
  localpos = localpos * 0.5 + 0.5;
  localpos = localpos * vec3(gd.g_resolution) - 0.5;

  vec3 localpos_floored = floor(localpos);
  vec3 trilinear_weight = fract(localpos);

  float weight_accum = 0.0;
  vec3 irradiance_accum = vec3(0.0);

  /* For each neighbor cells */
  for (int i = 0; i < 8; i++) {
    ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
    vec3 cell_cos = clamp(localpos_floored + vec3(offset), vec3(0.0), vec3(gd.g_resolution) - 1.0);

    /* Keep in sync with update_irradiance_probe */
    ivec3 icell_cos = ivec3(gd.g_level_bias * floor(cell_cos / gd.g_level_bias));
    int cell = gd.g_offset + icell_cos.z + icell_cos.y * gd.g_resolution.z +
               icell_cos.x * gd.g_resolution.z * gd.g_resolution.y;

    vec3 color = irradiance_from_cell_get(cell, N);

    /* We need this because we render probes in world space (so we need light vector in WS).
     * And rendering them in local probe space is too much problem. */
    vec3 ws_cell_location = gd.g_corner +
                            (gd.g_increment_x * cell_cos.x + gd.g_increment_y * cell_cos.y +
                             gd.g_increment_z * cell_cos.z);

    vec3 ws_point_to_cell = ws_cell_location - P;
    float ws_dist_point_to_cell = length(ws_point_to_cell);
    vec3 ws_light = ws_point_to_cell / ws_dist_point_to_cell;

    /* Smooth back-face test. */
    float weight = saturate(dot(ws_light, N));

    /* Precomputed visibility */
    weight *= load_visibility_cell(
        cell, ws_light, ws_dist_point_to_cell, gd.g_vis_bias, gd.g_vis_bleed, gd.g_vis_range);

    /* Smoother transition */
    weight += prbIrradianceSmooth;

    /* Trilinear weights */
    vec3 trilinear = mix(1.0 - trilinear_weight, trilinear_weight, vec3(offset));
    weight *= trilinear.x * trilinear.y * trilinear.z;

    /* Avoid zero weight */
    weight = max(0.00001, weight);

    weight_accum += weight;
    irradiance_accum += color * weight;
  }

  return irradiance_accum / weight_accum;
}

vec3 probe_evaluate_world_diff(vec3 N)
{
  if (prbNumRenderGrid == 0) {
    return vec3(0);
  }
  return irradiance_from_cell_get(0, N);
}
