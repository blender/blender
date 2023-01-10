/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

/* OSL Shader Engine
 *
 * Holds all variables to execute and use OSL shaders from the kernel.
 */

#include "kernel/osl/types.h"

#include "kernel/osl/closures_setup.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void shaderdata_to_shaderglobals(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   uint32_t path_flag,
                                                   ccl_private ShaderGlobals *globals)
{
  const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
  const differential3 dI = differential_from_compact(sd->I, sd->dI);

  /* copy from shader data to shader globals */
  globals->P = sd->P;
  globals->dPdx = dP.dx;
  globals->dPdy = dP.dy;
  globals->I = sd->I;
  globals->dIdx = dI.dx;
  globals->dIdy = dI.dy;
  globals->N = sd->N;
  globals->Ng = sd->Ng;
  globals->u = sd->u;
  globals->dudx = sd->du.dx;
  globals->dudy = sd->du.dy;
  globals->v = sd->v;
  globals->dvdx = sd->dv.dx;
  globals->dvdy = sd->dv.dy;
  globals->dPdu = sd->dPdu;
  globals->dPdv = sd->dPdv;
  globals->time = sd->time;
  globals->dtime = 1.0f;
  globals->surfacearea = 1.0f;
  globals->raytype = path_flag;
  globals->flipHandedness = 0;
  globals->backfacing = (sd->flag & SD_BACKFACING);

  /* shader data to be used in services callbacks */
  globals->renderstate = sd;

  /* hacky, we leave it to services to fetch actual object matrix */
  globals->shader2common = sd;
  globals->object2common = sd;

  /* must be set to NULL before execute */
  globals->Ci = nullptr;
}

ccl_device void flatten_closure_tree(KernelGlobals kg,
                                     ccl_private ShaderData *sd,
                                     uint32_t path_flag,
                                     ccl_private const OSLClosure *closure)
{
  int stack_size = 0;
  float3 weight = one_float3();
  float3 weight_stack[16];
  ccl_private const OSLClosure *closure_stack[16];

  while (closure) {
    switch (closure->id) {
      case OSL_CLOSURE_MUL_ID: {
        ccl_private const OSLClosureMul *mul = static_cast<ccl_private const OSLClosureMul *>(
            closure);
        weight *= mul->weight;
        closure = mul->closure;
        continue;
      }
      case OSL_CLOSURE_ADD_ID: {
        if (stack_size >= 16) {
          kernel_assert(!"Exhausted OSL closure stack");
          break;
        }
        ccl_private const OSLClosureAdd *add = static_cast<ccl_private const OSLClosureAdd *>(
            closure);
        closure = add->closureA;
        weight_stack[stack_size] = weight;
        closure_stack[stack_size++] = add->closureB;
        continue;
      }
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  case OSL_CLOSURE_##Upper##_ID: { \
    ccl_private const OSLClosureComponent *comp = \
        static_cast<ccl_private const OSLClosureComponent *>(closure); \
    osl_closure_##lower##_setup(kg, \
                                sd, \
                                path_flag, \
                                weight * comp->weight, \
                                reinterpret_cast<ccl_private const Upper##Closure *>(comp + 1)); \
    break; \
  }
#include "closures_template.h"
      default:
        break;
    }

    if (stack_size > 0) {
      weight = weight_stack[--stack_size];
      closure = closure_stack[stack_size];
    }
    else {
      closure = nullptr;
    }
  }
}

#ifndef __KERNEL_GPU__

template<ShaderType type>
void osl_eval_nodes(const KernelGlobalsCPU *kg,
                    const void *state,
                    ShaderData *sd,
                    uint32_t path_flag);

#else

template<ShaderType type, typename ConstIntegratorGenericState>
ccl_device_inline void osl_eval_nodes(KernelGlobals kg,
                                      ConstIntegratorGenericState state,
                                      ccl_private ShaderData *sd,
                                      uint32_t path_flag)
{
  ShaderGlobals globals;
  shaderdata_to_shaderglobals(kg, sd, path_flag, &globals);

  const int shader = sd->shader & SHADER_MASK;

#  ifdef __KERNEL_OPTIX__
  uint8_t group_data[2048];
  uint8_t closure_pool[1024];
  sd->osl_closure_pool = closure_pool;

  unsigned int optix_dc_index = 2 /* NUM_CALLABLE_PROGRAM_GROUPS */ +
                                (shader + type * kernel_data.max_shaders) * 2;
  optixDirectCall<void>(optix_dc_index + 0,
                        /* shaderglobals_ptr = */ &globals,
                        /* groupdata_ptr = */ (void *)group_data,
                        /* userdata_base_ptr = */ (void *)nullptr,
                        /* output_base_ptr = */ (void *)nullptr,
                        /* shadeindex = */ 0);
  optixDirectCall<void>(optix_dc_index + 1,
                        /* shaderglobals_ptr = */ &globals,
                        /* groupdata_ptr = */ (void *)group_data,
                        /* userdata_base_ptr = */ (void *)nullptr,
                        /* output_base_ptr = */ (void *)nullptr,
                        /* shadeindex = */ 0);
#  endif

  if (globals.Ci) {
    flatten_closure_tree(kg, sd, path_flag, globals.Ci);
  }
}

#endif

CCL_NAMESPACE_END
