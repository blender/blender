/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Surface Capture: Output surface parameters to diverse storage.
 *
 * This is a separate shader to allow custom closure behavior and avoid putting more complexity
 * into other surface shaders.
 */
#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)

#include "draw_curves_lib.glsl" /* IWYU pragma: export. For nodetree functions. */
#include "draw_view_lib.glsl"   /* IWYU pragma: export. For nodetree functions. */
#include "eevee_lightprobe_shared.hh"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

float4 closure_to_rgba_capture(Closure /*cl*/)
{
  return float4(0.0f);
}

namespace eevee {

struct SurfaceCapture {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;

  [[storage(SURFEL_BUF_SLOT, write)]] Surfel (&surfel_buf)[];
  [[storage(CAPTURE_BUF_SLOT, read_write)]] CaptureInfoData &capture_info_buf;

  [[push_constant]] bool is_double_sided;
};

[[fragment]]
void surf_capture([[resource_table]] SurfaceCapture &srt, [[front_facing]] const bool front_face)
{
  init_globals();

  /* TODO(fclem): Remove random sampling for capture and accumulate color. */
  float closure_rand = 0.5f;

  nodetree_surface(closure_rand);

  float3 albedo = float3(0.0f);

  for (int i = 0; i < CLOSURE_BIN_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get_resolved(uchar(i), 1.0f);
    if (cl.weight <= CLOSURE_WEIGHT_CUTOFF) {
      continue;
    }
    if (!closure_has_transmission(cl.type)) {
      /* Refraction is not supported in volume light probe capture. */
      albedo += cl.color;
    }
  }

  /* ----- Surfel output ----- */

  if (srt.capture_info_buf.do_surfel_count) {
    /* Generate a surfel only once. This check allow cases where no axis is dominant. */
    float3 vNg = drw_normal_world_to_view(g_data.Ng);
    bool is_surface_view_aligned = dominant_axis(vNg) == 2;
    if (is_surface_view_aligned) {
      uint surfel_id = atomicAdd(srt.capture_info_buf.surfel_len, 1u);
      if (srt.capture_info_buf.do_surfel_output) {
        ObjectInfos object_infos = drw_infos[drw_resource_id()];
        srt.surfel_buf[surfel_id].position = g_data.P;
        srt.surfel_buf[surfel_id].normal = front_face ? g_data.Ng : -g_data.Ng;
        srt.surfel_buf[surfel_id].albedo_front = albedo;
        srt.surfel_buf[surfel_id].radiance_direct.front.rgb = g_emission;
        srt.surfel_buf[surfel_id].radiance_direct.front.a = 0.0f;
        /* TODO(fclem): 2nd surface evaluation. */
        srt.surfel_buf[surfel_id].albedo_back = srt.is_double_sided ? albedo : float3(0);
        srt.surfel_buf[surfel_id].radiance_direct.back.rgb = srt.is_double_sided ? g_emission :
                                                                                   float3(0);
        srt.surfel_buf[surfel_id].radiance_direct.back.a = 0.0f;
        srt.surfel_buf[surfel_id].double_sided = srt.is_double_sided;
        srt.surfel_buf[surfel_id].receiver_light_set = receiver_light_set_get(object_infos);

        if (!srt.capture_info_buf.capture_emission) {
          srt.surfel_buf[surfel_id].radiance_direct.front.rgb = float3(0.0f);
          srt.surfel_buf[surfel_id].radiance_direct.back.rgb = float3(0.0f);
        }
      }
    }
  }
}

}  // namespace eevee
