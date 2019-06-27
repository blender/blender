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

/* TODO(sergey): There is a bit of headers dependency hell going on
 * here, so for now we just put here. In the future it might be better
 * to have dedicated file for such tweaks.
 */
#if (defined(__GNUC__) && !defined(__clang__)) && defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include <string.h>

#include "render/colorspace.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"

#include "kernel/osl/osl_closures.h"
#include "kernel/osl/osl_globals.h"
#include "kernel/osl/osl_services.h"
#include "kernel/osl/osl_shader.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_string.h"

#include "kernel/kernel_compat_cpu.h"
#include "kernel/split/kernel_split_data_types.h"
#include "kernel/kernel_globals.h"
#include "kernel/kernel_color.h"
#include "kernel/kernel_random.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_differential.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_camera.h"
#include "kernel/kernels/cpu/kernel_cpu_image.h"
#include "kernel/geom/geom.h"
#include "kernel/bvh/bvh.h"

#include "kernel/kernel_projection.h"
#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_shader.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

static void copy_matrix(OSL::Matrix44 &m, const Transform &tfm)
{
  ProjectionTransform t = projection_transpose(ProjectionTransform(tfm));
  memcpy((void *)&m, &t, sizeof(m));
}

static void copy_matrix(OSL::Matrix44 &m, const ProjectionTransform &tfm)
{
  ProjectionTransform t = projection_transpose(tfm);
  memcpy((void *)&m, &t, sizeof(m));
}

/* static ustrings */
ustring OSLRenderServices::u_distance("distance");
ustring OSLRenderServices::u_index("index");
ustring OSLRenderServices::u_world("world");
ustring OSLRenderServices::u_camera("camera");
ustring OSLRenderServices::u_screen("screen");
ustring OSLRenderServices::u_raster("raster");
ustring OSLRenderServices::u_ndc("NDC");
ustring OSLRenderServices::u_object_location("object:location");
ustring OSLRenderServices::u_object_index("object:index");
ustring OSLRenderServices::u_geom_dupli_generated("geom:dupli_generated");
ustring OSLRenderServices::u_geom_dupli_uv("geom:dupli_uv");
ustring OSLRenderServices::u_material_index("material:index");
ustring OSLRenderServices::u_object_random("object:random");
ustring OSLRenderServices::u_particle_index("particle:index");
ustring OSLRenderServices::u_particle_random("particle:random");
ustring OSLRenderServices::u_particle_age("particle:age");
ustring OSLRenderServices::u_particle_lifetime("particle:lifetime");
ustring OSLRenderServices::u_particle_location("particle:location");
ustring OSLRenderServices::u_particle_rotation("particle:rotation");
ustring OSLRenderServices::u_particle_size("particle:size");
ustring OSLRenderServices::u_particle_velocity("particle:velocity");
ustring OSLRenderServices::u_particle_angular_velocity("particle:angular_velocity");
ustring OSLRenderServices::u_geom_numpolyvertices("geom:numpolyvertices");
ustring OSLRenderServices::u_geom_trianglevertices("geom:trianglevertices");
ustring OSLRenderServices::u_geom_polyvertices("geom:polyvertices");
ustring OSLRenderServices::u_geom_name("geom:name");
ustring OSLRenderServices::u_geom_undisplaced("geom:undisplaced");
ustring OSLRenderServices::u_is_smooth("geom:is_smooth");
ustring OSLRenderServices::u_is_curve("geom:is_curve");
ustring OSLRenderServices::u_curve_thickness("geom:curve_thickness");
ustring OSLRenderServices::u_curve_tangent_normal("geom:curve_tangent_normal");
ustring OSLRenderServices::u_curve_random("geom:curve_random");
ustring OSLRenderServices::u_path_ray_length("path:ray_length");
ustring OSLRenderServices::u_path_ray_depth("path:ray_depth");
ustring OSLRenderServices::u_path_diffuse_depth("path:diffuse_depth");
ustring OSLRenderServices::u_path_glossy_depth("path:glossy_depth");
ustring OSLRenderServices::u_path_transparent_depth("path:transparent_depth");
ustring OSLRenderServices::u_path_transmission_depth("path:transmission_depth");
ustring OSLRenderServices::u_trace("trace");
ustring OSLRenderServices::u_hit("hit");
ustring OSLRenderServices::u_hitdist("hitdist");
ustring OSLRenderServices::u_N("N");
ustring OSLRenderServices::u_Ng("Ng");
ustring OSLRenderServices::u_P("P");
ustring OSLRenderServices::u_I("I");
ustring OSLRenderServices::u_u("u");
ustring OSLRenderServices::u_v("v");
ustring OSLRenderServices::u_empty;

OSLRenderServices::OSLRenderServices(OSL::TextureSystem *texture_system)
    : texture_system(texture_system)
{
}

