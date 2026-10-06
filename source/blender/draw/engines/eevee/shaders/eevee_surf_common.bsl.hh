/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_gsplat_lib.bsl.hh"
#include "eevee_nodetree_lib.bsl.hh"
#include "eevee_shadow_shared.hh"
#include "eevee_uniform.bsl.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

namespace eevee {

/* Common interface */
struct VertOutCommon {
  /* World Position. */
  [[smooth]] float3 P;
  /* World Normal. */
  [[smooth]] float3 N;
  /* Resource ID. */
  [[flat]] uint resource_id_raw;
};

struct VertOutShadow {
  [[flat]] int shadow_view_id;
};

struct VertOutShadowClipping {
  [[smooth]] float3 position;
  [[smooth]] float3 vector;
};

struct VertOutCurves {
  [[smooth]] float3 tangent;
  [[smooth]] float3 binormal;
  [[smooth]] float time;
  [[smooth]] float time_width;
  [[smooth]] float radius;
  [[smooth]] float point_id; /* Smooth to be used for barycentric. */
  [[flat]] int strand_id;
};

struct VertOutPointcloud {
  [[smooth]] float radius;
  [[smooth]] float3 position;
  [[flat]] int id;
};

struct VertOutGSplat {
  [[smooth]] float2 billboard_co;
  [[flat]] float opacity;
};

struct VertOutClipPlane {
  [[smooth]] float clip_distance;
};

struct ClipPlane {
  [[uniform(CLIP_PLANE_BUF)]] ClipPlaneData &clip_plane;
};

struct GeomShadow {
  [[storage(SHADOW_RENDER_VIEW_BUF_SLOT,
            read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];
};

void init_globals_mesh(const VertOutCommon &interp, ShadingData &sd, float3 barycentric_co)
{
  float wp_delta = 0.0f;
  float bc_delta = 0.0f;
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  wp_delta = length(gpu_dfdx(interp.P)) + length(gpu_dfdy(interp.P));
  bc_delta = length(gpu_dfdx(barycentric_co)) + length(gpu_dfdy(barycentric_co));
#endif
  float rate_of_change = wp_delta * safe_rcp(bc_delta);
  sd.barycentric_dists = rate_of_change * (1.0f - barycentric_co);
  sd.barycentric_coords = barycentric_co.xy;
}

void init_globals_curves(const VertOutCommon &interp,
                         const VertOutCurves &curve_interp,
                         ShadingData &sd,
                         const ViewMatrices view)
{
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
  sd.hair_strand_id = curve_interp.strand_id;
  sd.barycentric_coords.y = fract(curve_interp.point_id);
  sd.barycentric_coords.x = 1.0 - sd.barycentric_coords.y;
}

void init_globals_pointcloud(const VertOutPointcloud &ptcloud_interp, ShadingData &sd)
{
  sd.point_position = ptcloud_interp.position;
  sd.point_radius = ptcloud_interp.radius;
  sd.point_id = ptcloud_interp.id;
}

[[nodiscard]] ShadingData init_globals(const eevee::PipelineConstants &pipe,
                                       const eevee::Uniform &uni,
                                       const VertOutCommon &interp,
                                       const ViewMatrices view,
                                       bool front_face,
                                       float4 fragment_co)
{
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
  if (pipe.is_shadow_pipe) [[static_branch]] {
    sd.ray_type = RAY_TYPE_SHADOW;
  }
  else if (pipe.is_capture_pipe) [[static_branch]] {
    sd.ray_type = RAY_TYPE_DIFFUSE;
  }
  else {
    sd.ray_type = uni.pipeline_buf.ray_type;
  }
  sd.ray_depth = 0.0f;
  sd.ray_length = distance(sd.P, view.position());
  sd.barycentric_coords = float2(0.0f);
  sd.barycentric_dists = float3(0.0f);
  sd.thickness = Thickness::zero();
  sd.resource_id_raw = interp.resource_id_raw;

  sd.N = (front_face) ? sd.N : -sd.N;
  sd.Ni = (front_face) ? sd.Ni : -sd.Ni;
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  sd.Ng = safe_normalize(cross(gpu_dfdx(sd.P), gpu_dfdy(sd.P)));
  if (uni.pipeline_buf.is_main_view_inverted) {
    sd.Ng = -sd.Ng;
  }
#endif
  return sd;
}

/* Avoid some compiler issue with non set interface parameters. */
void init_interface(VertOutCommon &interp, uint resource_id_raw)
{
  interp.P = float3(0.0f);
  interp.N = float3(0.0f);
  interp.resource_id_raw = resource_id_raw;
}

float3 gsplat_amend_transmittance([[maybe_unused]] const VertOutGSplat &gsplat_interp,
                                  float3 transmittance)
{
  float alpha = draw::gsplat::evaluate_gaussian(gsplat_interp.billboard_co, gsplat_interp.opacity);
  alpha = saturate(alpha * (256.0f / 255.0f));
  return float3(1.0f) - alpha * saturate(float3(1.0f) - transmittance);
}

}  // namespace eevee

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
