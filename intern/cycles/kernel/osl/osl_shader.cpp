/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <OSL/oslexec.h>

#include "kernel/kernel_compat_cpu.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data_types.h"
#include "kernel/kernel_globals.h"

#include "kernel/geom/geom_object.h"

#include "kernel/osl/osl_closures.h"
#include "kernel/osl/osl_globals.h"
#include "kernel/osl/osl_services.h"
#include "kernel/osl/osl_shader.h"

#include "util/util_foreach.h"

#include "render/attribute.h"

CCL_NAMESPACE_BEGIN

/* Threads */

void OSLShader::thread_init(KernelGlobals *kg,
                            KernelGlobals *kernel_globals,
                            OSLGlobals *osl_globals)
{
  /* no osl used? */
  if (!osl_globals->use) {
    kg->osl = NULL;
    return;
  }

  /* per thread kernel data init*/
  kg->osl = osl_globals;

  OSL::ShadingSystem *ss = kg->osl->ss;
  OSLThreadData *tdata = new OSLThreadData();

  memset((void *)&tdata->globals, 0, sizeof(OSL::ShaderGlobals));
  tdata->globals.tracedata = &tdata->tracedata;
  tdata->globals.flipHandedness = false;
  tdata->osl_thread_info = ss->create_thread_info();
  tdata->context = ss->get_context(tdata->osl_thread_info);

  tdata->oiio_thread_info = osl_globals->ts->get_perthread_info();

  kg->osl_ss = (OSLShadingSystem *)ss;
  kg->osl_tdata = tdata;
}

void OSLShader::thread_free(KernelGlobals *kg)
{
  if (!kg->osl)
    return;

  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSLThreadData *tdata = kg->osl_tdata;
  ss->release_context(tdata->context);

  ss->destroy_thread_info(tdata->osl_thread_info);

  delete tdata;

  kg->osl = NULL;
  kg->osl_ss = NULL;
  kg->osl_tdata = NULL;
}

/* Globals */

static void shaderdata_to_shaderglobals(
    KernelGlobals *kg, ShaderData *sd, PathState *state, int path_flag, OSLThreadData *tdata)
{
  OSL::ShaderGlobals *globals = &tdata->globals;

  /* copy from shader data to shader globals */
  globals->P = TO_VEC3(sd->P);
  globals->dPdx = TO_VEC3(sd->dP.dx);
  globals->dPdy = TO_VEC3(sd->dP.dy);
  globals->I = TO_VEC3(sd->I);
  globals->dIdx = TO_VEC3(sd->dI.dx);
  globals->dIdy = TO_VEC3(sd->dI.dy);
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
  globals->surfacearea = (sd->object == OBJECT_NONE) ? 1.0f : object_surface_area(kg, sd->object);
  globals->time = sd->time;

  /* booleans */
  globals->raytype = path_flag;
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

  /* used by renderservices */
  sd->osl_globals = kg;
  sd->osl_path_state = state;
}

/* Surface */

static void flatten_surface_closure_tree(ShaderData *sd,
                                         int path_flag,
                                         const OSL::ClosureColor *closure,
                                         float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
  /* OSL gives us a closure tree, we flatten it into arrays per
   * closure type, for evaluation, sampling, etc later on. */

  switch (closure->id) {
    case OSL::ClosureColor::MUL: {
      OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
      flatten_surface_closure_tree(sd, path_flag, mul->closure, TO_FLOAT3(mul->weight) * weight);
      break;
    }
    case OSL::ClosureColor::ADD: {
      OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;
      flatten_surface_closure_tree(sd, path_flag, add->closureA, weight);
      flatten_surface_closure_tree(sd, path_flag, add->closureB, weight);
      break;
    }
    default: {
      OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
      CClosurePrimitive *prim = (CClosurePrimitive *)comp->data();

      if (prim) {
#ifdef OSL_SUPPORTS_WEIGHTED_CLOSURE_COMPONENTS
        weight = weight * TO_FLOAT3(comp->w);
#endif
        prim->setup(sd, path_flag, weight);
      }
      break;
    }
  }
}

void OSLShader::eval_surface(KernelGlobals *kg, ShaderData *sd, PathState *state, int path_flag)
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
    float3 P = sd->P;
    float3 dPdx = sd->dP.dx;
    float3 dPdy = sd->dP.dy;

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

      memcpy(&sd->P, data, sizeof(float) * 3);
      memcpy(&sd->dP.dx, data + 3, sizeof(float) * 3);
      memcpy(&sd->dP.dy, data + 6, sizeof(float) * 3);

      object_position_transform(kg, sd, &sd->P);
      object_dir_transform(kg, sd, &sd->dP.dx);
      object_dir_transform(kg, sd, &sd->dP.dy);

      globals->P = TO_VEC3(sd->P);
      globals->dPdx = TO_VEC3(sd->dP.dx);
      globals->dPdy = TO_VEC3(sd->dP.dy);
    }

    /* execute bump shader */
    ss->execute(octx, *(kg->osl->bump_state[shader]), *globals);

    /* reset state */
    sd->P = P;
    sd->dP.dx = dPdx;
    sd->dP.dy = dPdy;

    globals->P = TO_VEC3(P);
    globals->dPdx = TO_VEC3(dPdx);
    globals->dPdy = TO_VEC3(dPdy);
  }

  /* surface shader */
  if (kg->osl->surface_state[shader]) {
    ss->execute(octx, *(kg->osl->surface_state[shader]), *globals);
  }

  /* flatten closure tree */
  if (globals->Ci)
    flatten_surface_closure_tree(sd, path_flag, globals->Ci);
}