OSLRenderServices::~OSLRenderServices()
{
  if (texture_system) {
    VLOG(2) << "OSL texture system stats:\n" << texture_system->getstats();
  }
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSL::TransformationPtr xform,
                                   float time)
{
  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  if (xform) {
    const ShaderData *sd = (const ShaderData *)xform;
    KernelGlobals *kg = sd->osl_globals;
    int object = sd->object;

    if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
      Transform tfm;

      if (time == sd->time)
        tfm = sd->ob_tfm;
      else
        tfm = object_fetch_transform_motion_test(kg, object, time, NULL);
#else
      Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
      copy_matrix(result, tfm);

      return true;
    }
    else if (sd->type == PRIMITIVE_LAMP) {
      copy_matrix(result, sd->ob_tfm);

      return true;
    }
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSL::TransformationPtr xform,
                                           float time)
{
  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  if (xform) {
    const ShaderData *sd = (const ShaderData *)xform;
    KernelGlobals *kg = sd->osl_globals;
    int object = sd->object;

    if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
      Transform itfm;

      if (time == sd->time)
        itfm = sd->ob_itfm;
      else
        object_fetch_transform_motion_test(kg, object, time, &itfm);
#else
      Transform itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif
      copy_matrix(result, itfm);

      return true;
    }
    else if (sd->type == PRIMITIVE_LAMP) {
      copy_matrix(result, sd->ob_itfm);

      return true;
    }
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   ustring from,
                                   float time)
{
  ShaderData *sd = (ShaderData *)(sg->renderstate);
  KernelGlobals *kg = sd->osl_globals;

  if (from == u_ndc) {
    copy_matrix(result, kernel_data.cam.ndctoworld);
    return true;
  }
  else if (from == u_raster) {
    copy_matrix(result, kernel_data.cam.rastertoworld);
    return true;
  }
  else if (from == u_screen) {
    copy_matrix(result, kernel_data.cam.screentoworld);
    return true;
  }
  else if (from == u_camera) {
    copy_matrix(result, kernel_data.cam.cameratoworld);
    return true;
  }
  else if (from == u_world) {
    result.makeIdentity();
    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           ustring to,
                                           float time)
{
  ShaderData *sd = (ShaderData *)(sg->renderstate);
  KernelGlobals *kg = sd->osl_globals;

  if (to == u_ndc) {
    copy_matrix(result, kernel_data.cam.worldtondc);
    return true;
  }
  else if (to == u_raster) {
    copy_matrix(result, kernel_data.cam.worldtoraster);
    return true;
  }
  else if (to == u_screen) {
    copy_matrix(result, kernel_data.cam.worldtoscreen);
    return true;
  }
  else if (to == u_camera) {
    copy_matrix(result, kernel_data.cam.worldtocamera);
    return true;
  }
  else if (to == u_world) {
    result.makeIdentity();
    return true;
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSL::TransformationPtr xform)
{
  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  if (xform) {
    const ShaderData *sd = (const ShaderData *)xform;
    int object = sd->object;

    if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
      Transform tfm = sd->ob_tfm;
#else
      KernelGlobals *kg = sd->osl_globals;
      Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
#endif
      copy_matrix(result, tfm);

      return true;
    }
    else if (sd->type == PRIMITIVE_LAMP) {
      copy_matrix(result, sd->ob_tfm);

      return true;
    }
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSL::TransformationPtr xform)
{
  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  if (xform) {
    const ShaderData *sd = (const ShaderData *)xform;
    int object = sd->object;

    if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
      Transform tfm = sd->ob_itfm;
#else
      KernelGlobals *kg = sd->osl_globals;
      Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
#endif
      copy_matrix(result, tfm);

      return true;
    }
    else if (sd->type == PRIMITIVE_LAMP) {
      copy_matrix(result, sd->ob_itfm);

      return true;
    }
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from)
{
  ShaderData *sd = (ShaderData *)(sg->renderstate);
  KernelGlobals *kg = sd->osl_globals;

  if (from == u_ndc) {
    copy_matrix(result, kernel_data.cam.ndctoworld);
    return true;
  }
  else if (from == u_raster) {
    copy_matrix(result, kernel_data.cam.rastertoworld);
    return true;
  }
  else if (from == u_screen) {
    copy_matrix(result, kernel_data.cam.screentoworld);
    return true;
  }
  else if (from == u_camera) {
    copy_matrix(result, kernel_data.cam.cameratoworld);
    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           ustring to)
{
  ShaderData *sd = (ShaderData *)(sg->renderstate);
  KernelGlobals *kg = sd->osl_globals;

  if (to == u_ndc) {
    copy_matrix(result, kernel_data.cam.worldtondc);
    return true;
  }
  else if (to == u_raster) {
    copy_matrix(result, kernel_data.cam.worldtoraster);
    return true;
  }
  else if (to == u_screen) {
    copy_matrix(result, kernel_data.cam.worldtoscreen);
    return true;
  }
  else if (to == u_camera) {
    copy_matrix(result, kernel_data.cam.worldtocamera);
    return true;
  }

  return false;
}

bool OSLRenderServices::get_array_attribute(OSL::ShaderGlobals *sg,
                                            bool derivatives,
                                            ustring object,
                                            TypeDesc type,
                                            ustring name,
                                            int index,
                                            void *val)
{
  return false;
}

static bool set_attribute_float2(float2 f[3], TypeDesc type, bool derivatives, void *val)
{
  if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
      type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor) {
    float *fval = (float *)val;

    fval[0] = f[0].x;
    fval[1] = f[0].y;
    fval[2] = 0.0f;

    if (derivatives) {
      fval[3] = f[1].x;
      fval[4] = f[1].y;
      fval[5] = 0.0f;

      fval[6] = f[2].x;
      fval[7] = f[2].y;
      fval[8] = 0.0f;
    }

    return true;
  }
  else if (type == TypeDesc::TypeFloat) {
    float *fval = (float *)val;
    fval[0] = average(f[0]);

    if (derivatives) {
      fval[1] = average(f[1]);
      fval[2] = average(f[2]);
    }

    return true;
  }

  return false;
}

static bool set_attribute_float3(float3 f[3], TypeDesc type, bool derivatives, void *val)
{
  if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
      type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor) {
    float *fval = (float *)val;

    fval[0] = f[0].x;
    fval[1] = f[0].y;
    fval[2] = f[0].z;

    if (derivatives) {
      fval[3] = f[1].x;
      fval[4] = f[1].y;
      fval[5] = f[1].z;

      fval[6] = f[2].x;
      fval[7] = f[2].y;
      fval[8] = f[2].z;
    }

    return true;
  }
  else if (type == TypeDesc::TypeFloat) {
    float *fval = (float *)val;
    fval[0] = average(f[0]);

    if (derivatives) {
      fval[1] = average(f[1]);
      fval[2] = average(f[2]);
    }

    return true;
  }

  return false;
}

static bool set_attribute_float3(float3 f, TypeDesc type, bool derivatives, void *val)
{
  float3 fv[3];

  fv[0] = f;
  fv[1] = make_float3(0.0f, 0.0f, 0.0f);
  fv[2] = make_float3(0.0f, 0.0f, 0.0f);

  return set_attribute_float3(fv, type, derivatives, val);
}

static bool set_attribute_float(float f[3], TypeDesc type, bool derivatives, void *val)
{
  if (type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
      type == TypeDesc::TypeNormal || type == TypeDesc::TypeColor) {
    float *fval = (float *)val;
    fval[0] = f[0];
    fval[1] = f[1];
    fval[2] = f[2];

    if (derivatives) {
      fval[3] = f[1];
      fval[4] = f[1];
      fval[5] = f[1];

      fval[6] = f[2];
      fval[7] = f[2];
      fval[8] = f[2];
    }

    return true;
  }
  else if (type == TypeDesc::TypeFloat) {
    float *fval = (float *)val;
    fval[0] = f[0];

    if (derivatives) {
      fval[1] = f[1];
      fval[2] = f[2];
    }

    return true;
  }

  return false;
}

static bool set_attribute_float(float f, TypeDesc type, bool derivatives, void *val)
{
  float fv[3];

  fv[0] = f;
  fv[1] = 0.0f;
  fv[2] = 0.0f;

  return set_attribute_float(fv, type, derivatives, val);
}

static bool set_attribute_int(int i, TypeDesc type, bool derivatives, void *val)
{
  if (type.basetype == TypeDesc::INT && type.aggregate == TypeDesc::SCALAR && type.arraylen == 0) {
    int *ival = (int *)val;
    ival[0] = i;

    if (derivatives) {
      ival[1] = 0;
      ival[2] = 0;
    }

    return true;
  }

  return false;
}

static bool set_attribute_string(ustring str, TypeDesc type, bool derivatives, void *val)
{
  if (type.basetype == TypeDesc::STRING && type.aggregate == TypeDesc::SCALAR &&
      type.arraylen == 0) {
    ustring *sval = (ustring *)val;
    sval[0] = str;

    if (derivatives) {
      sval[1] = OSLRenderServices::u_empty;
      sval[2] = OSLRenderServices::u_empty;
    }

    return true;
  }

  return false;
}

static bool set_attribute_float3_3(float3 P[3], TypeDesc type, bool derivatives, void *val)
{
  if (type.vecsemantics == TypeDesc::POINT && type.arraylen >= 3) {
    float *fval = (float *)val;

    fval[0] = P[0].x;
    fval[1] = P[0].y;
    fval[2] = P[0].z;

    fval[3] = P[1].x;
    fval[4] = P[1].y;
    fval[5] = P[1].z;

    fval[6] = P[2].x;
    fval[7] = P[2].y;
    fval[8] = P[2].z;

    if (type.arraylen > 3)
      memset(fval + 3 * 3, 0, sizeof(float) * 3 * (type.arraylen - 3));
    if (derivatives)
      memset(fval + type.arraylen * 3, 0, sizeof(float) * 2 * 3 * type.arraylen);

    return true;
  }

  return false;
}

static bool set_attribute_matrix(const Transform &tfm, TypeDesc type, void *val)
{
  if (type == TypeDesc::TypeMatrix) {
    copy_matrix(*(OSL::Matrix44 *)val, tfm);
    return true;
  }

  return false;
}

static bool get_primitive_attribute(KernelGlobals *kg,
                                    const ShaderData *sd,
                                    const OSLGlobals::Attribute &attr,
                                    const TypeDesc &type,
                                    bool derivatives,
                                    void *val)
{
  if (attr.type == TypeDesc::TypePoint || attr.type == TypeDesc::TypeVector ||
      attr.type == TypeDesc::TypeNormal || attr.type == TypeDesc::TypeColor) {
    float3 fval[3];
    fval[0] = primitive_attribute_float3(
        kg, sd, attr.desc, (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
    return set_attribute_float3(fval, type, derivatives, val);
  }
  else if (attr.type == TypeFloat2) {
    float2 fval[3];
    fval[0] = primitive_attribute_float2(
        kg, sd, attr.desc, (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
    return set_attribute_float2(fval, type, derivatives, val);
  }
  else if (attr.type == TypeDesc::TypeFloat) {
    float fval[3];
    fval[0] = primitive_attribute_float(
        kg, sd, attr.desc, (derivatives) ? &fval[1] : NULL, (derivatives) ? &fval[2] : NULL);
    return set_attribute_float(fval, type, derivatives, val);
  }
  else {
    return false;
  }
}

static bool get_mesh_attribute(KernelGlobals *kg,
                               const ShaderData *sd,
                               const OSLGlobals::Attribute &attr,
                               const TypeDesc &type,
                               bool derivatives,
                               void *val)
{
  if (attr.type == TypeDesc::TypeMatrix) {
    Transform tfm = primitive_attribute_matrix(kg, sd, attr.desc);
    return set_attribute_matrix(tfm, type, val);
  }
  else {
    return false;
  }
}

static void get_object_attribute(const OSLGlobals::Attribute &attr, bool derivatives, void *val)
{
  size_t datasize = attr.value.datasize();

  memcpy(val, attr.value.data(), datasize);
  if (derivatives)
    memset((char *)val + datasize, 0, datasize * 2);
}

bool OSLRenderServices::get_object_standard_attribute(
    KernelGlobals *kg, ShaderData *sd, ustring name, TypeDesc type, bool derivatives, void *val)
{
  /* todo: turn this into hash table? */

  /* Object Attributes */
  if (name == u_object_location) {
    float3 f = object_location(kg, sd);
    return set_attribute_float3(f, type, derivatives, val);
  }
  else if (name == u_object_index) {
    float f = object_pass_id(kg, sd->object);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_geom_dupli_generated) {
    float3 f = object_dupli_generated(kg, sd->object);
    return set_attribute_float3(f, type, derivatives, val);
  }
  else if (name == u_geom_dupli_uv) {
    float3 f = object_dupli_uv(kg, sd->object);
    return set_attribute_float3(f, type, derivatives, val);
  }
  else if (name == u_material_index) {
    float f = shader_pass_id(kg, sd);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_object_random) {
    float f = object_random_number(kg, sd->object);
    return set_attribute_float(f, type, derivatives, val);
  }

  /* Particle Attributes */
  else if (name == u_particle_index) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_index(kg, particle_id);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_particle_random) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = hash_int_01(particle_index(kg, particle_id));
    return set_attribute_float(f, type, derivatives, val);
  }

  else if (name == u_particle_age) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_age(kg, particle_id);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_particle_lifetime) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_lifetime(kg, particle_id);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_particle_location) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_location(kg, particle_id);
    return set_attribute_float3(f, type, derivatives, val);
  }
#if 0 /* unsupported */
  else if (name == u_particle_rotation) {
    int particle_id = object_particle_id(kg, sd->object);
    float4 f = particle_rotation(kg, particle_id);
    return set_attribute_float4(f, type, derivatives, val);
  }
#endif
  else if (name == u_particle_size) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_size(kg, particle_id);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_particle_velocity) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_velocity(kg, particle_id);
    return set_attribute_float3(f, type, derivatives, val);
  }
  else if (name == u_particle_angular_velocity) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_angular_velocity(kg, particle_id);
    return set_attribute_float3(f, type, derivatives, val);
  }

  /* Geometry Attributes */
  else if (name == u_geom_numpolyvertices) {
    return set_attribute_int(3, type, derivatives, val);
  }
  else if ((name == u_geom_trianglevertices || name == u_geom_polyvertices) &&
           sd->type & PRIMITIVE_ALL_TRIANGLE) {
    float3 P[3];

    if (sd->type & PRIMITIVE_TRIANGLE)
      triangle_vertices(kg, sd->prim, P);
    else
      motion_triangle_vertices(kg, sd->object, sd->prim, sd->time, P);

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_position_transform(kg, sd, &P[0]);
      object_position_transform(kg, sd, &P[1]);
      object_position_transform(kg, sd, &P[2]);
    }

    return set_attribute_float3_3(P, type, derivatives, val);
  }
  else if (name == u_geom_name) {
    ustring object_name = kg->osl->object_names[sd->object];
    return set_attribute_string(object_name, type, derivatives, val);
  }
  else if (name == u_is_smooth) {
    float f = ((sd->shader & SHADER_SMOOTH_NORMAL) != 0);
    return set_attribute_float(f, type, derivatives, val);
  }
  /* Hair Attributes */
  else if (name == u_is_curve) {
    float f = (sd->type & PRIMITIVE_ALL_CURVE) != 0;
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_curve_thickness) {
    float f = curve_thickness(kg, sd);
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_curve_tangent_normal) {
    float3 f = curve_tangent_normal(kg, sd);
    return set_attribute_float3(f, type, derivatives, val);
  }
  else
    return false;
}

