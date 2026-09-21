/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_geom_infos.hh"

#include "eevee_lightprobe_shared.hh" /* IWYU pragma: export: Needed for resource declaration. */
#include "eevee_nodetree_lib.bsl.hh"
#include "eevee_sampling_shared.hh" /* IWYU pragma: export: Needed for resource declaration. */
#include "eevee_shadow_shared.hh"
#include "eevee_uniform.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

namespace eevee {

struct GeomShadow {
  [[storage(SHADOW_RENDER_VIEW_BUF_SLOT,
            read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];
};

}  // namespace eevee

#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER) && defined(MAT_GEOM_MESH)
float3 barycentric_distances_get()
{
  float wp_delta = length(gpu_dfdx(interp.P)) + length(gpu_dfdy(interp.P));
  float bc_delta = length(gpu_dfdx(gpu_BaryCoord)) + length(gpu_dfdy(gpu_BaryCoord));
  float rate_of_change = wp_delta / bc_delta;
  return rate_of_change * (1.0f - gpu_BaryCoord);
}
#endif

void init_globals_mesh(ShadingData &sd)
{
#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER) && defined(MAT_GEOM_MESH)
  sd.barycentric_coords = gpu_BaryCoord.xy;
  sd.barycentric_dists = barycentric_distances_get();
#else
  sd.barycentric_coords = float2(0.0f);
  sd.barycentric_dists = float3(0.0f);
#endif
}

void init_globals_curves(ShadingData &sd, const ViewMatrices view)
{
  auto &interp = interface_get(eevee_geom_iface_info, interp);
  auto &curve_interp = interface_get(eevee_geom_curves_iface_info, curve_interp);
  auto &curve_interp_flat = interface_get(eevee_geom_curves_iface_info, curve_interp_flat);
  /* Shade as a cylinder. */
  float cos_theta = curve_interp.time_width / curve_interp.radius;
  float sin_theta = sin_from_cos(cos_theta);
  sd.N = sd.Ni = normalize(interp.N * sin_theta + curve_interp.binormal * cos_theta);

  /* Costly, but follows cycles per pixel tangent space (not following curve shape). */
  float3 V = view.world_incident_vector(sd.P);
  sd.curve_T = -curve_interp.tangent;
  sd.curve_B = cross(V, sd.curve_T);
  sd.curve_N = safe_normalize(cross(sd.curve_T, sd.curve_B));

  sd.is_strand = true;
  sd.hair_diameter = curve_interp.radius * 2.0;
  sd.hair_strand_id = curve_interp_flat.strand_id;
#if defined(USE_BARYCENTRICS) && defined(GPU_FRAGMENT_SHADER)
  sd.barycentric_coords.y = fract(curve_interp.point_id);
  sd.barycentric_coords.x = 1.0 - sd.barycentric_coords.y;
#endif
}

void init_globals_pointcloud(ShadingData &sd)
{
  auto &ptcloud_interp = interface_get(eevee_geom_pointcloud_iface_info, pointcloud_interp);
  auto &ptcloud_interp_flat = interface_get(eevee_geom_pointcloud_iface_info,
                                            pointcloud_interp_flat);

  sd.point_position = ptcloud_interp.position;
  sd.point_radius = ptcloud_interp.radius;
  sd.point_id = ptcloud_interp_flat.id;
}

[[nodiscard]] ShadingData init_globals([[resource_table]] const eevee::Uniform &uni,
                                       const ViewMatrices view,
                                       bool front_face,
                                       float4 fragment_co)
{
  auto &interp = interface_get(eevee_geom_iface_info, interp);
  ShadingData sd;
  /* Default values. */
  sd.frag_co = fragment_co;
  sd.front_facing = front_face;
  sd.P = interp.P;
  sd.Ni = interp.N;
  sd.N = safe_normalize(interp.N);
  sd.Ng = sd.N;
  sd.is_strand = false;
  sd.hair_diameter = 0.0f;
  sd.hair_strand_id = 0;
  sd.point_position = float3(0.0f);
  sd.point_radius = 0.0f;
  sd.point_id = 0;
#if defined(MAT_SHADOW)
  sd.ray_type = RAY_TYPE_SHADOW;
#elif defined(MAT_CAPTURE)
  sd.ray_type = RAY_TYPE_DIFFUSE;
#else
  sd.ray_type = uni.pipeline_buf.ray_type;
#endif
  sd.ray_depth = 0.0f;
  sd.ray_length = distance(sd.P, view.position());
  sd.barycentric_coords = float2(0.0f);
  sd.barycentric_dists = float3(0.0f);
  sd.thickness = Thickness::zero();

  sd.N = (front_face) ? sd.N : -sd.N;
  sd.Ni = (front_face) ? sd.Ni : -sd.Ni;
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  sd.Ng = safe_normalize(cross(gpu_dfdx(sd.P), gpu_dfdy(sd.P)));
  if (uni.pipeline_buf.is_main_view_inverted) {
    sd.Ng = -sd.Ng;
  }
#endif

#if defined(MAT_GEOM_MESH)
  init_globals_mesh(sd);
#elif defined(MAT_GEOM_POINTCLOUD)
  init_globals_pointcloud(sd);
#elif defined(MAT_GEOM_CURVES)
  init_globals_curves(sd, view);
#endif
  return sd;
}

/* Avoid some compiler issue with non set interface parameters. */
void init_interface([[maybe_unused]] uint resource_id_raw)
{
#ifdef GPU_VERTEX_SHADER
  auto &interp = interface_get(eevee_geom_iface_info, interp);
  auto &interp_flat = interface_get(eevee_geom_iface_info, interp_flat);
  interp.P = float3(0.0f);
  interp.N = float3(0.0f);
  interp_flat.resource_id_raw = resource_id_raw;
#endif
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
