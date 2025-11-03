/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Surface Capture: Output surface parameters to diverse storage.
 *
 * This is a separate shader to allow custom closure behavior and avoid putting more complexity
 * into other surface shaders.
 */

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_surf_capture_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_surf_capture)

#include "draw_curves_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

float4 closure_to_rgba(Closure cl)
{
  return float4(0.0f);
}

void main()
{
  init_globals();

  /* TODO(fclem): Remove random sampling for capture and accumulate color. */
  float closure_rand = 0.5f;

  nodetree_surface(closure_rand);

  float3 albedo = float3(0.0f);

  for (int i = 0; i < CLOSURE_BIN_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get_resolved(i, 1.0f);
    if (cl.weight <= CLOSURE_WEIGHT_CUTOFF) {
      continue;
    }
    if (cl.type != CLOSURE_BSDF_TRANSLUCENT_ID &&
        cl.type != CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID)
    {
      albedo += cl.color;
    }
  }

  /* ----- Surfel output ----- */

  if (capture_info_buf.do_surfel_count) {
    /* Generate a surfel only once. This check allow cases where no axis is dominant. */
    float3 vNg = drw_normal_world_to_view(g_data.Ng);
    bool is_surface_view_aligned = dominant_axis(vNg) == 2;
    if (is_surface_view_aligned) {
      uint surfel_id = atomicAdd(capture_info_buf.surfel_len, 1u);
      if (capture_info_buf.do_surfel_output) {
        ObjectInfos object_infos = drw_infos[drw_resource_id()];
        surfel_buf[surfel_id].position = g_data.P;
        surfel_buf[surfel_id].normal = gl_FrontFacing ? g_data.Ng : -g_data.Ng;
        surfel_buf[surfel_id].albedo_front = albedo;
        surfel_buf[surfel_id].radiance_direct.front.rgb = g_emission;
        surfel_buf[surfel_id].radiance_direct.front.a = 0.0f;
        /* TODO(fclem): 2nd surface evaluation. */
        surfel_buf[surfel_id].albedo_back = is_double_sided ? albedo : float3(0);
        surfel_buf[surfel_id].radiance_direct.back.rgb = is_double_sided ? g_emission : float3(0);
        surfel_buf[surfel_id].radiance_direct.back.a = 0.0f;
        surfel_buf[surfel_id].double_sided = is_double_sided;
        surfel_buf[surfel_id].receiver_light_set = receiver_light_set_get(object_infos);

        if (!capture_info_buf.capture_emission) {
          surfel_buf[surfel_id].radiance_direct.front.rgb = float3(0.0f);
          surfel_buf[surfel_id].radiance_direct.back.rgb = float3(0.0f);
        }
      }
    }
  }
}
