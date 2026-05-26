/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_light_iter.bsl.hh"
#include "eevee_light_lib.bsl.hh"
#include "eevee_shadow.bsl.hh"
#include "eevee_shadow_tracing.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

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

float power_get(LightData light, LightingType type)
{
  /* Mask anything above 3. See LIGHT_TRANSLUCENT_WITH_THICKNESS. */
  return light.power[type & 3u];
}

bool light_linking_affects_receiver(uint2 light_set_membership, uchar receiver_light_set)
{
  return bitmask64_test(light_set_membership, receiver_light_set);
}

void eval_single_closure(LightData light,
                         LightVector lv,
                         LightVertices vertices,
                         ClosureLight &cl,
                         float3 V,
                         float attenuation,
                         float shadow)
{
  attenuation *= power_get(light, cl.type);
  if (attenuation < 1e-30f) {
    return;
  }
  auto &util_tx = sampler_get(eevee_utility_texture, utility_tx);
  float ltc_result = light_ltc(util_tx, light, cl.N, V, lv, cl.ltc_mat, vertices);
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

  void light_eval_single([[resource_table]] LightEvalData &srt,
                         LightData light,
                         const bool is_directional)
  {
    [[resource_table]] ShadowRenderData &srd = srt.shadow_data;

    if (!light_linking_affects_receiver(light.light_set_membership, receiver_light_set)) {
      return;
    }

#if defined(SPECIALIZED_SHADOW_PARAMS) || defined(SRT_CONSTANT_shadow_ray_count)
    int ray_count = shadow_ray_count;
    int ray_step_count = shadow_ray_step_count;
#else
    int ray_count = uniform_buf.shadow.ray_count;
    int ray_step_count = uniform_buf.shadow.step_count;
#endif

    LightVector lv = light_vector_get(light, is_directional, P);

    /* TODO(fclem): Get rid of this special case. */
    bool is_translucent_with_thickness = is_transmission &&
                                         (stack.cl[0].type == LIGHT_TRANSLUCENT_WITH_THICKNESS);

    float attenuation = light_attenuation_surface(light, is_directional, lv);
    float facing = light_attenuation_facing(light, lv.L, lv.dist, stack.cl[0].N, is_transmission);

    if (!is_translucent_with_thickness) {
      /* Only do attenuation for this case, since we integrate the whole sphere for translucency.
       * Moreover, stack.cl[0].N is overwritten for is_translucent_with_thickness. */
      attenuation *= facing;
    }

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
                           ray_count,
                           ray_step_count);
    }

    if (is_translucent_with_thickness) {
      /* This makes the LTC compute the solid angle of the light (still with the cosine term
       * applied but that still works great enough in practice). */
      stack.cl[0].N = lv.L;
      /* Adjust power because of the second lambertian distribution. */
      attenuation *= M_1_PI;
    }

    LightVertices light_shape_vertices = light_shape_corners(light, lv);

    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (is_transmission) [[static_branch]] {
        if (srt.light_closure_eval_count_transmit > i) [[static_branch]] {
          eval_single_closure(
              light, lv, light_shape_vertices, stack.cl[i], V, attenuation, shadow);
        }
      }
      else {
        if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
          eval_single_closure(
              light, lv, light_shape_vertices, stack.cl[i], V, attenuation, shadow);
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