bool OSLRenderServices::get_background_attribute(
    KernelGlobals *kg, ShaderData *sd, ustring name, TypeDesc type, bool derivatives, void *val)
{
  if (name == u_path_ray_length) {
    /* Ray Length */
    float f = sd->ray_length;
    return set_attribute_float(f, type, derivatives, val);
  }
  else if (name == u_path_ray_depth) {
    /* Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_path_diffuse_depth) {
    /* Diffuse Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->diffuse_bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_path_glossy_depth) {
    /* Glossy Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->glossy_bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_path_transmission_depth) {
    /* Transmission Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->transmission_bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_path_transparent_depth) {
    /* Transparent Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->transparent_bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_path_transmission_depth) {
    /* Transmission Ray Depth */
    PathState *state = sd->osl_path_state;
    int f = state->transmission_bounce;
    return set_attribute_int(f, type, derivatives, val);
  }
  else if (name == u_ndc) {
    /* NDC coordinates with special exception for otho */
    OSLThreadData *tdata = kg->osl_tdata;
    OSL::ShaderGlobals *globals = &tdata->globals;
    float3 ndc[3];

    if ((globals->raytype & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
        kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
      ndc[0] = camera_world_to_ndc(kg, sd, sd->ray_P);

      if (derivatives) {
        ndc[1] = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dx) - ndc[0];
        ndc[2] = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dy) - ndc[0];
      }
    }
    else {
      ndc[0] = camera_world_to_ndc(kg, sd, sd->P);

      if (derivatives) {
        ndc[1] = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dx) - ndc[0];
        ndc[2] = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dy) - ndc[0];
      }
    }

    return set_attribute_float3(ndc, type, derivatives, val);
  }
  else
    return false;
}

