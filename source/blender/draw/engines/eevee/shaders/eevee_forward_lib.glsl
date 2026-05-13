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
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_light_eval_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_subsurface_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

#ifdef GLSL_CPP_STUBS
#  define MAT_REFLECTION
#endif

/* Allow static compilation of forward materials. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT LIGHT_CLOSURE_EVAL_COUNT
#endif

#if CLOSURE_BIN_COUNT != LIGHT_CLOSURE_EVAL_COUNT
#  error Closure data count and eval count must match
#endif

void forward_lighting_eval(Thickness thickness,
                           float2 frag_co,
                           float3 &radiance,
                           float3 &transmittance)
{
  float vPz = dot(drw_view_forward(), g_data.P) - dot(drw_view_forward(), drw_view_position());
  float3 V = drw_world_incident_vector(g_data.P);

  ClosureLightStack stack;
  for (int i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get(uchar(i));
    closure_light_set(stack, uchar(i), closure_light_new(cl, V));
  }

  const auto &infos_buf = buffer_get(draw_object_infos, drw_infos);
  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  ObjectInfos object_infos = infos_buf[drw_resource_id()];
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

    if (cl_transmit.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
        cl_transmit.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID ||
        cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID)
    {
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
    }

#  if defined(MAT_SUBSURFACE)
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      float3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                   thickness.value());
      stack.cl[0].light_shadowed *= sss_profile;
      stack.cl[0].light_unshadowed *= sss_profile;
      stack.cl[0].light_shadowed += sss_reflect_shadowed;
      stack.cl[0].light_unshadowed += sss_reflect_unshadowed;
    }
#  endif
  }
#endif

  LightProbeSample samp = lightprobe_load(frag_co, g_data.P, g_data.Ng, V);

  float clamp_indirect_sh = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                             clamp_indirect_sh);

#ifdef MAT_REFLECTION
  /* Planar reflection. */
  float3 planar_probe_radiance = float3(0.0f);
  float3 average_N = g_data.Ng * 0.001f;
  {
    /* Get average normal.  */
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
      ClosureUndetermined cl = g_closure_get(i);
      average_N += cl.N * cl.weight;
    }
    average_N = safe_normalize(average_N);

    const int planar_id = lightprobe_planar_select(g_data.P, average_N);

    if (planar_id == -1) {
      average_N = float3(0.0f);
    }
    else {
      const auto &planar_buf = buffer_get(eevee_lightprobe_planar_data, probe_planar_buf);
      float3 P_reflected = lightprobe_planar_parallax(
          planar_buf[planar_id], g_data.P, average_N, V);

      float2 ndc_P_reflected = drw_point_world_to_ndc(P_reflected).xy;
      /* Planar probes are rendered upside down. */
      ndc_P_reflected.y = -ndc_P_reflected.y;
      float2 texel = drw_ndc_to_screen(ndc_P_reflected);

      planar_probe_radiance = textureLod(planar_radiance_tx, float3(texel, planar_id), 0.0).rgb;
      /* Discard background hits. */
      if (textureLod(planar_depth_tx, float3(texel, planar_id), 0.0).r == reverse_z::read(1.0f)) {
        average_N = float3(0.0f);
      }
    }
  }
#endif

  /* Combine all radiance. */
  float3 radiance_direct = float3(0.0f);
  float3 radiance_indirect = float3(0.0f);
  for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get_resolved(i, 1.0f);
    if (cl.weight > CLOSURE_WEIGHT_CUTOFF) {
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      float3 indirect_light = lightprobe_eval(samp, cl, g_data.P, V, thickness);

#ifdef MAT_REFLECTION
      if (cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID) {
        const float blend = saturate(to_closure_reflection(cl).roughness * -10.0f + 1.0f) *
                            saturate(dot(average_N, cl.N) * 100.0f - 99.0f);
        indirect_light = mix(indirect_light, planar_probe_radiance, blend);
      }
#endif

      if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
           cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
          (thickness.value() != 0.0f))
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

  radiance_direct = colorspace::brightness_clamp_max(radiance_direct, clamp_direct);
  radiance_indirect = colorspace::brightness_clamp_max(radiance_indirect, clamp_indirect);

  radiance_direct *= uniform_buf.clamp.direct_scale;
  radiance_indirect *= uniform_buf.clamp.indirect_scale;

  radiance = radiance_direct + radiance_indirect + g_emission;

  transmittance = g_transmittance;
}
