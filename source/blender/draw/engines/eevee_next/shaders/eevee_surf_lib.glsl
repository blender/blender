/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_material_info.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_geom_mesh)

#include "draw_model_lib.glsl"
#include "eevee_nodetree_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER) && defined(MAT_GEOM_MESH)
vec3 barycentric_distances_get()
{
  float wp_delta = length(dFdx(interp.P)) + length(dFdy(interp.P));
  float bc_delta = length(dFdx(gpu_BaryCoord)) + length(dFdy(gpu_BaryCoord));
  float rate_of_change = wp_delta / bc_delta;
  return rate_of_change * (1.0 - gpu_BaryCoord);
}
#endif

void init_globals_mesh()
{
#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER) && defined(MAT_GEOM_MESH)
  g_data.barycentric_coords = gpu_BaryCoord.xy;
  g_data.barycentric_dists = barycentric_distances_get();
#endif
}

void init_globals_curves()
{
#if defined(MAT_GEOM_CURVES)
  /* Shade as a cylinder. */
  float cos_theta = curve_interp.time_width / curve_interp.thickness;
#  if defined(GPU_FRAGMENT_SHADER)
  if (hairThicknessRes == 1) {
#    ifdef EEVEE_UTILITY_TX
    /* Random cosine normal distribution on the hair surface. */
    float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).x;
#      ifdef EEVEE_SAMPLING_DATA
    /* Needs to check for SAMPLING_DATA, otherwise surfel shader validation fails. */
    noise = fract(noise + sampling_rng_1D_get(SAMPLING_CURVES_U));
#      endif
    cos_theta = noise * 2.0 - 1.0;
#    endif
  }
#  endif
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  g_data.N = g_data.Ni = normalize(interp.N * sin_theta + curve_interp.binormal * cos_theta);

  /* Costly, but follows cycles per pixel tangent space (not following curve shape). */
  vec3 V = drw_world_incident_vector(g_data.P);
  g_data.curve_T = -curve_interp.tangent;
  g_data.curve_B = cross(V, g_data.curve_T);
  g_data.curve_N = safe_normalize(cross(g_data.curve_T, g_data.curve_B));

  g_data.is_strand = true;
  g_data.hair_time = curve_interp.time;
  g_data.hair_thickness = curve_interp.thickness;
  g_data.hair_strand_id = curve_interp_flat.strand_id;
#  if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER)
  g_data.barycentric_coords = hair_resolve_barycentric(curve_interp.barycentric_coords);
#  endif
#endif
}

void init_globals_gpencil()
{
  /* Undo back-face flip as the grease-pencil normal is already pointing towards the camera. */
  g_data.N = g_data.Ni = interp.N;
}

void init_globals()
{
  /* Default values. */
  g_data.P = interp.P;
  g_data.Ni = interp.N;
  g_data.N = safe_normalize(interp.N);
  g_data.Ng = g_data.N;
  g_data.is_strand = false;
  g_data.hair_time = 0.0;
  g_data.hair_thickness = 0.0;
  g_data.hair_strand_id = 0;
#if defined(MAT_SHADOW)
  g_data.ray_type = RAY_TYPE_SHADOW;
#elif defined(MAT_CAPTURE)
  g_data.ray_type = RAY_TYPE_DIFFUSE;
#else
  if (uniform_buf.pipeline.is_sphere_probe) {
    g_data.ray_type = RAY_TYPE_GLOSSY;
  }
  else {
    g_data.ray_type = RAY_TYPE_CAMERA;
  }
#endif
  g_data.ray_depth = 0.0;
  g_data.ray_length = distance(g_data.P, drw_view_position());
  g_data.barycentric_coords = vec2(0.0);
  g_data.barycentric_dists = vec3(0.0);

#ifdef GPU_FRAGMENT_SHADER
  g_data.N = (FrontFacing) ? g_data.N : -g_data.N;
  g_data.Ni = (FrontFacing) ? g_data.Ni : -g_data.Ni;
  g_data.Ng = safe_normalize(cross(dFdx(g_data.P), dFdy(g_data.P)));
#endif

#if defined(MAT_GEOM_MESH)
  init_globals_mesh();
#elif defined(MAT_GEOM_CURVES)
  init_globals_curves();
#elif defined(MAT_GEOM_GPENCIL)
  init_globals_gpencil();
#endif
}

/* Avoid some compiler issue with non set interface parameters. */
void init_interface()
{
#ifdef GPU_VERTEX_SHADER
  interp.P = vec3(0.0);
  interp.N = vec3(0.0);
  drw_ResourceID_iface.resource_index = resource_id;
#endif
}

#if defined(GPU_VERTEX_SHADER) && defined(MAT_SHADOW)
void shadow_viewport_layer_set(int view_id, int lod)
{
#  ifdef SHADOW_UPDATE_ATOMIC_RASTER
  shadow_iface.shadow_view_id = view_id;
#  else
  /* We still render to a layered frame-buffer in the case of Metal + Tile Based Renderer.
   * Since it needs correct depth buffering, each view needs to not overlap each others.
   * It doesn't matter much for other platform, so we use that as a way to pass the view id. */
  gpu_Layer = view_id;
#  endif
  gpu_ViewportIndex = lod;
}

vec3 shadow_position_vector_get(vec3 view_position, ShadowRenderView view)
{
  if (view.is_directional) {
    return vec3(0.0, 0.0, -view_position.z - view.clip_near);
  }
  return view_position;
}

/* In order to support physical clipping, we pass a vector to the fragment shader that then clips
 * each fragment using a unit sphere test. This allows to support both point light and area light
 * clipping at the same time. */
vec3 shadow_clip_vector_get(vec3 view_position, float clip_distance_inv)
{
  if (clip_distance_inv == 0.0) {
    /* No clipping. */
    return vec3(2.0);
  }
  /* Punctual shadow case. */
  return view_position * clip_distance_inv;
}
#endif

#if defined(GPU_FRAGMENT_SHADER) && defined(MAT_SHADOW)
int shadow_view_id_get()
{
#  ifdef SHADOW_UPDATE_ATOMIC_RASTER
  return shadow_iface.shadow_view_id;
#  else
  return gpu_Layer;
#  endif
}
#endif