bool OSLRenderServices::get_attribute(OSL::ShaderGlobals *sg,
                                      bool derivatives,
                                      ustring object_name,
                                      TypeDesc type,
                                      ustring name,
                                      void *val)
{
  if (sg == NULL || sg->renderstate == NULL)
    return false;

  ShaderData *sd = (ShaderData *)(sg->renderstate);
  return get_attribute(sd, derivatives, object_name, type, name, val);
}

bool OSLRenderServices::get_attribute(
    ShaderData *sd, bool derivatives, ustring object_name, TypeDesc type, ustring name, void *val)
{
  KernelGlobals *kg = sd->osl_globals;
  int prim_type = 0;
  int object;

  /* lookup of attribute on another object */
  if (object_name != u_empty) {
    OSLGlobals::ObjectNameMap::iterator it = kg->osl->object_name_map.find(object_name);

    if (it == kg->osl->object_name_map.end())
      return false;

    object = it->second;
  }
  else {
    object = sd->object;
    prim_type = attribute_primitive_type(kg, sd);

    if (object == OBJECT_NONE)
      return get_background_attribute(kg, sd, name, type, derivatives, val);
  }

  /* find attribute on object */
  object = object * ATTR_PRIM_TYPES + prim_type;
  OSLGlobals::AttributeMap &attribute_map = kg->osl->attribute_map[object];
  OSLGlobals::AttributeMap::iterator it = attribute_map.find(name);

  if (it != attribute_map.end()) {
    const OSLGlobals::Attribute &attr = it->second;

    if (attr.desc.element != ATTR_ELEMENT_OBJECT) {
      /* triangle and vertex attributes */
      if (get_primitive_attribute(kg, sd, attr, type, derivatives, val))
        return true;
      else
        return get_mesh_attribute(kg, sd, attr, type, derivatives, val);
    }
    else {
      /* object attribute */
      get_object_attribute(attr, derivatives, val);
      return true;
    }
  }
  else {
    /* not found in attribute, check standard object info */
    bool is_std_object_attribute = get_object_standard_attribute(
        kg, sd, name, type, derivatives, val);

    if (is_std_object_attribute)
      return true;

    return get_background_attribute(kg, sd, name, type, derivatives, val);
  }

  return false;
}

