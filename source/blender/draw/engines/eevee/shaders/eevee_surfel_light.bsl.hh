/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Apply lights contribution to scene surfel representation.
 */

#pragma once

#include "infos/eevee_lightprobe_volume_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define LIGHT_ITER_FORCE_NO_CULLING
#endif

COMPUTE_SHADER_CREATE_INFO(draw_view)
COMPUTE_SHADER_CREATE_INFO(eevee_global_ubo)
COMPUTE_SHADER_CREATE_INFO(eevee_utility_texture)
COMPUTE_SHADER_CREATE_INFO(eevee_surfel_common)
COMPUTE_SHADER_CREATE_INFO(eevee_light_data)
COMPUTE_SHADER_CREATE_INFO(eevee_shadow_data)

#include "eevee_closure_lib.glsl"
#include "eevee_light_eval_lib.glsl"

#ifndef LIGHT_ITER_FORCE_NO_CULLING
#  error light_eval_reflection argument assumes this is defined
#endif

namespace eevee::surfel {

struct EvalLight {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;

  [[compilation_constant]] int light_closure_eval_count;
  [[compilation_constant]] bool light_iter_force_no_culling;
};

[[compute, local_size(SURFEL_GROUP_SIZE)]]
void eval_light([[resource_table]] EvalLight & /*srt*/,
                [[global_invocation_id]] const uint3 global_id)
{
  const int index = int(global_id.x);
  if (index >= int(capture_info_buf.surfel_len)) {
    return;
  }

  Surfel surfel = surfel_buf[index];

  /* There is no view dependent effect as we evaluate everything using diffuse. */
  float3 V = surfel.normal;
  float3 Ng = surfel.normal;
  float3 P = surfel.position;

  ClosureLightStack stack;

  ClosureUndetermined cl_reflect;
  cl_reflect.N = surfel.normal;
  cl_reflect.type = CLOSURE_BSDF_DIFFUSE_ID;
  stack.cl[0] = closure_light_new(cl_reflect, V);
  light_eval_reflection(stack, P, Ng, V, 0.0f, surfel.receiver_light_set, 0.0f, 0.0f);

  if (capture_info_buf.capture_indirect) {
    surfel_buf[index].radiance_direct.front.rgb += stack.cl[0].light_shadowed *
                                                   surfel.albedo_front;
  }

  ClosureUndetermined cl_transmit;
  cl_transmit.N = -surfel.normal;
  cl_transmit.type = CLOSURE_BSDF_DIFFUSE_ID;
  stack.cl[0] = closure_light_new(cl_transmit, -V);
  light_eval_reflection(stack, P, -Ng, -V, 0.0f, surfel.receiver_light_set, 0.0f, 0.0f);

  if (capture_info_buf.capture_indirect) {
    surfel_buf[index].radiance_direct.back.rgb += stack.cl[0].light_shadowed * surfel.albedo_back;
  }
}

}  // namespace eevee::surfel

PipelineCompute eevee_surfel_light(eevee::surfel::eval_light,
                                   eevee::surfel::EvalLight{.light_closure_eval_count = 1,
                                                            .light_iter_force_no_culling = true});
