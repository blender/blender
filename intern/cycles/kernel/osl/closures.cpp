/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#include <OSL/genclosure.h>
#include <OSL/oslclosure.h>

#include "kernel/types.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"

#include "util/math.h"
#include "util/param.h"

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/geom/object.h"
#include "kernel/util/differential.h"

#include "kernel/osl/osl.h"

#include "kernel/osl/closures_setup.h"

#define TO_VEC3(v) OSL::Vec3(v.x, v.y, v.z)
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

CCL_NAMESPACE_BEGIN

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

void OSLRenderServices::register_closures(OSL::ShadingSystem *ss)
{
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  ss->register_closure( \
      #lower, OSL_CLOSURE_##Upper##_ID, osl_closure_##lower##_params(), nullptr, nullptr);

#include "closures_template.h"
}

/* Globals */

static void shaderdata_to_shaderglobals(const KernelGlobalsCPU *kg,
                                        ShaderData *sd,
                                        const void *state,
                                        uint32_t path_flag,
                                        OSLThreadData *tdata)
{
  OSL::ShaderGlobals *globals = &tdata->globals;

  const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
  const differential3 dI = differential_from_compact(sd->I, sd->dI);

  /* copy from shader data to shader globals */
  globals->P = TO_VEC3(sd->P);
  globals->dPdx = TO_VEC3(dP.dx);
  globals->dPdy = TO_VEC3(dP.dy);
  globals->I = TO_VEC3(sd->I);
  globals->dIdx = TO_VEC3(dI.dx);
  globals->dIdy = TO_VEC3(dI.dy);
  globals->N = TO_VEC3(sd->N);
  globals->Ng = TO_VEC3(sd->Ng);
  globals->u = sd->u;
  globals->dudx = sd->du.dx;
  globals->dudy = sd->du.dy;
  globals->v = sd->v;
  globals->dvdx = sd->dv.dx;
  globals->dvdy = sd->dv.dy;
  globals->dPdu = TO_VEC3(sd->dPdu);
  globals->dPdv = TO_VEC3(sd->dPdv);
  globals->surfacearea = 1.0f;
  globals->time = sd->time;

  /* booleans */
  globals->raytype = path_flag;
  globals->flipHandedness = 0;
  globals->backfacing = (sd->flag & SD_BACKFACING);

  /* shader data to be used in services callbacks */
  globals->renderstate = sd;

  /* hacky, we leave it to services to fetch actual object matrix */
  globals->shader2common = sd;
  globals->object2common = sd;

  /* must be set to NULL before execute */
  globals->Ci = NULL;

  /* clear trace data */
  tdata->tracedata.init = false;

  /* Used by render-services. */
  sd->osl_globals = kg;
  if (path_flag & PATH_RAY_SHADOW) {
    sd->osl_path_state = nullptr;
    sd->osl_shadow_path_state = (const IntegratorShadowStateCPU *)state;
  }
  else {
    sd->osl_path_state = (const IntegratorStateCPU *)state;
    sd->osl_shadow_path_state = nullptr;
  }
}

static void flatten_closure_tree(const KernelGlobalsCPU *kg,
                                 ShaderData *sd,
                                 uint32_t path_flag,
                                 const OSL::ClosureColor *closure,
                                 float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
  /* OSL gives us a closure tree, we flatten it into arrays per
   * closure type, for evaluation, sampling, etc later on. */

  switch (closure->id) {
    case OSL::ClosureColor::MUL: {
      OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
      flatten_closure_tree(kg, sd, path_flag, mul->closure, TO_FLOAT3(mul->weight) * weight);
      break;
    }
    case OSL::ClosureColor::ADD: {
      OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;
      flatten_closure_tree(kg, sd, path_flag, add->closureA, weight);
      flatten_closure_tree(kg, sd, path_flag, add->closureB, weight);
      break;
    }
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  case OSL_CLOSURE_##Upper##_ID: { \
    const OSL::ClosureComponent *comp = reinterpret_cast<const OSL::ClosureComponent *>(closure); \
    weight *= TO_FLOAT3(comp->w); \
    osl_closure_##lower##_setup( \
        kg, sd, path_flag, weight, reinterpret_cast<const Upper##Closure *>(comp + 1)); \
    break; \
  }
#include "closures_template.h"
    default:
      break;
  }
}

/* Surface */