bool OSLRenderServices::get_userdata(
    bool derivatives, ustring name, TypeDesc type, OSL::ShaderGlobals *sg, void *val)
{
  return false; /* disabled by lockgeom */
}

TextureSystem::TextureHandle *OSLRenderServices::get_texture_handle(ustring filename)
{
  OSLTextureHandleMap::iterator it = textures.find(filename);

  /* For non-OIIO textures, just return a pointer to our own OSLTextureHandle. */
  if (it != textures.end()) {
    if (it->second->type != OSLTextureHandle::OIIO) {
      return (TextureSystem::TextureHandle *)it->second.get();
    }
  }

  /* Get handle from OpenImageIO. */
  OSL::TextureSystem *ts = texture_system;
  TextureSystem::TextureHandle *handle = ts->get_texture_handle(filename);
  if (handle == NULL) {
    return NULL;
  }

  /* Insert new OSLTextureHandle if needed. */
  if (it == textures.end()) {
    textures.insert(filename, new OSLTextureHandle(OSLTextureHandle::OIIO));
    it = textures.find(filename);
  }

  /* Assign OIIO texture handle and return. */
  it->second->oiio_handle = handle;
  return (TextureSystem::TextureHandle *)it->second.get();
}

bool OSLRenderServices::good(TextureSystem::TextureHandle *texture_handle)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;

  if (handle->oiio_handle) {
    OSL::TextureSystem *ts = texture_system;
    return ts->good(handle->oiio_handle);
  }
  else {
    return true;
  }
}

