/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#include <OSL/genclosure.h>
#include <OSL/oslclosure.h>

#include "kernel/types.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"

#include "util/math.h"
#include "util/param.h"

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"
#include "kernel/util/differential.h"

#include "kernel/osl/camera.h"
#include "kernel/osl/osl.h"

#define TO_VEC3(v) OSL::Vec3(v.x, v.y, v.z)
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

CCL_NAMESPACE_BEGIN

static_assert(sizeof(OSLClosure) == sizeof(OSL::ClosureColor) &&
              sizeof(OSLClosureAdd) == sizeof(OSL::ClosureAdd) &&
              sizeof(OSLClosureMul) == sizeof(OSL::ClosureMul) &&
              sizeof(OSLClosureComponent) == sizeof(OSL::ClosureComponent));
static_assert(sizeof(ShaderGlobals) >= sizeof(OSL::ShaderGlobals) &&
              offsetof(ShaderGlobals, backfacing) == offsetof(OSL::ShaderGlobals, backfacing));

/* Registration */

#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  static OSL::ClosureParam *osl_closure_##lower##_params() \
  { \
    static OSL::ClosureParam params[] = {
#define OSL_CLOSURE_STRUCT_END(Upper, lower) \
  CLOSURE_STRING_KEYPARAM(Upper##Closure, label, "label"), CLOSURE_FINISH_PARAM(Upper##Closure) \
  } \
  ; \
  return params; \
  }
#define OSL_CLOSURE_STRUCT_MEMBER(Upper, TYPE, type, name, key) \
  CLOSURE_##TYPE##_KEYPARAM(Upper##Closure, name, key),
#define OSL_CLOSURE_STRUCT_ARRAY_MEMBER(Upper, TYPE, type, name, key, size) \
  CLOSURE_##TYPE##_ARRAY_PARAM(Upper##Closure, name, size),

#include "closures_template.h"

static OSL::ClosureParam *osl_closure_layer_params()
{
  static OSL::ClosureParam params[] = {CLOSURE_CLOSURE_PARAM(LayerClosure, top),
                                       CLOSURE_CLOSURE_PARAM(LayerClosure, base),
                                       CLOSURE_FINISH_PARAM(LayerClosure)};
  return params;
}

void OSLRenderServices::register_closures(OSL::ShadingSystem *ss)
{
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  ss->register_closure( \
      #lower, OSL_CLOSURE_##Upper##_ID, osl_closure_##lower##_params(), nullptr, nullptr);

#include "closures_template.h"
  ss->register_closure(
      "layer", OSL_CLOSURE_LAYER_ID, osl_closure_layer_params(), nullptr, nullptr);
}

/* Surface & Background */

template<>
void osl_eval_nodes<SHADER_TYPE_SURFACE>(const ThreadKernelGlobalsCPU *kg,
                                         const void *state,
                                         ShaderData *sd,
                                         const uint32_t path_flag)
{
  /* setup shader globals from shader data */
  shaderdata_to_shaderglobals(sd, path_flag, &kg->osl.shader_globals);

  /* clear trace data */
  kg->osl.tracedata.init = false;

  /* Used by render-services. */
  kg->osl.shader_globals.kg = kg;
  if (path_flag & PATH_RAY_SHADOW) {
    kg->osl.shader_globals.path_state = nullptr;
    kg->osl.shader_globals.shadow_path_state = (const IntegratorShadowStateCPU *)state;
  }
  else {
    kg->osl.shader_globals.path_state = (const IntegratorStateCPU *)state;
    kg->osl.shader_globals.shadow_path_state = nullptr;
  }

  /* execute shader for this point */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl.ss;
  OSL::ShaderGlobals *globals = reinterpret_cast<OSL::ShaderGlobals *>(&kg->osl.shader_globals);
  OSL::ShadingContext *octx = kg->osl.context;
  const int shader = sd->shader & SHADER_MASK;

  if (sd->object == OBJECT_NONE) {
    /* background */
    if (kg->osl.globals->background_state) {
      ss->execute(*octx,
                  *(kg->osl.globals->background_state),
                  kg->osl.thread_index,
                  0,
                  *globals,
                  nullptr,
                  nullptr);
    }
  }
  else {
    /* automatic bump shader */
    if (kg->osl.globals->bump_state[shader]) {
      /* save state */
      const float3 P = sd->P;
      const float dP = sd->dP;
      const OSL::Vec3 dPdx = globals->dPdx;
      const OSL::Vec3 dPdy = globals->dPdy;

      /* set state as if undisplaced */
      if (sd->flag & SD_HAS_DISPLACEMENT) {
        const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);
        kernel_assert(desc.offset != ATTR_STD_NOT_FOUND);

        dual3 P = primitive_surface_attribute<float3>(kg, sd, desc, true, true);
        object_position_transform(kg, sd, &P);

        sd->P = P.val;
        sd->dP = differential_make_compact(P);

        globals->P = TO_VEC3(sd->P);
        globals->dPdx = TO_VEC3(P.dx);
        globals->dPdy = TO_VEC3(P.dy);

        /* Set normal as if undisplaced. */
        const AttributeDescriptor ndesc = find_attribute(kg, sd, ATTR_STD_NORMAL_UNDISPLACED);
        if (ndesc.offset != ATTR_STD_NOT_FOUND) {
          float3 N = safe_normalize(
              primitive_surface_attribute<float3>(kg, sd, ndesc, false, false).val);
          object_normal_transform(kg, sd, &N);
          sd->N = (sd->flag & SD_BACKFACING) ? -N : N;
          globals->N = TO_VEC3(sd->N);
        }
      }

      /* execute bump shader */
      ss->execute(*octx,
                  *(kg->osl.globals->bump_state[shader]),
                  kg->osl.thread_index,
                  0,
                  *globals,
                  nullptr,
                  nullptr);

      /* reset state */
      sd->P = P;
      sd->dP = dP;

      /* Apply bump output to sd->N since it's used for shadow terminator logic, for example. */
      sd->N = TO_FLOAT3(globals->N);

      globals->P = TO_VEC3(P);
      globals->dPdx = TO_VEC3(dPdx);
      globals->dPdy = TO_VEC3(dPdy);
    }

    /* surface shader */
    if (kg->osl.globals->surface_state[shader]) {
      ss->execute(*octx,
                  *(kg->osl.globals->surface_state[shader]),
                  kg->osl.thread_index,
                  0,
                  *globals,
                  nullptr,
                  nullptr);
    }
  }

  /* flatten closure tree */
  if (kg->osl.shader_globals.Ci) {
    flatten_closure_tree(kg, sd, path_flag, kg->osl.shader_globals.Ci);
  }
}

/* Volume */

template<>
void osl_eval_nodes<SHADER_TYPE_VOLUME>(const ThreadKernelGlobalsCPU *kg,
                                        const void *state,
                                        ShaderData *sd,
                                        const uint32_t path_flag)
{
  /* setup shader globals from shader data */
  shaderdata_to_shaderglobals(sd, path_flag, &kg->osl.shader_globals);

  /* clear trace data */
  kg->osl.tracedata.init = false;

  /* Used by render-services. */
  kg->osl.shader_globals.kg = kg;
  if (path_flag & PATH_RAY_SHADOW) {
    kg->osl.shader_globals.path_state = nullptr;
    kg->osl.shader_globals.shadow_path_state = (const IntegratorShadowStateCPU *)state;
  }
  else {
    kg->osl.shader_globals.path_state = (const IntegratorStateCPU *)state;
    kg->osl.shader_globals.shadow_path_state = nullptr;
  }

  /* execute shader */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl.ss;
  OSL::ShaderGlobals *globals = reinterpret_cast<OSL::ShaderGlobals *>(&kg->osl.shader_globals);
  OSL::ShadingContext *octx = kg->osl.context;
  const int shader = sd->shader & SHADER_MASK;

  if (kg->osl.globals->volume_state[shader]) {
    ss->execute(*octx,
                *(kg->osl.globals->volume_state[shader]),
                kg->osl.thread_index,
                0,
                *globals,
                nullptr,
                nullptr);
  }

  /* flatten closure tree */
  if (kg->osl.shader_globals.Ci) {
    flatten_closure_tree(kg, sd, path_flag, kg->osl.shader_globals.Ci);
  }
}

/* Displacement */

template<>
void osl_eval_nodes<SHADER_TYPE_DISPLACEMENT>(const ThreadKernelGlobalsCPU *kg,
                                              const void *state,
                                              ShaderData *sd,
                                              const uint32_t path_flag)
{
  /* setup shader globals from shader data */
  shaderdata_to_shaderglobals(sd, path_flag, &kg->osl.shader_globals);

  /* clear trace data */
  kg->osl.tracedata.init = false;

  /* Used by render-services. */
  kg->osl.shader_globals.kg = kg;
  kg->osl.shader_globals.path_state = (const IntegratorStateCPU *)state;
  kg->osl.shader_globals.shadow_path_state = nullptr;

  /* execute shader */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl.ss;
  OSL::ShaderGlobals *globals = reinterpret_cast<OSL::ShaderGlobals *>(&kg->osl.shader_globals);
  OSL::ShadingContext *octx = kg->osl.context;
  const int shader = sd->shader & SHADER_MASK;

  if (kg->osl.globals->displacement_state[shader]) {
    ss->execute(*octx,
                *(kg->osl.globals->displacement_state[shader]),
                kg->osl.thread_index,
                0,
                *globals,
                nullptr,
                nullptr);
  }

  /* get back position */
  sd->P = TO_FLOAT3(globals->P);
}

/* Camera */

packed_float3 osl_eval_camera(const ThreadKernelGlobalsCPU *kg,
                              const packed_float3 sensor,
                              const packed_float3 dSdx,
                              const packed_float3 dSdy,
                              const float2 rand_lens,
                              packed_float3 &P,
                              packed_float3 &dPdx,
                              packed_float3 &dPdy,
                              packed_float3 &D,
                              packed_float3 &dDdx,
                              packed_float3 &dDdy)
{
  if (!kg->osl.globals->camera_state) {
    return zero_spectrum();
  }

  /* Setup shader globals from the sensor position. */
  cameradata_to_shaderglobals(sensor, dSdx, dSdy, rand_lens, &kg->osl.shader_globals);

  /* Clear trace data. */
  kg->osl.tracedata.init = false;

  /* Provide kernel globals to the render-services. */
  kg->osl.shader_globals.kg = kg;

  /* Execute the shader. */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl.ss;
  OSL::ShaderGlobals *globals = reinterpret_cast<OSL::ShaderGlobals *>(&kg->osl.shader_globals);
  OSL::ShadingContext *octx = kg->osl.context;

  float output[21] = {0.0f};

  ss->execute(
      *octx, *kg->osl.globals->camera_state, kg->osl.thread_index, 0, *globals, nullptr, output);

  P = make_float3(output[0], output[1], output[2]);
  dPdx = make_float3(output[3], output[4], output[5]);
  dPdy = make_float3(output[6], output[7], output[8]);
  D = make_float3(output[9], output[10], output[11]);
  dDdx = make_float3(output[12], output[13], output[14]);
  dDdy = make_float3(output[15], output[16], output[17]);
  return make_float3(output[18], output[19], output[20]);
}

CCL_NAMESPACE_END
