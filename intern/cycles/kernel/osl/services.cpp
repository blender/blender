/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* TODO(sergey): There is a bit of headers dependency hell going on
 * here, so for now we just put here. In the future it might be better
 * to have dedicated file for such tweaks.
 */
#if (defined(__GNUC__) && !defined(__clang__)) && defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include <cstring>

#include "scene/colorspace.h"
#include "scene/object.h"

#include "util/log.h"
#include "util/string.h"

#include "kernel/device/cpu/image.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"
#include "kernel/osl/services_shared.h"
#include "kernel/osl/types.h"

#include "kernel/integrator/state.h"

#include "kernel/geom/primitive.h"
#include "kernel/geom/shader_data.h"

#include "kernel/bvh/bvh.h"

#include "kernel/camera/camera.h"

#include "kernel/svm/ao.h"
#include "kernel/svm/bevel.h"

#include "kernel/util/ies.h"
#include "kernel/util/texture_3d.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

static void copy_matrix(OSL::Matrix44 &m, const Transform &tfm)
{
  ProjectionTransform t = projection_transpose(ProjectionTransform(tfm));
  memcpy((float *)&m, (const float *)&t, sizeof(m));
}

static void copy_matrix(OSL::Matrix44 &m, const ProjectionTransform &tfm)
{
  ProjectionTransform t = projection_transpose(tfm);
  memcpy((float *)&m, (const float *)&t, sizeof(m));
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
ustring OSLRenderServices::u_object_color("object:color");
ustring OSLRenderServices::u_object_alpha("object:alpha");
ustring OSLRenderServices::u_object_index("object:index");
ustring OSLRenderServices::u_object_is_light("object:is_light");
ustring OSLRenderServices::u_bump_map_normal("geom:bump_map_normal");
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
ustring OSLRenderServices::u_curve_length("geom:curve_length");
ustring OSLRenderServices::u_curve_tangent_normal("geom:curve_tangent_normal");
ustring OSLRenderServices::u_curve_random("geom:curve_random");
ustring OSLRenderServices::u_is_point("geom:is_point");
ustring OSLRenderServices::u_point_radius("geom:point_radius");
ustring OSLRenderServices::u_point_position("geom:point_position");
ustring OSLRenderServices::u_point_random("geom:point_random");
ustring OSLRenderServices::u_normal_map_normal("geom:normal_map_normal");
ustring OSLRenderServices::u_path_ray_length("path:ray_length");
ustring OSLRenderServices::u_path_ray_depth("path:ray_depth");
ustring OSLRenderServices::u_path_diffuse_depth("path:diffuse_depth");
ustring OSLRenderServices::u_path_glossy_depth("path:glossy_depth");
ustring OSLRenderServices::u_path_transparent_depth("path:transparent_depth");
ustring OSLRenderServices::u_path_transmission_depth("path:transmission_depth");
ustring OSLRenderServices::u_path_portal_depth("path:portal_depth");
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

ustring OSLRenderServices::u_sensor_size("cam:sensor_size");
ustring OSLRenderServices::u_image_resolution("cam:image_resolution");
ustring OSLRenderServices::u_aperture_aspect_ratio("cam:aperture_aspect_ratio");
ustring OSLRenderServices::u_aperture_size("cam:aperture_size");
ustring OSLRenderServices::u_aperture_position("cam:aperture_position");
ustring OSLRenderServices::u_focal_distance("cam:focal_distance");

ImageManager *OSLRenderServices::image_manager = nullptr;

OSLRenderServices::OSLRenderServices(OSL::TextureSystem *texture_system, const int device_type)
    : OSL::RendererServices(texture_system), device_type_(device_type)
{
}

OSLRenderServices::~OSLRenderServices()
{
  if (m_texturesys) {
    LOG_INFO << "OSL texture system stats:\n" << m_texturesys->getstats();
  }
}

int OSLRenderServices::supports(string_view feature) const
{
#ifdef WITH_OPTIX
  if (feature == "OptiX") {
    return device_type_ == DEVICE_OPTIX;
  }
#else
  (void)feature;
#endif

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSL::TransformationPtr /*xform*/,
                                   const float time)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }

  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  const ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  const int object = sd->object;

  if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
    Transform tfm;

    if (time == sd->time) {
      tfm = object_get_transform(kg, sd);
    }
    else {
      tfm = object_fetch_transform_motion_test(kg, object, time, nullptr);
    }
#else
    const Transform tfm = object_get_transform(kg, sd);
