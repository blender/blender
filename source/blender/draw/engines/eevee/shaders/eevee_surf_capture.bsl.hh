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

#include "eevee_lightprobe_shared.hh"
#include "eevee_nodetree_frag_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"
#include "gpu_shader_math_vector.bsl.hh"

float4 closure_to_rgba_capture(KernelGlobals &kg, ShadingData &sd, Closure /*cl*/)
{
  float3 transmittance = sd.transmittance;
  closure_weights_reset(kg, sd, 0.0f);
  return float4(0.0f, 0.0f, 0.0f, saturate(1.0f - average(transmittance)));
}

namespace eevee {

struct SurfaceCapture {
  [[storage(SURFEL_BUF_SLOT, write)]] Surfel (&surfel_buf)[];
  [[storage(CAPTURE_BUF_SLOT, read_write)]] CaptureInfoData &capture_info_buf;

  [[push_constant]] bool is_double_sided;
};

[[fragment]]
void surf_capture([[resource_table]] KernelGlobals &kg,
                  [[resource_table]] PipelineConstants &pipe,
                  [[resource_table]] SurfaceCapture &srt,
                  [[resource_table]] const Uniform &uni,
                  [[resource_table]] const draw::View &views,
                  [[resource_table]] const UtilityTexture & /*util_tx*/,
                  [[in]] const VertOutCommon &interp,
                  [[in]] [[condition(is_curves)]] const VertOutCurves &curves_interp,
                  [[in]] [[condition(is_pointcloud)]] const VertOutPointcloud &ptcloud_interp,
                  [[frag_coord]] const float4 frag_co,
                  [[front_facing]] const bool front_face)
{
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

  /* TODO(fclem): Remove random sampling for capture and accumulate color. */
  float closure_rand = 0.5f;

  nodetree_surface(kg, sd, closure_rand);

  float3 albedo = float3(0.0f);

  for (int i = 0; i < CLOSURE_BIN_COUNT; i++) {
    ClosureUndetermined cl = sd.closure_get_resolved(uchar(i), 1.0f);
    if (cl.weight() <= CLOSURE_WEIGHT_CUTOFF) {
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
    float3 vNg = views.get(0).normal_world_to_view(sd.Ng);
    bool is_surface_view_aligned = dominant_axis(vNg) == 2;
    if (is_surface_view_aligned) {
      uint surfel_id = atomicAdd(srt.capture_info_buf.surfel_len, 1u);
      if (srt.capture_info_buf.do_surfel_output) {
        ObjectInfos object_infos = kg.object_infos_get(sd);
        srt.surfel_buf[surfel_id].position = sd.P;
        srt.surfel_buf[surfel_id].normal = front_face ? sd.Ng : -sd.Ng;
        srt.surfel_buf[surfel_id].albedo_front = albedo;
        srt.surfel_buf[surfel_id].radiance_direct.front.rgb = sd.emission;
        srt.surfel_buf[surfel_id].radiance_direct.front.a = 0.0f;
        /* TODO(fclem): 2nd surface evaluation. */
        srt.surfel_buf[surfel_id].albedo_back = srt.is_double_sided ? albedo : float3(0);
        srt.surfel_buf[surfel_id].radiance_direct.back.rgb = srt.is_double_sided ? sd.emission :
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
