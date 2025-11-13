/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

/* OSL Shader Engine
 *
 * Holds all variables to execute and use OSL shaders from the kernel.
 */

#ifdef __KERNEL_OPTIX__
#  include "kernel/geom/attribute.h"
#  include "kernel/geom/primitive.h"
#endif

#include "kernel/osl/closures_setup.h"
#include "kernel/osl/types.h"

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void shaderdata_to_shaderglobals(ccl_private ShaderData *sd,
                                                   const uint32_t path_flag,
                                                   ccl_private ShaderGlobals *globals)
{
  const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
  const differential3 dI = differential_from_compact(sd->wi, sd->dI);

  /* copy from shader data to shader globals */
  globals->P = sd->P;
  globals->dPdx = dP.dx;
  globals->dPdy = dP.dy;
  globals->I = sd->wi;
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
  globals->sd = sd;
  globals->shadingStateUniform = nullptr;
  globals->thread_index = 0;
  globals->shade_index = 0;

  /* hacky, we leave it to services to fetch actual object matrix */
  globals->shader2common = sd;
  globals->object2common = sd;

  /* must be set to nullptr before execute */
  globals->Ci = nullptr;
}

ccl_device void flatten_closure_tree(KernelGlobals kg,
                                     ccl_private ShaderData *sd,
                                     const uint32_t path_flag,
                                     const ccl_private OSLClosure *closure)
{
  int stack_size = 0;
  float3 weight = one_float3();
  float3 weight_stack[16];
  const ccl_private OSLClosure *closure_stack[16];
  int layer_stack_level = -1;
  float3 layer_albedo = zero_float3();

  while (true) {
    switch (closure->id) {
      case OSL_CLOSURE_MUL_ID: {
        const ccl_private OSLClosureMul *mul = static_cast<const ccl_private OSLClosureMul *>(
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
        const ccl_private OSLClosureAdd *add = static_cast<const ccl_private OSLClosureAdd *>(
            closure);
        closure = add->closureA;
        weight_stack[stack_size] = weight;
        closure_stack[stack_size++] = add->closureB;
        continue;
      }
      case OSL_CLOSURE_LAYER_ID: {
        const ccl_private OSLClosureComponent *comp =
            static_cast<const ccl_private OSLClosureComponent *>(closure);
        const ccl_private LayerClosure *layer = reinterpret_cast<const ccl_private LayerClosure *>(
            comp + 1);

        /* Layer closures may not appear in the top layer subtree of another layer closure. */
        kernel_assert(layer_stack_level == -1);

        if (layer->top != nullptr) {
          /* Push base layer onto the stack, will be handled after the top layers */
          weight_stack[stack_size] = weight;
          closure_stack[stack_size] = layer->base;
          /* Start accumulating albedo of the top layers */
          layer_stack_level = stack_size++;
          layer_albedo = zero_float3();
          /* Continue with the top layers */
          closure = layer->top;
        }
        else {
          /* No top layer, just continue with base. */
          closure = layer->base;
        }
        continue;
      }
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  case OSL_CLOSURE_##Upper##_ID: { \
    ccl_private const OSLClosureComponent *comp = \
        static_cast<ccl_private const OSLClosureComponent *>(closure); \
    float3 albedo = one_float3(); \
    osl_closure_##lower##_setup(kg, \
                                sd, \
                                path_flag, \
                                weight * comp->weight, \
                                reinterpret_cast<ccl_private const Upper##Closure *>(comp + 1), \
                                (layer_stack_level >= 0) ? &albedo : nullptr); \
    if (layer_stack_level >= 0) { \
      layer_albedo += albedo; \
    } \
    break; \
  }
#include "closures_template.h"
      default:
        break;
    }

    /* Pop the next closure from the stack (or return if we're done). */
    do {
      if (stack_size == 0) {
        return;
      }

      weight = weight_stack[--stack_size];
      closure = closure_stack[stack_size];
      if (stack_size == layer_stack_level) {
        /* We just finished processing the top layers of a Layer closure, so adjust the weight to
         * account for the layering. */
        weight = closure_layering_weight(layer_albedo, weight);
        layer_stack_level = -1;
        /* If it's fully occluded, skip the base layer we just popped from the stack and grab
         * the next entry instead. */
        if (is_zero(weight)) {
          continue;
        }
      }
    } while (closure == nullptr);
  }
}

