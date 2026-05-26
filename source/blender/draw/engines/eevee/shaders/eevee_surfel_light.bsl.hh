/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Apply lights contribution to scene surfel representation.
 */

#pragma once

#include "draw_view_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define LIGHT_ITER_FORCE_NO_CULLING
#endif

COMPUTE_SHADER_CREATE_INFO(draw_view)

#include "eevee_closure.bsl.hh"
#include "eevee_light_eval.bsl.hh"
#include "eevee_surfel.bsl.hh"

namespace eevee::surfel {

struct EvalLight {
  [[legacy_info]] ShaderCreateInfo draw_view;

  /* WORKAROUND: Disables culling in lighting evaluation function. */
  [[compilation_constant]] bool light_iter_force_no_culling;
  /* WORKAROUND: Disables random jitter on shadow raytracing. */
  [[compilation_constant]] bool shadow_no_random;
};

[[compute, local_size(SURFEL_GROUP_SIZE)]]
void eval_light([[resource_table]] EvalLight & /*srt*/,
                [[resource_table]] LightEvalIterator &lights,
                [[resource_table]] const UtilityTexture &util_tx,
                [[resource_table]] SurfelData &surfels,
                [[global_invocation_id]] const uint3 global_id)
{
  const int index = int(global_id.x);
  if (index >= int(surfels.capture_info_buf.surfel_len)) {
    return;
  }

  Surfel surfel = surfels.surfel_buf[index];

  /* There is no view dependent effect as we evaluate everything using diffuse. */
  float3 V = surfel.normal;
  float3 Ng = surfel.normal;
  float3 P = surfel.position;

  eevee::light::EvalCtx<false> ctx;
  ctx.P = P;
  ctx.Ng = Ng;
  ctx.V = V;
  /* Note: This shader disables light culling and shadow jitter. */
  ctx.texel = float2(0.0);
  ctx.thickness = Thickness::zero();
  ctx.receiver_light_set = surfel.receiver_light_set;
  ctx.terminator_normal_offset = 0.0f;
  ctx.terminator_geometry_offset = 0.0f;

  ClosureUndetermined cl_reflect;
  cl_reflect.N = surfel.normal;
  cl_reflect.type = CLOSURE_BSDF_DIFFUSE_ID;
  ctx.stack.cl[0] = closure_light_new(util_tx, cl_reflect, V);
  lights.eval_reflection(ctx, 1.0f);

  if (surfels.capture_info_buf.capture_indirect) {
    surfels.surfel_buf[index].radiance_direct.front.rgb += ctx.stack.cl[0].light_shadowed *
                                                           surfel.albedo_front;
  }

  ClosureUndetermined cl_transmit;
  cl_transmit.N = -surfel.normal;
  cl_transmit.type = CLOSURE_BSDF_DIFFUSE_ID;
  ctx.stack.cl[0] = closure_light_new(util_tx, cl_transmit, -V);

  ctx.Ng = -Ng;
  ctx.V = -V;
  lights.eval_reflection(ctx, 1.0f);

  if (surfels.capture_info_buf.capture_indirect) {
    surfels.surfel_buf[index].radiance_direct.back.rgb += ctx.stack.cl[0].light_shadowed *
                                                          surfel.albedo_back;
  }
}

}  // namespace eevee::surfel

PipelineCompute eevee_surfel_light(eevee::surfel::eval_light,
                                   eevee::surfel::EvalLight{
                                       .light_iter_force_no_culling = true,
                                   },
                                   eevee::ShadowRenderData{
                                       .shadow_random = false,
                                   },
                                   eevee::LightEvalData{
                                       .light_closure_eval_count_reflect = 1,
                                       .light_closure_eval_count_transmit = 0,
                                   });