#endif
    copy_matrix(result, tfm);

    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSL::TransformationPtr /*xform*/,
                                           const float time)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }

  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  const ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  const int object = sd->object;

  if (object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
    Transform itfm;

    if (time == sd->time) {
      itfm = object_get_inverse_transform(kg, sd);
    }
    else {
      object_fetch_transform_motion_test(kg, object, time, &itfm);
    }
#else
    const Transform itfm = object_get_inverse_transform(kg, sd);
#endif
    copy_matrix(result, itfm);

    return true;
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSLUStringHash from,
                                   const float /*time*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  if (from == u_ndc) {
    copy_matrix(result, kernel_data.cam.ndctoworld);
    return true;
  }
  if (from == u_raster) {
    copy_matrix(result, kernel_data.cam.rastertoworld);
    return true;
  }
  if (from == u_screen) {
    copy_matrix(result, kernel_data.cam.screentoworld);
    return true;
  }
  if (from == u_camera) {
    copy_matrix(result, kernel_data.cam.cameratoworld);
    return true;
  }
  if (from == u_world) {
    result.makeIdentity();
    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSLUStringHash to,
                                           const float /*time*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  if (to == u_ndc) {
    copy_matrix(result, kernel_data.cam.worldtondc);
    return true;
  }
  if (to == u_raster) {
    copy_matrix(result, kernel_data.cam.worldtoraster);
    return true;
  }
  if (to == u_screen) {
    copy_matrix(result, kernel_data.cam.worldtoscreen);
    return true;
  }
  if (to == u_camera) {
    copy_matrix(result, kernel_data.cam.worldtocamera);
    return true;
  }
  if (to == u_world) {
    result.makeIdentity();
    return true;
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSL::TransformationPtr /*xform*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }

  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  const ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  const int object = sd->object;

  if (object != OBJECT_NONE) {
    const Transform tfm = object_get_transform(kg, sd);
    copy_matrix(result, tfm);

    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSL::TransformationPtr /*xform*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }

  /* this is only used for shader and object space, we don't really have
   * a concept of shader space, so we just use object space for both. */
  const ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  const int object = sd->object;

  if (object != OBJECT_NONE) {
    const Transform tfm = object_get_inverse_transform(kg, sd);
    copy_matrix(result, tfm);

    return true;
  }

  return false;
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSLUStringHash from)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  if (from == u_ndc) {
    copy_matrix(result, kernel_data.cam.ndctoworld);
    return true;
  }
  if (from == u_raster) {
    copy_matrix(result, kernel_data.cam.rastertoworld);
    return true;
  }
  if (from == u_screen) {
    copy_matrix(result, kernel_data.cam.screentoworld);
    return true;
  }
  if (from == u_camera) {
    copy_matrix(result, kernel_data.cam.cameratoworld);
    return true;
  }

  return false;
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSLUStringHash to)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  if (to == u_ndc) {
    copy_matrix(result, kernel_data.cam.worldtondc);
    return true;
  }
  if (to == u_raster) {
    copy_matrix(result, kernel_data.cam.worldtoraster);
    return true;
  }
  if (to == u_screen) {
    copy_matrix(result, kernel_data.cam.worldtoscreen);
    return true;
  }
  if (to == u_camera) {
    copy_matrix(result, kernel_data.cam.worldtocamera);
    return true;
  }

  return false;
}

bool OSLRenderServices::get_array_attribute(OSL::ShaderGlobals * /*sg*/,
                                            bool /* derivatives*/,
                                            OSLUStringHash /* object*/,
                                            const TypeDesc /* type*/,
                                            OSLUStringHash /* name*/,
                                            const int /* index*/,
                                            void * /*val*/)
{
  return false;
}

ccl_device_template_spec bool set_attribute(const dual1 v,
                                            TypeDesc type,
                                            bool derivatives,
                                            void *val)
{
  if (type == TypeFloatArray4) {
    set_data_float4(make_float4(make_float3(v)), derivatives, val);
    return true;
  }
  if (type == TypePoint || type == TypeVector || type == TypeNormal || type == TypeColor) {
    set_data_float3(make_float3(v), derivatives, val);
    return true;
  }
  if (type == TypeFloat) {
    set_data_float(v, derivatives, val);
    return true;
  }

  return false;
}

ccl_device_template_spec bool set_attribute(const dual2 v,
                                            TypeDesc type,
                                            bool derivatives,
                                            void *val)
{
  if (type == TypeFloatArray4) {
    set_data_float4(make_float4(make_float3(v)), derivatives, val);
    return true;
  }
  if (type == TypePoint || type == TypeVector || type == TypeNormal || type == TypeColor) {
    set_data_float3(make_float3(v), derivatives, val);
    return true;
  }
  if (type == TypeFloat) {
    set_data_float(average(v), derivatives, val);
    return true;
  }

  return false;
}

ccl_device_template_spec bool set_attribute(const dual3 v,
                                            TypeDesc type,
                                            bool derivatives,
                                            void *val)
{
  if (type == TypeFloatArray4) {
    set_data_float4(make_float4(v), derivatives, val);
    return true;
  }
  if (type == TypePoint || type == TypeVector || type == TypeNormal || type == TypeColor) {
    set_data_float3(v, derivatives, val);
    return true;
  }
  if (type == TypeFloat) {
    set_data_float(average(v), derivatives, val);
    return true;
  }

  return false;
}