#ifndef __KERNEL_GPU__

template<ShaderType type>
void osl_eval_nodes(const ThreadKernelGlobalsCPU *kg,
                    const void *state,
                    ShaderData *sd,
                    uint32_t path_flag);

#else

template<ShaderType type, typename ConstIntegratorGenericState>
ccl_device_inline void osl_eval_nodes(KernelGlobals kg,
                                      ConstIntegratorGenericState state,
                                      ccl_private ShaderData *sd,
                                      const uint32_t path_flag)
{
  ShaderGlobals globals;
  shaderdata_to_shaderglobals(sd, path_flag, &globals);

  const int shader = sd->shader & SHADER_MASK;

#  ifdef __KERNEL_OPTIX__
  uint8_t closure_pool[1024];
  globals.closure_pool = closure_pool;
  if (path_flag & PATH_RAY_SHADOW) {
    globals.shade_index = -state - 1;
  }
  else {
    globals.shade_index = state + 1;
  }

  /* For surface shaders, we might have an automatic bump shader that needs to be executed before
   * the main shader to update globals.N. */
  if constexpr (type == SHADER_TYPE_SURFACE) {
    if (sd->flag & SD_HAS_BUMP) {
      /* Save state. */
      const float3 P = sd->P;
      const float dP = sd->dP;
      const packed_float3 dPdx = globals.dPdx;
      const packed_float3 dPdy = globals.dPdy;

      /* Set position state as if undisplaced. */
      if (sd->flag & SD_HAS_DISPLACEMENT) {
        const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);
        kernel_assert(desc.offset != ATTR_STD_NOT_FOUND);

        dual3 P = primitive_surface_attribute<float3>(kg, sd, desc, true, true);

        object_position_transform(kg, sd, &P);

        sd->P = P.val;
        sd->dP = differential_make_compact(P);

        globals.P = sd->P;
        globals.dPdx = P.dx;
        globals.dPdy = P.dy;

        /* Set normal as if undisplaced. */
        const AttributeDescriptor ndesc = find_attribute(kg, sd, ATTR_STD_NORMAL_UNDISPLACED);
        if (ndesc.offset != ATTR_STD_NOT_FOUND) {
          float3 N = safe_normalize(
              primitive_surface_attribute<float3>(kg, sd, ndesc, false, false).val);
          object_normal_transform(kg, sd, &N);
          sd->N = (sd->flag & SD_BACKFACING) ? -N : N;
          globals.N = sd->N;
        }
      }

      /* Execute bump shader. */
      unsigned int optix_dc_index = 2 /* NUM_CALLABLE_PROGRAM_GROUPS */ + 1 /* camera program */ +
                                    (shader + SHADER_TYPE_BUMP * kernel_data.max_shaders);
      optixDirectCall<void>(optix_dc_index,
                            /* shaderglobals_ptr = */ &globals,
                            /* groupdata_ptr = */ (void *)nullptr,
                            /* userdata_base_ptr = */ (void *)nullptr,
                            /* output_base_ptr = */ (void *)nullptr,
                            /* shadeindex = */ 0,
                            /* interactive_params_ptr */ (void *)nullptr);

      /* Reset state. */
      sd->P = P;
      sd->dP = dP;

      /* Apply bump output to sd->N since it's used for shadow terminator logic, for example. */
      sd->N = globals.N;

      globals.P = P;
      globals.dPdx = dPdx;
      globals.dPdy = dPdy;
    }
  }

  unsigned int optix_dc_index = 2 /* NUM_CALLABLE_PROGRAM_GROUPS */ + 1 /* camera program */ +
                                (shader + type * kernel_data.max_shaders);
  optixDirectCall<void>(optix_dc_index,
                        /* shaderglobals_ptr = */ &globals,
                        /* groupdata_ptr = */ (void *)nullptr,
                        /* userdata_base_ptr = */ (void *)nullptr,
                        /* output_base_ptr = */ (void *)nullptr,
                        /* shadeindex = */ 0,
                        /* interactive_params_ptr */ (void *)nullptr);
#  endif

  if constexpr (type == SHADER_TYPE_DISPLACEMENT) {
    sd->P = globals.P;
  }
  else if (globals.Ci) {
    flatten_closure_tree(kg, sd, path_flag, globals.Ci);
  }
}

#endif

CCL_NAMESPACE_END