bool OSLRenderServices::texture(ustring filename,
                                TextureHandle *texture_handle,
                                TexturePerthread *texture_thread_info,
                                TextureOpt &options,
                                OSL::ShaderGlobals *sg,
                                float s,
                                float t,
                                float dsdx,
                                float dtdx,
                                float dsdy,
                                float dtdy,
                                int nchannels,
                                float *result,
                                float *dresultds,
                                float *dresultdt,
                                ustring *errormessage)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  OSLTextureHandle::Type texture_type = (handle) ? handle->type : OSLTextureHandle::OIIO;
  ShaderData *sd = (ShaderData *)(sg->renderstate);
  KernelGlobals *kernel_globals = sd->osl_globals;
  bool status = false;

  switch (texture_type) {
    case OSLTextureHandle::BEVEL: {
      /* Bevel shader hack. */
      if (nchannels >= 3) {
        PathState *state = sd->osl_path_state;
        int num_samples = (int)s;
        float radius = t;
        float3 N = svm_bevel(kernel_globals, sd, state, radius, num_samples);
        result[0] = N.x;
        result[1] = N.y;
        result[2] = N.z;
        status = true;
      }
      break;
    }
    case OSLTextureHandle::AO: {
      /* AO shader hack. */
      PathState *state = sd->osl_path_state;
      int num_samples = (int)s;
      float radius = t;
      float3 N = make_float3(dsdx, dtdx, dsdy);
      int flags = 0;
      if ((int)dtdy) {
        flags |= NODE_AO_INSIDE;
      }
      if ((int)options.sblur) {
        flags |= NODE_AO_ONLY_LOCAL;
      }
      if ((int)options.tblur) {
        flags |= NODE_AO_GLOBAL_RADIUS;
      }
      result[0] = svm_ao(kernel_globals, sd, N, state, radius, num_samples, flags);
      status = true;
      break;
    }
    case OSLTextureHandle::SVM: {
      /* Packed texture. */
      float4 rgba = kernel_tex_image_interp(kernel_globals, handle->svm_slot, s, 1.0f - t);

      result[0] = rgba[0];
      if (nchannels > 1)
        result[1] = rgba[1];
      if (nchannels > 2)
        result[2] = rgba[2];
      if (nchannels > 3)
        result[3] = rgba[3];
      status = true;
      break;
    }
    case OSLTextureHandle::IES: {
      /* IES light. */
      result[0] = kernel_ies_interp(kernel_globals, handle->svm_slot, s, t);
      status = true;
      break;
    }
    case OSLTextureHandle::OIIO: {
      /* OpenImageIO texture cache. */
      OSL::TextureSystem *ts = texture_system;

      if (handle && handle->oiio_handle) {
        if (texture_thread_info == NULL) {
          OSLThreadData *tdata = kernel_globals->osl_tdata;
          texture_thread_info = tdata->oiio_thread_info;
        }

        status = ts->texture(handle->oiio_handle,
                             texture_thread_info,
                             options,
                             s,
                             t,
                             dsdx,
                             dtdx,
                             dsdy,
                             dtdy,
                             nchannels,
                             result,
                             dresultds,
                             dresultdt);
      }
      else {
        status = ts->texture(filename,
                             options,
                             s,
                             t,
                             dsdx,
                             dtdx,
                             dsdy,
                             dtdy,
                             nchannels,
                             result,
                             dresultds,
                             dresultdt);
      }

      if (!status) {
        /* This might be slow, but prevents error messages leak and
         * other nasty stuff happening. */
        ts->geterror();
      }
      else if (handle && handle->processor) {
        ColorSpaceManager::to_scene_linear(handle->processor, result, nchannels);
      }
      break;
    }
  }

  if (!status) {
    if (nchannels == 3 || nchannels == 4) {
      result[0] = 1.0f;
      result[1] = 0.0f;
      result[2] = 1.0f;

      if (nchannels == 4)
        result[3] = 1.0f;
    }
  }

  return status;
}