/* Attributes with the TypeRGBA type descriptor should be retrieved and stored
 * in a float array of size 4 (e.g. node_vertex_color.osl), this array have
 * a type descriptor TypeFloatArray4. If the storage is not a TypeFloatArray4,
 * we either store the first three components in a vector, store the average of
 * the components in a float, or fail the retrieval and do nothing. We allow
 * this for the correct operation of the Attribute node.
 */

ccl_device_template_spec bool set_attribute(const dual4 v,
                                            TypeDesc type,
                                            bool derivatives,
                                            void *val)
{
  if (type == TypeFloatArray4) {
    set_data_float4(v, derivatives, val);
    return true;
  }
  if (type == TypePoint || type == TypeVector || type == TypeNormal || type == TypeColor) {
    set_data_float3(make_float3(v), derivatives, val);
    return true;
  }
  if (type == TypeFloat) {
    set_data_float(average(make_float3(v)), derivatives, val);
    return true;
  }
  return false;
}

template<typename T>
ccl_device_inline bool set_attribute(const T f, const TypeDesc type, bool derivatives, void *val)
{
  return set_attribute(dual<T>(f), type, derivatives, val);
}

ccl_device_template_spec bool set_attribute(const int i,
                                            const TypeDesc type,
                                            bool derivatives,
                                            void *val)
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

