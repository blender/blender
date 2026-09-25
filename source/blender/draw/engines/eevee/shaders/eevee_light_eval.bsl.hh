/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_light_iter.bsl.hh"
#include "eevee_light_lib.bsl.hh"
#include "eevee_ltc_lib.bsl.hh"
#include "eevee_shadow.bsl.hh"
#include "eevee_shadow_tracing.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

#if !defined(SRT_CONSTANT_light_closure_eval_count_reflect)
#  define SRT_CONSTANT_light_closure_eval_count_reflect 0
#endif
#if !defined(SRT_CONSTANT_light_closure_eval_count_transmit)
#  define SRT_CONSTANT_light_closure_eval_count_transmit 0
#endif

#ifdef GLSL_CPP_STUBS
#  define LIGHT_STACK_SIZE_REFLECT 3
#elif SRT_CONSTANT_light_closure_eval_count_reflect == 0
#  define LIGHT_STACK_SIZE_REFLECT 1 /* Avoid compilation error. */
#else
#  define LIGHT_STACK_SIZE_REFLECT SRT_CONSTANT_light_closure_eval_count_reflect
#endif

#ifdef GLSL_CPP_STUBS
#  define LIGHT_STACK_SIZE_TRANSMIT 3
#elif SRT_CONSTANT_light_closure_eval_count_transmit == 0
#  define LIGHT_STACK_SIZE_TRANSMIT 1 /* Avoid compilation error. */
#else
#  define LIGHT_STACK_SIZE_TRANSMIT SRT_CONSTANT_light_closure_eval_count_transmit
#endif

namespace eevee {

struct LightEvalData {
  [[resource_table]] srt_t<ShadowRenderData> shadow_data;
  [[resource_table]] srt_t<UtilityTexture> utility_tx;

  [[compilation_constant]] int light_closure_eval_count_reflect;
  [[compilation_constant]] int light_closure_eval_count_transmit;
};

namespace light {

template<bool is_transmission> struct ClosureStack {};

template<> struct ClosureStack<false> {
  ClosureLight cl[LIGHT_STACK_SIZE_REFLECT];
};

template<> struct ClosureStack<true> {
  ClosureLight cl[LIGHT_STACK_SIZE_TRANSMIT];
};

void eval_single_closure(sampler2DArray util_tx,
                         LightData light,
                         LightVector lv,
                         LightShape shape,
                         ClosureLight &cl,
                         float3 V,
                         float attenuation,
                         float shadow)
{
  attenuation *= power_get(light, cl.type);
  if (attenuation < 1e-30f) {
    return;
  }

  /* TODO(not_mark): remove, and update tests as this causes precision change. */
  /* Load LTC matrix and rotate into orthonormal basis around N. */
  LTCData ltc_data = LTCData::unpack_from(cl);
  float3x3 T = from_incident_vector(cl.N, V);
  ltc_data.Minv = ltc_data.Minv * transpose(T);
  float ltc_result = ltc::evaluate(util_tx, light, shape, lv, ltc_data);

  float3 out_radiance = light.color * ltc_result;
  float visibility = shadow * attenuation;
  cl.light_shadowed += visibility * out_radiance;
  cl.light_unshadowed += attenuation * out_radiance;
}

template<bool is_transmission> struct EvalCtx {
  ClosureStack<is_transmission> stack;

  float3 P;
  float3 Ng;
  float3 V;
  float2 texel;
  Thickness thickness;
  uchar receiver_light_set;
  float terminator_normal_offset;
  float terminator_geometry_offset;
  int ray_count;
  int ray_step_count;

  void light_eval_single([[resource_table]] LightEvalData &srt,
                         LightData light,
                         const bool is_directional)
  {
    [[resource_table]] ShadowRenderData &srd = srt.shadow_data;

    if (!light_linking_affects_receiver(light.light_set_membership, receiver_light_set)) {
      return;
    }

    LightVector lv = LightVector::get(light, is_directional, P);

    /* TODO(fclem): Get rid of this special case. */
    bool is_translucent_with_thickness = is_transmission &&
                                         (stack.cl[0].type == LIGHT_TRANSLUCENT_WITH_THICKNESS);

    float attenuation = light_attenuation_surface(light, is_directional, lv);
    if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
      return;
    }

    float shadow = 1.0f;
    if (light.tilemap_index != LIGHT_NO_SHADOW) {
      shadow = shadow_eval(srd,
                           light,
                           is_directional,
                           is_transmission,
                           is_translucent_with_thickness,
                           texel,
                           thickness,
                           P,
                           Ng,
                           stack.cl[0].N,
                           terminator_normal_offset,
                           terminator_geometry_offset,
                           1.0f,
                           ray_count,
                           ray_step_count);
    }

    LightShape shape = LightShape::get(light, lv);

    [[resource_table]] const UtilityTexture &util = srt.utility_tx;
    const auto &util_tx = util.utility_tx;

    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if constexpr (is_transmission) {
        if (srt.light_closure_eval_count_transmit > i) [[static_branch]] {
          eval_single_closure(util_tx, light, lv, shape, stack.cl[i], V, attenuation, shadow);
        }
      }
      else {
        if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
          eval_single_closure(util_tx, light, lv, shape, stack.cl[i], V, attenuation, shadow);
        }
      }
    }
  }

  void eval_directional([[resource_table]] LightEvalData &srt, uint /*l_idx*/, LightData light)
  {
    light_eval_single(srt, light, true);
  }

  void eval_local([[resource_table]] LightEvalData &srt, uint /*l_idx*/, LightData light)
  {
    light_eval_single(srt, light, false);
  }
};

template struct EvalCtx<true>;
template struct EvalCtx<false>;

template void foreach_visible<EvalCtx<true>, LightEvalData>(
    const LightRenderData &, float2, float, EvalCtx<true> &, LightEvalData &);
template void foreach_visible<EvalCtx<false>, LightEvalData>(
    const LightRenderData &, float2, float, EvalCtx<false> &, LightEvalData &);

/* NOTE: Doesn't init the closure stack. */
EvalCtx<true> init_from_reflect_ctx(EvalCtx<false> ctx)
{
  EvalCtx<true> ctx_tr;
  ctx_tr.P = ctx.P;
  ctx_tr.Ng = ctx.Ng;
  ctx_tr.V = ctx.V;
  ctx_tr.texel = ctx.texel;
  ctx_tr.thickness = ctx.thickness;
  ctx_tr.receiver_light_set = ctx.receiver_light_set;
  ctx_tr.terminator_normal_offset = ctx.terminator_normal_offset;
  ctx_tr.terminator_geometry_offset = ctx.terminator_geometry_offset;
  ctx_tr.ray_count = ctx.ray_count;
  ctx_tr.ray_step_count = ctx.ray_step_count;
  return ctx_tr;
}

}  // namespace light

struct LightEvalIterator {
  [[resource_table]] srt_t<LightEvalData> inner;
  [[resource_table]] srt_t<LightRenderData> light_data;

  void eval_reflection(light::EvalCtx<false> &ctx, float vPz)
  {
    [[resource_table]] LightEvalData &srt = inner;
    if (srt.light_closure_eval_count_reflect > 0) [[static_branch]] {
      light::foreach_visible(light_data, ctx.texel, vPz, ctx, srt);
    }
  }

  void eval_transmission(light::EvalCtx<true> &ctx, float vPz)
  {
    [[resource_table]] LightEvalData &srt = inner;
    if (srt.light_closure_eval_count_transmit > 0) [[static_branch]] {
      light::foreach_visible(light_data, ctx.texel, vPz, ctx, srt);
    }
  }
};

}  // namespace eevee