bool OSLRenderServices::texture3d(ustring filename,
                                  TextureHandle *texture_handle,
                                  TexturePerthread *texture_thread_info,
                                  TextureOpt &options,
                                  OSL::ShaderGlobals *sg,
                                  const OSL::Vec3 &P,
                                  const OSL::Vec3 &dPdx,
                                  const OSL::Vec3 &dPdy,
                                  const OSL::Vec3 &dPdz,
                                  int nchannels,
                                  float *result,
                                  float *dresultds,
                                  float *dresultdt,
                                  float *dresultdr,
                                  ustring *errormessage)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  OSLTextureHandle::Type texture_type = (handle) ? handle->type : OSLTextureHandle::OIIO;
  bool status = false;

  switch (texture_type) {
    case OSLTextureHandle::SVM: {
      /* Packed texture. */
      ShaderData *sd = (ShaderData *)(sg->renderstate);
      KernelGlobals *kernel_globals = sd->osl_globals;
      int slot = handle->svm_slot;
      float4 rgba = kernel_tex_image_interp_3d(
          kernel_globals, slot, P.x, P.y, P.z, INTERPOLATION_NONE);

      result[0] = rgba[0];
      if (nchannels > 1)
        result[1] = rgba[1];
      if (nchannels > 2)
        result[2] = rgba[2];
      if (nchannels > 3)
        result[3] = rgba[3];
      status = true;
      break;
    }
    case OSLTextureHandle::OIIO: {
      /* OpenImageIO texture cache. */
      OSL::TextureSystem *ts = texture_system;

      if (handle && handle->oiio_handle) {
        if (texture_thread_info == NULL) {
          ShaderData *sd = (ShaderData *)(sg->renderstate);
          KernelGlobals *kernel_globals = sd->osl_globals;
          OSLThreadData *tdata = kernel_globals->osl_tdata;
          texture_thread_info = tdata->oiio_thread_info;
        }

        status = ts->texture3d(handle->oiio_handle,
                               texture_thread_info,
                               options,
                               P,
                               dPdx,
                               dPdy,
                               dPdz,
                               nchannels,
                               result,
                               dresultds,
                               dresultdt,
                               dresultdr);
      }
      else {
        status = ts->texture3d(filename,
                               options,
                               P,
                               dPdx,
                               dPdy,
                               dPdz,
                               nchannels,
                               result,
                               dresultds,
                               dresultdt,
                               dresultdr);
      }

      if (!status) {
        /* This might be slow, but prevents error messages leak and
         * other nasty stuff happening. */
        ts->geterror();
      }
      else if (handle && handle->processor) {
        ColorSpaceManager::to_scene_linear(handle->processor, result, nchannels);
      }
      break;
    }
    case OSLTextureHandle::IES:
    case OSLTextureHandle::AO:
    case OSLTextureHandle::BEVEL: {
      status = false;
      break;
    }
  }

  if (!status) {
    if (nchannels == 3 || nchannels == 4) {
      result[0] = 1.0f;
      result[1] = 0.0f;
      result[2] = 1.0f;

      if (nchannels == 4)
        result[3] = 1.0f;
    }
  }

  return status;
}

bool OSLRenderServices::environment(ustring filename,
                                    TextureHandle *texture_handle,
                                    TexturePerthread *thread_info,
                                    TextureOpt &options,
                                    OSL::ShaderGlobals *sg,
                                    const OSL::Vec3 &R,
                                    const OSL::Vec3 &dRdx,
                                    const OSL::Vec3 &dRdy,
                                    int nchannels,
                                    float *result,
                                    float *dresultds,
                                    float *dresultdt,
                                    ustring *errormessage)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  OSL::TextureSystem *ts = texture_system;
  bool status = false;

  if (handle && handle->oiio_handle) {
    if (thread_info == NULL) {
      ShaderData *sd = (ShaderData *)(sg->renderstate);
      KernelGlobals *kernel_globals = sd->osl_globals;
      OSLThreadData *tdata = kernel_globals->osl_tdata;
      thread_info = tdata->oiio_thread_info;
    }

    status = ts->environment(handle->oiio_handle,
                             thread_info,
                             options,
                             R,
                             dRdx,
                             dRdy,
                             nchannels,
                             result,
                             dresultds,
                             dresultdt);
  }
  else {
    status = ts->environment(
        filename, options, R, dRdx, dRdy, nchannels, result, dresultds, dresultdt);
  }

  if (!status) {
    if (nchannels == 3 || nchannels == 4) {
      result[0] = 1.0f;
      result[1] = 0.0f;
      result[2] = 1.0f;

      if (nchannels == 4)
        result[3] = 1.0f;
    }
  }
  else if (handle && handle->processor) {
    ColorSpaceManager::to_scene_linear(handle->processor, result, nchannels);
  }

  return status;
}

bool OSLRenderServices::get_texture_info(OSL::ShaderGlobals *sg,
                                         ustring filename,
                                         TextureHandle *texture_handle,
                                         int subimage,
                                         ustring dataname,
                                         TypeDesc datatype,
                                         void *data)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;

  /* No texture info for other texture types. */
  if (handle && handle->type != OSLTextureHandle::OIIO) {
    return false;
  }

  /* Get texture info from OpenImageIO. */
  OSL::TextureSystem *ts = texture_system;
  return ts->get_texture_info(filename, subimage, dataname, datatype, data);
}

