/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"

#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_geom_mesh)
SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#include "draw_view_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER) && defined(MAT_GEOM_MESH)
float3 barycentric_distances_get()
{
  float wp_delta = length(gpu_dfdx(interp.P)) + length(gpu_dfdy(interp.P));
  float bc_delta = length(gpu_dfdx(gpu_BaryCoord)) + length(gpu_dfdy(gpu_BaryCoord));
  float rate_of_change = wp_delta / bc_delta;
  return rate_of_change * (1.0f - gpu_BaryCoord);
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
  float cos_theta = curve_interp.time_width / curve_interp.radius;
  float sin_theta = sin_from_cos(cos_theta);
  g_data.N = g_data.Ni = normalize(interp.N * sin_theta + curve_interp.binormal * cos_theta);

  /* Costly, but follows cycles per pixel tangent space (not following curve shape). */
  float3 V = drw_world_incident_vector(g_data.P);
  g_data.curve_T = -curve_interp.tangent;
  g_data.curve_B = cross(V, g_data.curve_T);
  g_data.curve_N = safe_normalize(cross(g_data.curve_T, g_data.curve_B));

  g_data.is_strand = true;
  g_data.hair_diameter = curve_interp.radius * 2.0;
  g_data.hair_strand_id = curve_interp_flat.strand_id;
#  if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER)
  g_data.barycentric_coords.y = fract(curve_interp.point_id);
  g_data.barycentric_coords.x = 1.0 - g_data.barycentric_coords.y;
#  endif
#endif
}

void init_globals()
{
  /* Default values. */
  g_data.P = interp.P;
  g_data.Ni = interp.N;
  g_data.N = safe_normalize(interp.N);
  g_data.Ng = g_data.N;
  g_data.is_strand = false;
  g_data.hair_diameter = 0.0f;
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
  g_data.ray_depth = 0.0f;
  g_data.ray_length = distance(g_data.P, drw_view_position());
  g_data.barycentric_coords = float2(0.0f);
  g_data.barycentric_dists = float3(0.0f);

#ifdef GPU_FRAGMENT_SHADER
  g_data.N = (gl_FrontFacing) ? g_data.N : -g_data.N;
  g_data.Ni = (gl_FrontFacing) ? g_data.Ni : -g_data.Ni;
  g_data.Ng = safe_normalize(cross(gpu_dfdx(g_data.P), gpu_dfdy(g_data.P)));
#endif

#if defined(MAT_GEOM_MESH)
  init_globals_mesh();
#elif defined(MAT_GEOM_CURVES)
  init_globals_curves();
#endif
}

/* Avoid some compiler issue with non set interface parameters. */
void init_interface()
{
#ifdef GPU_VERTEX_SHADER
  interp.P = float3(0.0f);
  interp.N = float3(0.0f);
  drw_ResourceID_iface.resource_index = drw_resource_id_raw();
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

float3 shadow_position_vector_get(float3 view_position, ShadowRenderView view)
{
  if (view.is_directional) {
    return float3(0.0f, 0.0f, -view_position.z - view.clip_near);
  }
  return view_position;
}

/* In order to support physical clipping, we pass a vector to the fragment shader that then clips
 * each fragment using a unit sphere test. This allows to support both point light and area light
 * clipping at the same time. */
float3 shadow_clip_vector_get(float3 view_position, float clip_distance_inv)
{
  if (clip_distance_inv == 0.0f) {
    /* No clipping. */
    return float3(2.0f);
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
