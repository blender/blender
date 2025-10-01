/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "draw_model_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_light_eval_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_subsurface_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

/* Allow static compilation of forward materials. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT LIGHT_CLOSURE_EVAL_COUNT
#endif

#if CLOSURE_BIN_COUNT != LIGHT_CLOSURE_EVAL_COUNT
#  error Closure data count and eval count must match
#endif

void forward_lighting_eval(float thickness, out float3 radiance, out float3 transmittance)
{
  float vPz = dot(drw_view_forward(), g_data.P) - dot(drw_view_forward(), drw_view_position());
  float3 V = drw_world_incident_vector(g_data.P);

  ClosureLightStack stack;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get(i);
    closure_light_set(stack, i, closure_light_new(cl, V));
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  ObjectInfos object_infos = drw_infos[drw_resource_id()];
  uchar receiver_light_set = receiver_light_set_get(object_infos);
  float normal_offset = object_infos.shadow_terminator_normal_offset;
  float geometry_offset = object_infos.shadow_terminator_geometry_offset;
  light_eval_reflection(
      stack, g_data.P, g_data.Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

#if defined(MAT_SUBSURFACE) || defined(MAT_REFRACTION) || defined(MAT_TRANSLUCENT)

  ClosureUndetermined cl_transmit = g_closure_get(0);
  if (cl_transmit.type != CLOSURE_NONE_ID) {
#  if defined(MAT_SUBSURFACE)
    float3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#  endif

    stack.cl[0] = closure_light_new(cl_transmit, V, thickness);

    /* NOTE: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(stack,
                            g_data.P,
                            g_data.Ng,
                            V,
                            vPz,
                            thickness,
                            receiver_light_set,
                            normal_offset,
                            geometry_offset);

#  if defined(MAT_SUBSURFACE)
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      float3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                   thickness);
      stack.cl[0].light_shadowed *= sss_profile;
      stack.cl[0].light_unshadowed *= sss_profile;
      stack.cl[0].light_shadowed += sss_reflect_shadowed;
      stack.cl[0].light_unshadowed += sss_reflect_unshadowed;
    }
#  endif
  }
#endif

  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);

  float clamp_indirect_sh = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect_sh);

  /* Combine all radiance. */
  float3 radiance_direct = float3(0.0f);
  float3 radiance_indirect = float3(0.0f);
  for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get_resolved(i, 1.0f);
    if (cl.weight > CLOSURE_WEIGHT_CUTOFF) {
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      float3 indirect_light = lightprobe_eval(samp, cl, g_data.P, V, thickness);

      if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
           cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
          (thickness != 0.0f))
      {
        /* We model two transmission event, so the surface color need to be applied twice. */
        cl.color *= cl.color;
      }

      radiance_direct += direct_light * cl.color;
      radiance_indirect += indirect_light * cl.color;
    }
  }
  /* Light clamping. */
  float clamp_direct = uniform_buf.clamp.surface_direct;
  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  radiance_direct = colorspace_brightness_clamp_max(radiance_direct, clamp_direct);
  radiance_indirect = colorspace_brightness_clamp_max(radiance_indirect, clamp_indirect);

  radiance = radiance_direct + radiance_indirect + g_emission;
  transmittance = g_transmittance;
}