int OSLRenderServices::pointcloud_search(OSL::ShaderGlobals *sg,
                                         ustring filename,
                                         const OSL::Vec3 &center,
                                         float radius,
                                         int max_points,
                                         bool sort,
                                         size_t *out_indices,
                                         float *out_distances,
                                         int derivs_offset)
{
  return 0;
}

int OSLRenderServices::pointcloud_get(OSL::ShaderGlobals *sg,
                                      ustring filename,
                                      size_t *indices,
                                      int count,
                                      ustring attr_name,
                                      TypeDesc attr_type,
                                      void *out_data)
{
  return 0;
}

bool OSLRenderServices::pointcloud_write(OSL::ShaderGlobals *sg,
                                         ustring filename,
                                         const OSL::Vec3 &pos,
                                         int nattribs,
                                         const ustring *names,
                                         const TypeDesc *types,
                                         const void **data)
{
  return false;
}

bool OSLRenderServices::trace(TraceOpt &options,
                              OSL::ShaderGlobals *sg,
                              const OSL::Vec3 &P,
                              const OSL::Vec3 &dPdx,
                              const OSL::Vec3 &dPdy,
                              const OSL::Vec3 &R,
                              const OSL::Vec3 &dRdx,
                              const OSL::Vec3 &dRdy)
{
  /* todo: options.shader support, maybe options.traceset */
  ShaderData *sd = (ShaderData *)(sg->renderstate);

  /* setup ray */
  Ray ray;

  ray.P = TO_FLOAT3(P);
  ray.D = TO_FLOAT3(R);
  ray.t = (options.maxdist == 1.0e30f) ? FLT_MAX : options.maxdist - options.mindist;
  ray.time = sd->time;

  if (options.mindist == 0.0f) {
    /* avoid self-intersections */
    if (ray.P == sd->P) {
      bool transmit = (dot(sd->Ng, ray.D) < 0.0f);
      ray.P = ray_offset(sd->P, (transmit) ? -sd->Ng : sd->Ng);
    }
  }
  else {
    /* offset for minimum distance */
    ray.P += options.mindist * ray.D;
  }

  /* ray differentials */
  ray.dP.dx = TO_FLOAT3(dPdx);
  ray.dP.dy = TO_FLOAT3(dPdy);
  ray.dD.dx = TO_FLOAT3(dRdx);
  ray.dD.dy = TO_FLOAT3(dRdy);

  /* allocate trace data */
  OSLTraceData *tracedata = (OSLTraceData *)sg->tracedata;
  tracedata->ray = ray;
  tracedata->setup = false;
  tracedata->init = true;
  tracedata->sd.osl_globals = sd->osl_globals;

  KernelGlobals *kg = sd->osl_globals;

  /* Can't raytrace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return false;
  }

  /* Raytrace, leaving out shadow opaque to avoid early exit. */
  uint visibility = PATH_RAY_ALL_VISIBILITY - PATH_RAY_SHADOW_OPAQUE;
  return scene_intersect(kg, ray, visibility, &tracedata->isect);
}

bool OSLRenderServices::getmessage(OSL::ShaderGlobals *sg,
                                   ustring source,
                                   ustring name,
                                   TypeDesc type,
                                   void *val,
                                   bool derivatives)
{
  OSLTraceData *tracedata = (OSLTraceData *)sg->tracedata;

  if (source == u_trace && tracedata->init) {
    if (name == u_hit) {
      return set_attribute_int((tracedata->isect.prim != PRIM_NONE), type, derivatives, val);
    }
    else if (tracedata->isect.prim != PRIM_NONE) {
      if (name == u_hitdist) {
        float f[3] = {tracedata->isect.t, 0.0f, 0.0f};
        return set_attribute_float(f, type, derivatives, val);
      }
      else {
        ShaderData *sd = &tracedata->sd;
        KernelGlobals *kg = sd->osl_globals;

        if (!tracedata->setup) {
          /* lazy shader data setup */
          shader_setup_from_ray(kg, sd, &tracedata->isect, &tracedata->ray);
          tracedata->setup = true;
        }

        if (name == u_N) {
          return set_attribute_float3(sd->N, type, derivatives, val);
        }
        else if (name == u_Ng) {
          return set_attribute_float3(sd->Ng, type, derivatives, val);
        }
        else if (name == u_P) {
          float3 f[3] = {sd->P, sd->dP.dx, sd->dP.dy};
          return set_attribute_float3(f, type, derivatives, val);
        }
        else if (name == u_I) {
          float3 f[3] = {sd->I, sd->dI.dx, sd->dI.dy};
          return set_attribute_float3(f, type, derivatives, val);
        }
        else if (name == u_u) {
          float f[3] = {sd->u, sd->du.dx, sd->du.dy};
          return set_attribute_float(f, type, derivatives, val);
        }
        else if (name == u_v) {
          float f[3] = {sd->v, sd->dv.dx, sd->dv.dy};
          return set_attribute_float(f, type, derivatives, val);
        }

        return get_attribute(sd, derivatives, u_empty, type, name, val);
      }
    }
  }

  return false;
}

CCL_NAMESPACE_END