ccl_device_template_spec bool set_attribute(ustring str,
                                            const TypeDesc type,
                                            bool derivatives,
                                            void *val)
{
  if (type.basetype == TypeDesc::STRING && type.aggregate == TypeDesc::SCALAR &&
      type.arraylen == 0)
  {
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

static bool set_attribute_float3_3(const float3 P[3], TypeDesc type, bool derivatives, void *val)
{
  if (type.vecsemantics == TypeDesc::POINT && type.arraylen >= 3) {
    float *fval = (float *)val;

    copy_v3_v3(fval, P[0]);
    copy_v3_v3(fval + 3, P[1]);
    copy_v3_v3(fval + 6, P[2]);

    if (type.arraylen > 3) {
      memset(fval + 3 * 3, 0, sizeof(float) * 3 * (type.arraylen - 3));
    }
    if (derivatives) {
      memset(fval + type.arraylen * 3, 0, sizeof(float) * 2 * 3 * type.arraylen);
    }

    return true;
  }

  return false;
}

static bool set_attribute_matrix(const Transform &tfm, const TypeDesc type, void *val)
{
  if (type == TypeMatrix) {
    copy_matrix(*(OSL::Matrix44 *)val, tfm);
    return true;
  }

  return false;
}

template<typename T>
inline bool get_object_attribute_impl(const ThreadKernelGlobalsCPU *kg,
                                      ShaderData *sd,
                                      const AttributeDescriptor &desc,
                                      const TypeDesc &type,
                                      bool derivatives,
                                      void *val)
{
  dual<T> data;
#ifdef __VOLUME__
  if (primitive_is_volume_attribute(sd)) {
    data.val = primitive_volume_attribute<T>(kg, sd, desc, true);
  }
  else
#endif
  {
    data = primitive_surface_attribute<T>(kg, sd, desc, derivatives, derivatives);
  }
  return set_attribute(data, type, derivatives, val);
}

static bool get_object_attribute(const ThreadKernelGlobalsCPU *kg,
                                 ShaderData *sd,
                                 const AttributeDescriptor &desc,
                                 const TypeDesc &type,
                                 bool derivatives,
                                 void *val)
{
  if (desc.type == NODE_ATTR_FLOAT) {
    return get_object_attribute_impl<float>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT2) {
    return get_object_attribute_impl<float2>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT3) {
    return get_object_attribute_impl<float3>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    return get_object_attribute_impl<float4>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_MATRIX) {
    const Transform tfm = primitive_attribute_matrix(kg, desc);
    return set_attribute_matrix(tfm, type, val);
  }
  return false;
}

bool OSLRenderServices::get_object_standard_attribute(
    ShaderGlobals *globals, OSLUStringHash name, const TypeDesc type, bool derivatives, void *val)
{
  ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  /* todo: turn this into hash table? */

  /* Object Attributes */
  if (name == u_object_location) {
    const float3 f = object_location(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_object_color) {
    const float3 f = object_color(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_object_alpha) {
    const float f = object_alpha(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_object_index) {
    const float f = object_pass_id(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_object_is_light) {
    const float f = (sd->type & PRIMITIVE_LAMP) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_geom_dupli_generated) {
    const float3 f = object_dupli_generated(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_geom_dupli_uv) {
    const float3 f = object_dupli_uv(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_material_index) {
    const float f = shader_pass_id(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_object_random) {
    const float f = object_random_number(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }

  /* Particle Attributes */
  if (name == u_particle_index) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_index(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_random) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = hash_uint2_to_float(particle_index(kg, particle_id), 0);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_age) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_age(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_lifetime) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_lifetime(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_location) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_location(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
#if 0 /* unsupported */
  if (name == u_particle_rotation) {
    int particle_id = object_particle_id(kg, sd->object);
    float4 f = particle_rotation(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
#endif
  if (name == u_particle_size) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_size(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_velocity) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_particle_angular_velocity) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_angular_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }

  /* Geometry Attributes */
  if (name == u_geom_numpolyvertices) {
    return set_attribute(3, type, derivatives, val);
  }
  if ((name == u_geom_trianglevertices || name == u_geom_polyvertices) &&
      sd->type & PRIMITIVE_TRIANGLE)
  {
    float3 P[3];

    if (sd->type & PRIMITIVE_MOTION) {
      motion_triangle_vertices(kg, sd->object, sd->prim, sd->time, P);
    }
    else {
      triangle_vertices(kg, sd->prim, P);
    }

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_position_transform(kg, sd, &P[0]);
      object_position_transform(kg, sd, &P[1]);
      object_position_transform(kg, sd, &P[2]);
    }

    return set_attribute_float3_3(P, type, derivatives, val);
  }
  if (name == u_geom_name) {
    const ustring object_name = kg->osl.globals->object_names[sd->object];
    return set_attribute(object_name, type, derivatives, val);
  }
  if (name == u_is_smooth) {
    const float f = ((sd->shader & SHADER_SMOOTH_NORMAL) != 0);
    return set_attribute(f, type, derivatives, val);
  }
#ifdef __HAIR__
  /* Hair Attributes */
  if (name == u_is_curve) {
    const float f = (sd->type & PRIMITIVE_CURVE) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_curve_thickness) {
    const float f = curve_thickness(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_curve_tangent_normal) {
    const float3 f = curve_tangent_normal(sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_curve_random) {
    const float f = curve_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif
#ifdef __POINTCLOUD__
  /* point attributes */
  if (name == u_is_point) {
    const float f = (sd->type & PRIMITIVE_POINT) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_point_radius) {
    const float f = point_radius(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_point_position) {
    const float3 f = point_position(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_point_random) {
    const float f = point_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif
  if (name == u_normal_map_normal) {
    if (sd->type & PRIMITIVE_TRIANGLE) {
      const float3 f = triangle_smooth_normal_unnormalized(kg, sd, sd->Ng, sd->prim, sd->u, sd->v);
      return set_attribute(f, type, derivatives, val);
    }
    return false;
  }
  if (name == u_bump_map_normal) {
    dual3 f;
    if (!attribute_bump_map_normal(kg, sd, f)) {
      return false;
    }
    return set_attribute(f, type, derivatives, val);
  }
  return get_background_attribute(globals, name, type, derivatives, val);
}

bool OSLRenderServices::get_background_attribute(
    ShaderGlobals *globals, OSLUStringHash name, const TypeDesc type, bool derivatives, void *val)
{
  ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  const IntegratorStateCPU *state = globals->path_state;
  const IntegratorShadowStateCPU *shadow_state = globals->shadow_path_state;
  if (name == u_path_ray_length) {
    /* Ray Length */
    const float f = sd->ray_length;
    return set_attribute(f, type, derivatives, val);
  }

#define READ_PATH_STATE(elem) \
  ((state != nullptr)        ? state->path.elem : \
   (shadow_state != nullptr) ? shadow_state->shadow_path.elem : \
                               0)

  if (name == u_path_ray_depth) {
    /* Ray Depth */
    int f = READ_PATH_STATE(bounce);

    /* Read bounce from different locations depending on if this is a shadow path. For background,
     * light emission and shadow evaluation from a surface or volume we are effectively one bounce
     * further. */
    if (globals->raytype & (PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
      f += 1;
    }

    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_path_diffuse_depth) {
    /* Diffuse Ray Depth */
    const int f = READ_PATH_STATE(diffuse_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_path_glossy_depth) {
    /* Glossy Ray Depth */
    const int f = READ_PATH_STATE(glossy_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_path_transmission_depth) {
    /* Transmission Ray Depth */
    const int f = READ_PATH_STATE(transmission_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_path_transparent_depth) {
    /* Transparent Ray Depth */
    const int f = READ_PATH_STATE(transparent_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == u_path_portal_depth) {
    /* Portal Ray Depth */
    const int f = READ_PATH_STATE(portal_bounce);
    return set_attribute(f, type, derivatives, val);
  }
#undef READ_PATH_STATE

  if (name == u_ndc) {
    /* NDC coordinates with special exception for orthographic projection. */
    dual3 ndc;

    if ((globals->raytype & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
        kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
    {
      ndc.val = camera_world_to_ndc(kg, sd, sd->ray_P);
    }
    else {
      ndc.val = camera_world_to_ndc(kg, sd, sd->P);

      if (derivatives) {
        const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
        ndc.dx = camera_world_to_ndc(kg, sd, sd->P + dP.dx) - ndc.val;
        ndc.dy = camera_world_to_ndc(kg, sd, sd->P + dP.dy) - ndc.val;
      }
    }

    return set_attribute(ndc, type, derivatives, val);
  }

  return false;
}

bool OSLRenderServices::get_camera_attribute(
    ShaderGlobals *globals, OSLUStringHash name, TypeDesc type, bool derivatives, void *val)
{
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  if (name == u_sensor_size) {
    const float2 sensor = make_float2(kernel_data.cam.sensorwidth, kernel_data.cam.sensorheight);
    return set_attribute(sensor, type, derivatives, val);
  }
  if (name == u_image_resolution) {
    const float2 image = make_float2(kernel_data.cam.width, kernel_data.cam.height);
    return set_attribute(image, type, derivatives, val);
  }
  if (name == u_aperture_aspect_ratio) {
    return set_attribute(1.0f / kernel_data.cam.inv_aperture_ratio, type, derivatives, val);
  }
  if (name == u_aperture_size) {
    return set_attribute(kernel_data.cam.aperturesize, type, derivatives, val);
  }
  if (name == u_aperture_position) {
    /* The random numbers for aperture sampling are packed into N. */
    const float2 rand_lens = make_float2(globals->N.x, globals->N.y);
    const float2 pos = camera_sample_aperture(&kernel_data.cam, rand_lens);
    return set_attribute(pos * kernel_data.cam.aperturesize, type, derivatives, val);
  }
  if (name == u_focal_distance) {
    return set_attribute(kernel_data.cam.focaldistance, type, derivatives, val);
  }
  return false;
}

bool OSLRenderServices::get_attribute(OSL::ShaderGlobals *sg,
                                      bool derivatives,
                                      OSLUStringHash object_name,
                                      const TypeDesc type,
                                      OSLUStringHash name,
                                      void *val)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  if (globals == nullptr) {
    return false;
  }

  ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  if (sd == nullptr) {
    /* Camera shader. */
    return get_camera_attribute(globals, name, type, derivatives, val);
  }

  /* lookup of attribute on another object */
  int object;
  if (object_name != u_empty) {
    const OSLGlobals::ObjectNameMap::iterator it = kg->osl.globals->object_name_map.find(
        object_name);

    if (it == kg->osl.globals->object_name_map.end()) {
      return false;
    }

    object = it->second;
  }
  else {
    object = sd->object;
  }

  /* find attribute on object */
  const AttributeDescriptor desc = find_attribute(kg, object, sd->prim, name.hash());
  if (desc.offset != ATTR_STD_NOT_FOUND) {
    return get_object_attribute(kg, sd, desc, type, derivatives, val);
  }

  /* not found in attribute, check standard object info */
  return get_object_standard_attribute(globals, name, type, derivatives, val);
}

bool OSLRenderServices::get_userdata(bool /*derivatives*/,
                                     OSLUStringHash /* name*/,
                                     const TypeDesc /* type*/,
                                     OSL::ShaderGlobals * /*sg*/,
                                     void * /*val*/)
{
  return false; /* disabled by lockgeom */
}

OSL::TextureSystem::TextureHandle *OSLRenderServices::get_texture_handle(
    OSLUStringHash filename, OSL::ShadingContext *context, const OSL::TextureOpt *opt)
{
  return get_texture_handle(to_ustring(filename), context, opt);
}

OSL::TextureSystem::TextureHandle *OSLRenderServices::get_texture_handle(
    OSL::ustring filename, OSL::ShadingContext * /*context*/, const OSL::TextureOpt * /*options*/)
{
  OSLTextureHandleMap::iterator it = textures.find(filename);

  if (device_type_ == DEVICE_CPU) {
    /* For non-OIIO textures, just return a pointer to our own OSLTextureHandle. */
    if (it != textures.end()) {
      if (it->second.type != OSLTextureHandle::OIIO) {
        return (OSL::TextureSystem::TextureHandle *)(&it->second);
      }
    }

    /* Get handle from OpenImageIO. */
    OSL::TextureSystem *ts = m_texturesys;
    OSL::TextureSystem::TextureHandle *handle = ts->get_texture_handle(to_ustring(filename));
    if (handle == nullptr) {
      return nullptr;
    }

    /* Insert new OSLTextureHandle if needed. */
    if (it == textures.end()) {
      textures.insert(filename, OSLTextureHandle(OSLTextureHandle::OIIO));
      it = textures.find(filename);
    }

    /* Assign OIIO texture handle and return.
     * OIIO::unordered_map_concurrent always returns a const handle even if the underlying
     * std::unordered_map supports updating values just fine. */
    const_cast<OSLTextureHandle &>(it->second).oiio_handle = handle;
    return (OSL::TextureSystem::TextureHandle *)(&it->second);
  }

  /* Construct GPU texture handle for existing textures. */
  if (it != textures.end()) {
    switch (it->second.type) {
      case OSLTextureHandle::OIIO:
        return nullptr;
      case OSLTextureHandle::SVM:
        if (!it->second.handle.empty() && it->second.handle.get_manager() != image_manager) {
          it.clear();
          break;
        }
        return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(OSL_TEXTURE_HANDLE_TYPE_SVM |
                                                                     it->second.svm_slots[0].y);
      case OSLTextureHandle::IES:
        if (!it->second.handle.empty() && it->second.handle.get_manager() != image_manager) {
          it.clear();
          break;
        }
        return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(OSL_TEXTURE_HANDLE_TYPE_IES |
                                                                     it->second.svm_slots[0].y);
      case OSLTextureHandle::AO:
        return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(
            OSL_TEXTURE_HANDLE_TYPE_AO_OR_BEVEL | 1);
      case OSLTextureHandle::BEVEL:
        return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(
            OSL_TEXTURE_HANDLE_TYPE_AO_OR_BEVEL | 2);
    }
  }

  if (!image_manager) {
    return nullptr;
  }

  /* Load new textures using SVM image manager. */
  const ImageHandle handle = image_manager->add_image(filename.string(), ImageParams());
  if (handle.empty()) {
    return nullptr;
  }

  if (!textures.insert(filename, OSLTextureHandle(handle))) {
    return nullptr;
  }

  return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(OSL_TEXTURE_HANDLE_TYPE_SVM |
                                                               handle.svm_slot());
}

bool OSLRenderServices::good(OSL::TextureSystem::TextureHandle *texture_handle)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;

  if (handle->oiio_handle) {
    OSL::TextureSystem *ts = m_texturesys;
    return ts->good(handle->oiio_handle);
  }
  return true;
}

bool OSLRenderServices::texture(OSLUStringHash filename,
                                TextureHandle *texture_handle,
                                TexturePerthread *texture_thread_info,
                                OSL::TextureOpt &options,
                                OSL::ShaderGlobals *sg,
                                float s,
                                float t,
                                const float dsdx,
                                const float dtdx,
                                const float dsdy,
                                const float dtdy,
                                const int nchannels,
                                float *result,
                                float *dresultds,
                                float *dresultdt,
                                OSLUStringHash * /*errormessage*/)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  const OSLTextureHandle::Type texture_type = (handle) ? handle->type : OSLTextureHandle::OIIO;
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kernel_globals = globals->kg;
  const IntegratorStateCPU *state = globals->path_state;
  bool status = false;

  switch (texture_type) {
    case OSLTextureHandle::BEVEL: {
#ifdef __SHADER_RAYTRACE__
      /* Bevel shader hack. */
      if (nchannels >= 3 && state != nullptr) {
        const int num_samples = (int)s;
        const float radius = t;
        const float3 N = svm_bevel(kernel_globals, state, sd, radius, num_samples);
        result[0] = N.x;
        result[1] = N.y;
        result[2] = N.z;
        status = true;
      }
#endif
      break;
    }
    case OSLTextureHandle::AO: {
#ifdef __SHADER_RAYTRACE__
      /* AO shader hack. */
      if (state != nullptr) {
        const int num_samples = (int)s;
        const float radius = t;
        const float3 N = make_float3(dsdx, dtdx, dsdy);
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
        result[0] = svm_ao(kernel_globals, state, sd, N, radius, num_samples, flags);
        status = true;
      }
#endif
      break;
    }
    case OSLTextureHandle::SVM: {
      int id = -1;
      if (handle->svm_slots[0].w == -1) {
        /* Packed single texture. */
        id = handle->svm_slots[0].y;
      }
      else {
        /* Packed tiled texture. */
        const int tx = (int)s;
        const int ty = (int)t;
        const int tile = 1001 + 10 * ty + tx;
        for (const int4 &tile_node : handle->svm_slots) {
          if (tile_node.x == tile) {
            id = tile_node.y;
            break;
          }
          if (tile_node.z == tile) {
            id = tile_node.w;
            break;
          }
        }
        s -= tx;
        t -= ty;
      }

      float4 rgba;
      if (id == -1) {
        rgba = make_float4(
            TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B, TEX_IMAGE_MISSING_A);
      }
      else {
        rgba = kernel_tex_image_interp(kernel_globals, id, s, 1.0f - t);
      }

      result[0] = rgba[0];
      if (nchannels > 1) {
        result[1] = rgba[1];
      }
      if (nchannels > 2) {
        result[2] = rgba[2];
      }
      if (nchannels > 3) {
        result[3] = rgba[3];
      }
      status = true;
      break;
    }
    case OSLTextureHandle::IES: {
      /* IES light. */
      result[0] = kernel_ies_interp(kernel_globals, handle->svm_slots[0].y, s, t);
      status = true;
      break;
    }
    case OSLTextureHandle::OIIO: {
      /* OpenImageIO texture cache. */
      OSL::TextureSystem *ts = m_texturesys;

      if (handle && handle->oiio_handle) {
        if (texture_thread_info == nullptr) {
          texture_thread_info = kernel_globals->osl.oiio_thread_info;
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
        status = ts->texture(to_ustring(filename),
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

      if (nchannels == 4) {
        result[3] = 1.0f;
      }
    }
  }

  return status;
}

bool OSLRenderServices::texture3d(OSLUStringHash filename,
                                  TextureHandle *texture_handle,
                                  TexturePerthread *texture_thread_info,
                                  OSL::TextureOpt &options,
                                  OSL::ShaderGlobals *sg,
                                  const OSL::Vec3 &P,
                                  const OSL::Vec3 &dPdx,
                                  const OSL::Vec3 &dPdy,
                                  const OSL::Vec3 &dPdz,
                                  const int nchannels,
                                  float *result,
                                  float *dresultds,
                                  float *dresultdt,
                                  float *dresultdr,
                                  OSLUStringHash * /*errormessage*/)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  const OSLTextureHandle::Type texture_type = (handle) ? handle->type : OSLTextureHandle::OIIO;
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kernel_globals = globals->kg;
  bool status = false;

  switch (texture_type) {
    case OSLTextureHandle::SVM: {
      /* Packed texture. */
      const int slot = handle->svm_slots[0].y;
      const float3 P_float3 = make_float3(P.x, P.y, P.z);
      float4 rgba = kernel_tex_image_interp_3d(
          kernel_globals, slot, P_float3, INTERPOLATION_NONE, -1.0f);

      result[0] = rgba[0];
      if (nchannels > 1) {
        result[1] = rgba[1];
      }
      if (nchannels > 2) {
        result[2] = rgba[2];
      }
      if (nchannels > 3) {
        result[3] = rgba[3];
      }
      status = true;
      break;
    }
    case OSLTextureHandle::OIIO: {
      /* OpenImageIO texture cache. */
      OSL::TextureSystem *ts = m_texturesys;

      if (handle && handle->oiio_handle) {
        if (texture_thread_info == nullptr) {
          texture_thread_info = kernel_globals->osl.oiio_thread_info;
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
        status = ts->texture3d(to_ustring(filename),
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

      if (nchannels == 4) {
        result[3] = 1.0f;
      }
    }
  }

  return status;
}

bool OSLRenderServices::environment(OSLUStringHash filename,
                                    TextureHandle *texture_handle,
                                    TexturePerthread *thread_info,
                                    OSL::TextureOpt &options,
                                    OSL::ShaderGlobals *sg,
                                    const OSL::Vec3 &R,
                                    const OSL::Vec3 &dRdx,
                                    const OSL::Vec3 &dRdy,
                                    const int nchannels,
                                    float *result,
                                    float *dresultds,
                                    float *dresultdt,
                                    OSLUStringHash * /*errormessage*/)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  OSL::TextureSystem *ts = m_texturesys;
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  bool status = false;

  if (handle && handle->oiio_handle) {
    if (thread_info == nullptr) {
      thread_info = globals->kg->osl.oiio_thread_info;
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
        to_ustring(filename), options, R, dRdx, dRdy, nchannels, result, dresultds, dresultdt);
  }

  if (!status) {
    if (nchannels == 3 || nchannels == 4) {
      result[0] = 1.0f;
      result[1] = 0.0f;
      result[2] = 1.0f;

      if (nchannels == 4) {
        result[3] = 1.0f;
      }
    }
  }
  else if (handle && handle->processor) {
    ColorSpaceManager::to_scene_linear(handle->processor, result, nchannels);
  }

  return status;
}

bool OSLRenderServices::get_texture_info(OSLUStringHash filename,
                                         TextureHandle *texture_handle,
                                         TexturePerthread *texture_thread_info,
                                         OSL::ShaderGlobals * /*sg*/,
                                         const int subimage,
                                         OSLUStringHash dataname,
                                         const TypeDesc datatype,
                                         void *data,
                                         OSLUStringHash * /*errormessage*/)
{
  OSLTextureHandle *handle = (OSLTextureHandle *)texture_handle;
  OSL::TextureSystem *ts = m_texturesys;

  if (handle) {
    /* No texture info for other texture types. */
    if (handle->type != OSLTextureHandle::OIIO) {
      return false;
    }

    if (handle->oiio_handle) {
      /* Get texture info from OpenImageIO. */
      return ts->get_texture_info(handle->oiio_handle,
                                  texture_thread_info,
                                  subimage,
                                  to_ustring(dataname),
                                  datatype,
                                  data);
    }
  }

  /* Get texture info from OpenImageIO, slower using filename. */
  return ts->get_texture_info(
      to_ustring(filename), subimage, to_ustring(dataname), datatype, data);
}

int OSLRenderServices::pointcloud_search(OSL::ShaderGlobals * /*sg*/,
                                         OSLUStringHash /*filename*/,
                                         const OSL::Vec3 & /*center*/,
                                         const float /*radius*/,
                                         const int /*max_points*/,
                                         bool /*sort*/,
#if OSL_LIBRARY_VERSION_CODE >= 11400
                                         int * /*indices*/,
#else
                                         size_t * /*out_indices*/,
#endif
                                         float * /*out_distances*/,
                                         const int /*derivs_offset*/)
{
  return 0;
}

int OSLRenderServices::pointcloud_get(OSL::ShaderGlobals * /*sg*/
                                      ,
                                      OSLUStringHash /*filename*/,
#if OSL_LIBRARY_VERSION_CODE >= 11400
                                      const int * /*indices*/,
#else
                                      size_t * /*indices*/,
#endif
                                      const int /*count*/,
                                      OSLUStringHash /*attr_name*/,
                                      const TypeDesc /*attr_type*/,
                                      void * /*out_data*/)
{
  return 0;
}

bool OSLRenderServices::pointcloud_write(OSL::ShaderGlobals * /*sg*/,
                                         OSLUStringHash /*filename*/,
                                         const OSL::Vec3 & /*pos*/,
                                         const int /*nattribs*/,
                                         const OSLUStringRep * /*names*/,
                                         const TypeDesc * /*types*/,
                                         const void ** /*data*/)
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
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  ShaderData *sd = globals->sd;
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  if (sd == nullptr) {
    return false;
  }

  /* setup ray */
  Ray ray;

  ray.P = make_float3(P.x, P.y, P.z);
  ray.D = make_float3(R.x, R.y, R.z);
  ray.tmin = 0.0f;
  ray.tmax = (options.maxdist == 1.0e30f) ? FLT_MAX : options.maxdist - options.mindist;
  ray.time = sd->time;
  ray.self.object = OBJECT_NONE;
  ray.self.prim = PRIM_NONE;
  ray.self.light_object = OBJECT_NONE;
  ray.self.light_prim = PRIM_NONE;

  if (options.mindist == 0.0f) {
    /* avoid self-intersections */
    if (ray.P == sd->P) {
      ray.self.object = sd->object;
      ray.self.prim = sd->prim;
    }
  }
  else {
    /* offset for minimum distance */
    ray.P += options.mindist * ray.D;
  }

  /* ray differentials */
  differential3 dP;
  dP.dx = make_float3(dPdx.x, dPdx.y, dPdx.z);
  dP.dy = make_float3(dPdy.x, dPdy.y, dPdy.z);
  ray.dP = differential_make_compact(dP);
  differential3 dD;
  dD.dx = make_float3(dRdx.x, dRdx.y, dRdx.z);
  dD.dy = make_float3(dRdy.x, dRdy.y, dRdy.z);
  ray.dD = differential_make_compact(dD);

  /* allocate trace data */
  OSLTraceData *tracedata = globals->tracedata;
  tracedata->ray = ray;
  tracedata->setup = false;
  tracedata->init = true;
  tracedata->hit = false;

  /* Can't ray-trace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return false;
  }

  /* Ray-trace, leaving out shadow opaque to avoid early exit. */
  const uint visibility = PATH_RAY_ALL_VISIBILITY - PATH_RAY_SHADOW_OPAQUE;
  tracedata->hit = scene_intersect(kg, &ray, visibility, &tracedata->isect);
  return tracedata->hit;
}

bool OSLRenderServices::getmessage(OSL::ShaderGlobals *sg,
                                   OSLUStringHash source,
                                   OSLUStringHash name,
                                   const TypeDesc type,
                                   void *val,
                                   bool derivatives)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;
  OSLTraceData *tracedata = globals->tracedata;

  if (source == u_trace && tracedata->init) {
    if (name == u_hit) {
      return set_attribute<int>(tracedata->hit, type, derivatives, val);
    }
    if (tracedata->hit) {
      if (name == u_hitdist) {
        return set_attribute(tracedata->isect.t, type, derivatives, val);
      }

      ShaderData *sd = &tracedata->sd;

      if (!tracedata->setup) {
        /* lazy shader data setup */
        shader_setup_from_ray(kg, sd, &tracedata->ray, &tracedata->isect);
        tracedata->setup = true;
      }

      if (name == u_N) {
        return set_attribute(sd->N, type, derivatives, val);
      }
      if (name == u_Ng) {
        return set_attribute(sd->Ng, type, derivatives, val);
      }
      if (name == u_P) {
        const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
        return set_attribute(dual3(sd->P, dP.dx, dP.dy), type, derivatives, val);
      }
      if (name == u_I) {
        const differential3 dI = differential_from_compact(sd->wi, sd->dI);
        return set_attribute(dual3(sd->wi, dI.dx, dI.dy), type, derivatives, val);
      }
      if (name == u_u) {
        return set_attribute(dual1(sd->u, sd->du.dx, sd->du.dy), type, derivatives, val);
      }
      if (name == u_v) {
        return set_attribute(dual1(sd->v, sd->dv.dx, sd->dv.dy), type, derivatives, val);
      }

      return get_attribute(sg, derivatives, u_empty, type, name, val);
    }
  }

  return false;
}

CCL_NAMESPACE_END
