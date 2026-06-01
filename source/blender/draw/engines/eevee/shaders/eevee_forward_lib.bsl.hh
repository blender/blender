/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "infos/eevee_geom_infos.hh"

#include "draw_model.bsl.hh"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_light_eval.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_lightprobe_plane.bsl.hh"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_subsurface_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

#ifdef GLSL_CPP_STUBS
#  define MAT_REFLECTION
#endif

/* Allow static compilation of forward materials. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT SRT_CONSTANT_light_closure_eval_count
#endif

#ifndef GLSL_CPP_STUBS
#  if CLOSURE_BIN_COUNT != SRT_CONSTANT_light_closure_eval_count && \
      SRT_CONSTANT_light_closure_eval_count != 0
#    error Closure data count and eval count must match
#  endif
#endif

namespace eevee {

void forward_lighting_eval(const ViewMatrices view,
                           uint resource_id,
                           Thickness thickness,
                           float2 frag_co,
                           float3 &radiance,
                           float3 &transmittance)
{
  [[resource_table]] LightEvalIterator &lights = resource_table_get(eevee::LightEvalIterator);
  [[resource_table]] UtilityTexture &util_tx = resource_table_get(UtilityTexture);
  [[resource_table]] const Uniform &uni = resource_table_get(eevee::Uniform);
  /* clang-format off */ /* Multiline macro breaks error line counting. */
  [[resource_table]] LightprobeRenderData &lightprobes = resource_table_get(eevee::LightprobeRenderData);
  [[resource_table]] LightprobePlaneRenderData &lightprobe_planes = resource_table_get(eevee::LightprobePlaneRenderData);
  /* clang-format on */
  [[resource_table]] LightEvalData &srt = lights.inner;
  [[resource_table]] draw::Infos &infos = resource_table_get(draw::Infos);

  float vPz = dot(view.forward(), g_data.P) - dot(view.forward(), view.position());
  float3 V = view.world_incident_vector(g_data.P);

  light::EvalCtx<false> ctx;
  for (uint i = 0u; i < 3; i++) [[unroll]] {
    if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
      ClosureUndetermined cl = g_closure_get(uchar(i));
      ctx.stack.cl[i] = closure_light_new(util_tx, cl, V);
    }
  }

  ctx.P = g_data.P;
  ctx.Ng = g_data.Ng;
  ctx.V = V;
  ctx.texel = frag_co;
  ctx.thickness = thickness;

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  ObjectInfos object_infos = infos.get(resource_id);
  ctx.receiver_light_set = receiver_light_set_get(object_infos);
  ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
  ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;

  lights.eval_reflection(ctx, vPz);

  if (srt.light_closure_eval_count_transmit > 0) [[static_branch]] {
    ClosureUndetermined cl_transmit = g_closure_get(0);
    if (closure_has_transmission(cl_transmit.type) || cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID)
    {
      light::EvalCtx<true> ctx_tr = light::init_from_reflect_ctx(ctx);
      ctx_tr.stack.cl[0] = closure_light_new(util_tx, cl_transmit, V, thickness);

      /* NOTE: Only evaluates `stack.cl[0]`. */
      lights.eval_transmission(ctx_tr, vPz);

      if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
#if defined(GLSL_CPP_STUBS) || defined(MAT_SUBSURFACE)
        /* Apply transmission profile onto transmitted light and sum with reflected light. */
        float3 sss_profile = subsurface_transmission(
            util_tx, to_closure_subsurface(cl_transmit).sss_radius, thickness.value());
        ctx.stack.cl[0].light_shadowed += ctx_tr.stack.cl[0].light_shadowed * sss_profile;
        ctx.stack.cl[0].light_unshadowed += ctx_tr.stack.cl[0].light_unshadowed * sss_profile;
#endif
      }
      else {
        ctx.stack.cl[0].light_shadowed = ctx_tr.stack.cl[0].light_shadowed;
        ctx.stack.cl[0].light_unshadowed = ctx_tr.stack.cl[0].light_unshadowed;
      }
    }
  }

  LightProbeSample samp = lightprobes.load(frag_co, g_data.P, g_data.Ng, V);

  float clamp_indirect_sh = uni.uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                             clamp_indirect_sh);

#ifdef MAT_REFLECTION /* Disable if only rough surfaces. */
  /* Planar reflection. */
  float3 planar_probe_radiance = float3(0.0f);
  float3 average_N = g_data.Ng * 0.001f;
  {
    /* Get average normal.  */
    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
        ClosureUndetermined cl = g_closure_get(uchar(i));
        average_N += cl.N * cl.weight;
      }
    }
    average_N = safe_normalize(average_N);

    const int planar_id = lightprobe_planes.select_probe(g_data.P, average_N);

    if (planar_id == -1) {
      average_N = float3(0.0f);
    }
    else {
      float3 P_reflected = lightprobe::plane::parallax(
          lightprobe_planes.probe_planar_buf[planar_id], g_data.P, average_N, V);

      float2 ndc_P_reflected = view.point_world_to_ndc(P_reflected).xy;
      /* Planar probes are rendered upside down. */
      ndc_P_reflected.y = -ndc_P_reflected.y;
      float2 texel = view.ndc_to_screen(ndc_P_reflected);

      planar_probe_radiance =
          textureLod(lightprobe_planes.planar_radiance_tx, float3(texel, planar_id), 0.0).rgb;
      /* Discard background hits. */
      if (textureLod(lightprobe_planes.planar_depth_tx, float3(texel, planar_id), 0.0).r ==
          reverse_z::read(1.0f))
      {
        average_N = float3(0.0f);
      }
    }
  }
#endif

  /* Combine all radiance. */
  float3 radiance_direct = float3(0.0f);
  float3 radiance_indirect = float3(0.0f);

  for (uint i = 0u; i < 3; i++) [[unroll]] {
    if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
      ClosureUndetermined cl = g_closure_get_resolved(uchar(i), 1.0f);
      if (cl.weight > CLOSURE_WEIGHT_CUTOFF) {
        float3 direct_light = ctx.stack.cl[i].light_shadowed;
        float3 indirect_light = lightprobes.eval(samp, cl, g_data.P, V, thickness);

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
  }
  /* Light clamping. */
  float clamp_direct = uni.uniform_buf.clamp.surface_direct;
  float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;

  radiance_direct = colorspace::brightness_clamp_max(radiance_direct, clamp_direct);
  radiance_indirect = colorspace::brightness_clamp_max(radiance_indirect, clamp_indirect);

  radiance_direct *= uni.uniform_buf.clamp.direct_scale;
  radiance_indirect *= uni.uniform_buf.clamp.indirect_scale;

  radiance = radiance_direct + radiance_indirect + g_emission;

  transmittance = g_transmittance;
}

}  // namespace eevee