void OSLShader::eval_surface(const KernelGlobalsCPU *kg,
                             const void *state,
                             ShaderData *sd,
                             uint32_t path_flag)
{
  /* setup shader globals from shader data */
  OSLThreadData *tdata = kg->osl_tdata;
  shaderdata_to_shaderglobals(kg, sd, state, path_flag, tdata);

  /* execute shader for this point */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSL::ShaderGlobals *globals = &tdata->globals;
  OSL::ShadingContext *octx = tdata->context;
  int shader = sd->shader & SHADER_MASK;

  /* automatic bump shader */
  if (kg->osl->bump_state[shader]) {
    /* save state */
    const float3 P = sd->P;
    const float dP = sd->dP;
    const OSL::Vec3 dPdx = globals->dPdx;
    const OSL::Vec3 dPdy = globals->dPdy;

    /* set state as if undisplaced */
    if (sd->flag & SD_HAS_DISPLACEMENT) {
      float data[9];
      bool found = kg->osl->services->get_attribute(sd,
                                                    true,
                                                    OSLRenderServices::u_empty,
                                                    TypeDesc::TypeVector,
                                                    OSLRenderServices::u_geom_undisplaced,
                                                    data);
      (void)found;
      assert(found);

      differential3 tmp_dP;
      memcpy(&sd->P, data, sizeof(float) * 3);
      memcpy(&tmp_dP.dx, data + 3, sizeof(float) * 3);
      memcpy(&tmp_dP.dy, data + 6, sizeof(float) * 3);

      object_position_transform(kg, sd, &sd->P);
      object_dir_transform(kg, sd, &tmp_dP.dx);
      object_dir_transform(kg, sd, &tmp_dP.dy);

      sd->dP = differential_make_compact(tmp_dP);

      globals->P = TO_VEC3(sd->P);
      globals->dPdx = TO_VEC3(tmp_dP.dx);
      globals->dPdy = TO_VEC3(tmp_dP.dy);
    }

    /* execute bump shader */
    ss->execute(octx, *(kg->osl->bump_state[shader]), *globals);

    /* reset state */
    sd->P = P;
    sd->dP = dP;

    globals->P = TO_VEC3(P);
    globals->dPdx = TO_VEC3(dPdx);
    globals->dPdy = TO_VEC3(dPdy);
  }

  /* surface shader */
  if (kg->osl->surface_state[shader]) {
    ss->execute(octx, *(kg->osl->surface_state[shader]), *globals);
  }

  /* flatten closure tree */
  if (globals->Ci) {
    flatten_closure_tree(kg, sd, path_flag, globals->Ci);
  }
}

/* Background */

void OSLShader::eval_background(const KernelGlobalsCPU *kg,
                                const void *state,
                                ShaderData *sd,
                                uint32_t path_flag)
{
  /* setup shader globals from shader data */
  OSLThreadData *tdata = kg->osl_tdata;
  shaderdata_to_shaderglobals(kg, sd, state, path_flag, tdata);

  /* execute shader for this point */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSL::ShaderGlobals *globals = &tdata->globals;
  OSL::ShadingContext *octx = tdata->context;

  if (kg->osl->background_state) {
    ss->execute(octx, *(kg->osl->background_state), *globals);
  }

  /* return background color immediately */
  if (globals->Ci) {
    flatten_closure_tree(kg, sd, path_flag, globals->Ci);
  }
}

/* Volume */

void OSLShader::eval_volume(const KernelGlobalsCPU *kg,
                            const void *state,
                            ShaderData *sd,
                            uint32_t path_flag)
{
  /* setup shader globals from shader data */
  OSLThreadData *tdata = kg->osl_tdata;
  shaderdata_to_shaderglobals(kg, sd, state, path_flag, tdata);

  /* execute shader */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSL::ShaderGlobals *globals = &tdata->globals;
  OSL::ShadingContext *octx = tdata->context;
  int shader = sd->shader & SHADER_MASK;

  if (kg->osl->volume_state[shader]) {
    ss->execute(octx, *(kg->osl->volume_state[shader]), *globals);
  }

  /* flatten closure tree */
  if (globals->Ci) {
    flatten_closure_tree(kg, sd, path_flag, globals->Ci);
  }
}

/* Displacement */

void OSLShader::eval_displacement(const KernelGlobalsCPU *kg, const void *state, ShaderData *sd)
{
  /* setup shader globals from shader data */
  OSLThreadData *tdata = kg->osl_tdata;
  shaderdata_to_shaderglobals(kg, sd, state, 0, tdata);

  /* execute shader */
  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSL::ShaderGlobals *globals = &tdata->globals;
  OSL::ShadingContext *octx = tdata->context;
  int shader = sd->shader & SHADER_MASK;

  if (kg->osl->displacement_state[shader]) {
    ss->execute(octx, *(kg->osl->displacement_state[shader]), *globals);
  }

  /* get back position */
  sd->P = TO_FLOAT3(globals->P);
}

CCL_NAMESPACE_END
