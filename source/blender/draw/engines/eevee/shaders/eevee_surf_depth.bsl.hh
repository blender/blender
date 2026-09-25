/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Depth shader that can stochastically discard transparent pixel.
 */
#pragma once

#include "draw_gsplat_lib.bsl.hh" /* IWYU pragma: export. For nodetree functions. */

#include "eevee_nodetree_frag_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"
#include "eevee_transparency.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "eevee_velocity.bsl.hh"

float4 closure_to_rgba_depth([[resource_table]] KernelGlobals &kg, ShadingData &sd, Closure /*cl*/)
{
  float4 out_color;
  out_color.rgb = sd.emission;
  out_color.a = saturate(1.0f - average(sd.transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset(kg, sd, 0.0f);

  return out_color;
}

namespace eevee {

/* WORKAROUND(fclem): This is not supposed to be needed.
 * But Metal still writes to the velocity buffer if the frag output is defined. And conditions are
 * not yet supported on in/out. */
template<bool with_velocity> struct SurfaceDepthFragOut {};

template<> struct SurfaceDepthFragOut<false> {
  [[frag_color(PREPASS_FRAG_OUT_NORMAL)]] float4 normal;
  [[frag_color(PREPASS_FRAG_OUT_OB_ID)]] uint object_id;
};

template<> struct SurfaceDepthFragOut<true> {
  [[frag_color(PREPASS_FRAG_OUT_NORMAL)]] float4 normal;
  [[frag_color(PREPASS_FRAG_OUT_OB_ID)]] uint object_id;
  [[frag_color(PREPASS_FRAG_OUT_VELOCITY)]] float4 velocity;
};

template<bool with_velocity>
[[fragment]]
void surf_depth([[resource_table]] KernelGlobals &kg,
                [[resource_table]] PipelineConstants &pipe,
                [[resource_table]] const Uniform &uni,
                [[resource_table]] const Sampling &sampling,
                [[resource_table]] const UtilityTexture & /*util_tx*/,
                [[resource_table]] const draw::View &views,
                [[resource_table]] [[condition(use_velocity)]] const GeometryVelocity &geo_vel,
                [[frag_coord]] const float4 frag_co,
                [[in]] const VertOutCommon &interp,
                [[in]] [[condition(is_curves)]] const VertOutCurves &curves_interp,
                [[in]] [[condition(is_pointcloud)]] const VertOutPointcloud &ptcloud_interp,
                [[in]] [[condition(is_gsplat)]] const VertOutGSplat &gsplat_interp,
                [[in]] [[condition(use_velocity)]] const VertOutVelocity &motion,
                [[in]] [[condition(use_clip_plane)]] const VertOutClipPlane &clip_interp,
                [[out]] SurfaceDepthFragOut<with_velocity> &frag_out,
                [[front_facing]] const bool front_face)
{
  if (pipe.use_transparency) [[static_branch]] {
    const ViewMatrices view = views.get(0);

    ShadingData sd = init_globals(uni, interp, view, front_face, frag_co);
    if (pipe.is_mesh) [[static_branch]] {
      init_globals_mesh(interp, sd);
    }
    else if (pipe.is_curves) [[static_branch]] {
      init_globals_curves(interp, curves_interp, sd, view);
    }
    else if (pipe.is_pointcloud) [[static_branch]] {
      init_globals_pointcloud(ptcloud_interp, sd);
    }

    nodetree_surface(kg, sd, 0.0f);

    if (pipe.is_gsplat) [[static_branch]] {
      sd.transmittance = gsplat_amend_transmittance(gsplat_interp, sd.transmittance);
    }

    float noise_offset = sampling.rng_1D_get(SAMPLING_TRANSPARENCY);
    float threshold = hashed_transparency::alpha_threshold(
        uni.pipeline_buf.alpha_hash_scale, noise_offset, sd.P);

    float transparency = average(sd.transmittance);
    if (transparency > threshold) {
      gpu_discard_fragment();
      return;
    }
  }

  if (pipe.use_clip_plane) [[static_branch]] {
    /* Do not use hardware clip planes as they modify the rasterization (some GPUs add vertices).
     * This would in turn create a discrepancy between the pre-pass depth and the G-buffer depth
     * which exhibits missing pixels data. */
    if (clip_interp.clip_distance > 0.0f) {
      gpu_discard_fragment();
      return;
    }
  }

  if constexpr (with_velocity) {
    if (pipe.use_velocity) [[static_branch]] {
      [[resource_table]] const CameraVelocity &cam_vel = geo_vel.camera;
      frag_out.velocity = cam_vel.surface_velocity(
          interp.P + motion.prev, interp.P, interp.P + motion.next);
      frag_out.velocity = velocity::pack(frag_out.velocity);
    }
  }

  /* Always written, but may be optimized out by frame-buffer/subpass setup. */
  frag_out.normal.rgb = normalize(interp.N) * 0.5f + 0.5f;
  frag_out.object_id = interp.resource_id_raw & uint(0xFFFF);
}

template void surf_depth<true>(KernelGlobals &,
                               PipelineConstants &,
                               const Uniform &,
                               const Sampling &,
                               const UtilityTexture &,
                               const draw::View &,
                               const GeometryVelocity &,
                               const float4,
                               const VertOutCommon &,
                               const VertOutCurves &,
                               const VertOutPointcloud &,
                               const VertOutGSplat &,
                               const VertOutVelocity &,
                               const VertOutClipPlane &,
                               SurfaceDepthFragOut<true> &,
                               const bool);
template void surf_depth<false>(KernelGlobals &,
                                PipelineConstants &,
                                const Uniform &,
                                const Sampling &,
                                const UtilityTexture &,
                                const draw::View &,
                                const GeometryVelocity &,
                                const float4,
                                const VertOutCommon &,
                                const VertOutCurves &,
                                const VertOutPointcloud &,
                                const VertOutGSplat &,
                                const VertOutVelocity &,
                                const VertOutClipPlane &,
                                SurfaceDepthFragOut<false> &,
                                const bool);

}  // namespace eevee
