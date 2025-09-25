/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Apply lights contribution to scene surfel representation.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define LIGHT_ITER_FORCE_NO_CULLING
#endif

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_light)

#include "eevee_closure_lib.glsl"
#include "eevee_light_eval_lib.glsl"

#ifndef LIGHT_ITER_FORCE_NO_CULLING
#  error light_eval_reflection argument assumes this is defined
#endif

void main()
{
  int index = int(gl_GlobalInvocationID.x);
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
