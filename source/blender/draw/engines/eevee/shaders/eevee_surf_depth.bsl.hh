/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Depth shader that can stochastically discard transparent pixel.
 */
#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_clip_plane)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)

#include "draw_curves_lib.glsl" /* IWYU pragma: export. For nodetree functions. */
#include "draw_view_lib.glsl"   /* IWYU pragma: export. For nodetree functions. */
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "eevee_transparency_lib.glsl"
#include "eevee_velocity_lib.glsl"

float4 closure_to_rgba_depth(Closure /*cl*/)
{
  float4 out_color;
  out_color.rgb = g_emission;
  out_color.a = saturate(1.0f - average(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset(0.0f);

  return out_color;
}

namespace eevee {

struct SurfaceDepth {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;

  [[compilation_constant]] bool use_velocity;
};

struct SurfaceDepthFragOut {
  [[frag_color(PREPASS_FRAG_OUT_NORMAL)]] float4 normal;
  [[frag_color(PREPASS_FRAG_OUT_OB_ID)]] uint object_id;
};

struct VelocityFragOut {
  [[frag_color(PREPASS_FRAG_OUT_VELOCITY)]] float4 velocity;
};

[[fragment]]
void surf_depth([[resource_table]] SurfaceDepth & /*srt*/,
                [[frag_coord]] const float4 frag_co,
                [[out]] SurfaceDepthFragOut &frag_out,
                [[out, condition(use_velocity)]] VelocityFragOut &vel_out,
                [[front_facing]] const bool front_face)
{
#ifdef MAT_TRANSPARENT
  init_globals(front_face);

  nodetree_surface(0.0f);

  float noise_offset = sampling_rng_1D_get(SAMPLING_TRANSPARENCY);
  float threshold = transparency_hashed_alpha_threshold(
      pipeline_buf.alpha_hash_scale, noise_offset, g_data.P);

  float transparency = average(g_transmittance);
  if (transparency > threshold) {
    gpu_discard_fragment();
    return;
  }
#endif

#ifdef MAT_CLIP_PLANE
  /* Do not use hardware clip planes as they modify the rasterization (some GPUs add vertices).
   * This would in turn create a discrepancy between the pre-pass depth and the G-buffer depth
   * which exhibits missing pixels data. */
  if (clip_interp.clip_distance > 0.0f) {
    gpu_discard_fragment();
    return;
  }
#endif

#ifdef MAT_VELOCITY
  {
    const auto &motion = interface_get(eevee_velocity_geom, motion);
    vel_out.velocity = velocity_surface(interp.P + motion.prev, interp.P, interp.P + motion.next);
    vel_out.velocity = velocity_pack(vel_out.velocity);
  }
#endif

  /* Always written, but may be optimized out by frame-buffer/subpass setup. */
  frag_out.normal.rgb = normalize(interp.N) * 0.5f + 0.5f;
  frag_out.object_id = drw_resource_id() & 0xFFFF;
}

}  // namespace eevee