/* Background */

static void flatten_background_closure_tree(ShaderData *sd,
                                            const OSL::ClosureColor *closure,
                                            float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
  /* OSL gives us a closure tree, if we are shading for background there
   * is only one supported closure type at the moment, which has no evaluation
   * functions, so we just sum the weights */

  switch (closure->id) {
    case OSL::ClosureColor::MUL: {
      OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
      flatten_background_closure_tree(sd, mul->closure, weight * TO_FLOAT3(mul->weight));
      break;
    }
    case OSL::ClosureColor::ADD: {
      OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;

      flatten_background_closure_tree(sd, add->closureA, weight);
      flatten_background_closure_tree(sd, add->closureB, weight);
      break;
    }
    default: {
      OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
      CClosurePrimitive *prim = (CClosurePrimitive *)comp->data();

      if (prim) {
#ifdef OSL_SUPPORTS_WEIGHTED_CLOSURE_COMPONENTS
        weight = weight * TO_FLOAT3(comp->w);
#endif
        prim->setup(sd, 0, weight);
      }
      break;
    }
  }
}

void OSLShader::eval_background(KernelGlobals *kg, ShaderData *sd, PathState *state, int path_flag)
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
  if (globals->Ci)
    flatten_background_closure_tree(sd, globals->Ci);
}

/* Volume */

static void flatten_volume_closure_tree(ShaderData *sd,
                                        const OSL::ClosureColor *closure,
                                        float3 weight = make_float3(1.0f, 1.0f, 1.0f))
{
  /* OSL gives us a closure tree, we flatten it into arrays per
   * closure type, for evaluation, sampling, etc later on. */

  switch (closure->id) {
    case OSL::ClosureColor::MUL: {
      OSL::ClosureMul *mul = (OSL::ClosureMul *)closure;
      flatten_volume_closure_tree(sd, mul->closure, TO_FLOAT3(mul->weight) * weight);
      break;
    }
    case OSL::ClosureColor::ADD: {
      OSL::ClosureAdd *add = (OSL::ClosureAdd *)closure;
      flatten_volume_closure_tree(sd, add->closureA, weight);
      flatten_volume_closure_tree(sd, add->closureB, weight);
      break;
    }
    default: {
      OSL::ClosureComponent *comp = (OSL::ClosureComponent *)closure;
      CClosurePrimitive *prim = (CClosurePrimitive *)comp->data();

      if (prim) {
#ifdef OSL_SUPPORTS_WEIGHTED_CLOSURE_COMPONENTS
        weight = weight * TO_FLOAT3(comp->w);
#endif
        prim->setup(sd, 0, weight);
      }
    }
  }
}

void OSLShader::eval_volume(KernelGlobals *kg, ShaderData *sd, PathState *state, int path_flag)
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
  if (globals->Ci)
    flatten_volume_closure_tree(sd, globals->Ci);
}

/* Displacement */

void OSLShader::eval_displacement(KernelGlobals *kg, ShaderData *sd, PathState *state)
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

/* Attributes */

int OSLShader::find_attribute(KernelGlobals *kg,
                              const ShaderData *sd,
                              uint id,
                              AttributeDescriptor *desc)
{
  /* for OSL, a hash map is used to lookup the attribute by name. */
  int object = sd->object * ATTR_PRIM_TYPES;
#ifdef __HAIR__
  if (sd->type & PRIMITIVE_ALL_CURVE)
    object += ATTR_PRIM_CURVE;
#endif

  OSLGlobals::AttributeMap &attr_map = kg->osl->attribute_map[object];
  ustring stdname(std::string("geom:") +
                  std::string(Attribute::standard_name((AttributeStandard)id)));
  OSLGlobals::AttributeMap::const_iterator it = attr_map.find(stdname);

  if (it != attr_map.end()) {
    const OSLGlobals::Attribute &osl_attr = it->second;
    *desc = osl_attr.desc;

    if (sd->prim == PRIM_NONE && (AttributeElement)osl_attr.desc.element != ATTR_ELEMENT_MESH) {
      desc->offset = ATTR_STD_NOT_FOUND;
      return ATTR_STD_NOT_FOUND;
    }

    /* return result */
    if (osl_attr.desc.element == ATTR_ELEMENT_NONE) {
      desc->offset = ATTR_STD_NOT_FOUND;
    }
    return desc->offset;
  }
  else {
    desc->offset = ATTR_STD_NOT_FOUND;
    return (int)ATTR_STD_NOT_FOUND;
  }
}

CCL_NAMESPACE_END
