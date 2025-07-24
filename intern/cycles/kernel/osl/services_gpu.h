/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#include "kernel/camera/camera.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/curve.h"
#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/object.h"
#include "kernel/geom/point.h"
#include "kernel/geom/primitive.h"
#include "kernel/geom/triangle.h"

#include "kernel/util/differential.h"
#include "kernel/util/ies.h"
#include "kernel/util/texture_3d.h"

#include "util/hash.h"
#include "util/transform.h"

#include "kernel/osl/osl.h"
#include "kernel/osl/services_shared.h"

namespace DeviceStrings {

/* "" */
ccl_device_constant DeviceString _emptystring_ = 0ull;
/* "common" */
ccl_device_constant DeviceString u_common = 14645198576927606093ull;
/* "world" */
ccl_device_constant DeviceString u_world = 16436542438370751598ull;
/* "shader" */
ccl_device_constant DeviceString u_shader = 4279676006089868ull;
/* "object" */
ccl_device_constant DeviceString u_object = 973692718279674627ull;
/* "NDC" */
ccl_device_constant DeviceString u_ndc = 5148305047403260775ull;
/* "screen" */
ccl_device_constant DeviceString u_screen = 14159088609039777114ull;
/* "camera" */
ccl_device_constant DeviceString u_camera = 2159505832145726196ull;
/* "raster" */
ccl_device_constant DeviceString u_raster = 7759263238610201778ull;
/* "colorsystem" */
ccl_device_constant DeviceString u_colorsystem = 1390623632464445670ull;
/* "object:location" */
ccl_device_constant DeviceString u_object_location = 7846190347358762897ull;
/* "object:color" */
ccl_device_constant DeviceString u_object_color = 12695623857059169556ull;
/* "object:alpha" */
ccl_device_constant DeviceString u_object_alpha = 11165053919428293151ull;
/* "object:index" */
ccl_device_constant DeviceString u_object_index = 6588325838217472556ull;
/* "object:is_light" */
ccl_device_constant DeviceString u_object_is_light = 13979755312845091842ull;
/* "geom:bump_map_normal" */
ccl_device_constant DeviceString u_bump_map_normal = 9592102745179132106ull;
/* "geom:dupli_generated" */
ccl_device_constant DeviceString u_geom_dupli_generated = 6715607178003388908ull;
/* "geom:dupli_uv" */
ccl_device_constant DeviceString u_geom_dupli_uv = 1294253317490155849ull;
/* "material:index" */
ccl_device_constant DeviceString u_material_index = 741770758159634623ull;
/* "object:random" */
ccl_device_constant DeviceString u_object_random = 15789063994977955884ull;
/* "particle:index" */
ccl_device_constant DeviceString u_particle_index = 9489711748229903784ull;
/* "particle:random" */
ccl_device_constant DeviceString u_particle_random = 17993722202766855761ull;
/* "particle:age" */
ccl_device_constant DeviceString u_particle_age = 7380730644710951109ull;
/* "particle:lifetime" */
ccl_device_constant DeviceString u_particle_lifetime = 16576828923156200061ull;
/* "particle:location" */
ccl_device_constant DeviceString u_particle_location = 10309536211423573010ull;
/* "particle:rotation" */
ccl_device_constant DeviceString u_particle_rotation = 17858543768041168459ull;
/* "particle:size" */
ccl_device_constant DeviceString u_particle_size = 16461524249715420389ull;
/* "particle:velocity" */
ccl_device_constant DeviceString u_particle_velocity = 13199101248768308863ull;
/* "particle:angular_velocity" */
ccl_device_constant DeviceString u_particle_angular_velocity = 16327930120486517910ull;
/* "geom:numpolyvertices" */
ccl_device_constant DeviceString u_geom_numpolyvertices = 382043551489988826ull;
/* "geom:trianglevertices" */
ccl_device_constant DeviceString u_geom_trianglevertices = 17839267571524187074ull;
/* "geom:polyvertices" */
ccl_device_constant DeviceString u_geom_polyvertices = 1345577201967881769ull;
/* "geom:name" */
ccl_device_constant DeviceString u_geom_name = 13606338128269760050ull;
/* "geom:undisplaced" */
ccl_device_constant DeviceString u_geom_undisplaced = 12431586303019276305ull;
/* "geom:is_smooth" */
ccl_device_constant DeviceString u_is_smooth = 857544214094480123ull;
/* "geom:is_curve" */
ccl_device_constant DeviceString u_is_curve = 129742495633653138ull;
/* "geom:curve_thickness" */
ccl_device_constant DeviceString u_curve_thickness = 10605802038397633852ull;
/* "geom:curve_length" */
ccl_device_constant DeviceString u_curve_length = 11423459517663715453ull;
/* "geom:curve_tangent_normal" */
ccl_device_constant DeviceString u_curve_tangent_normal = 12301397394034985633ull;
/* "geom:curve_random" */
ccl_device_constant DeviceString u_curve_random = 15293085049960492358ull;
/* "geom:is_point" */
ccl_device_constant DeviceString u_is_point = 2511357849436175953ull;
/* "geom:point_radius" */
ccl_device_constant DeviceString u_point_radius = 9956381140398668479ull;
/* "geom:point_position" */
ccl_device_constant DeviceString u_point_position = 15684484280742966916ull;
/* "geom:point_random" */
ccl_device_constant DeviceString u_point_random = 5632627207092325544ull;
/* "geom:normal_map_normal" */
ccl_device_constant DeviceString u_normal_map_normal = 10718948685686827073;
/* "path:ray_length" */
ccl_device_constant DeviceString u_path_ray_length = 16391985802412544524ull;
/* "path:ray_depth" */
ccl_device_constant DeviceString u_path_ray_depth = 16643933224879500399ull;
/* "path:diffuse_depth" */
ccl_device_constant DeviceString u_path_diffuse_depth = 13191651286699118408ull;
/* "path:glossy_depth" */
ccl_device_constant DeviceString u_path_glossy_depth = 15717768399057252940ull;
/* "path:transparent_depth" */
ccl_device_constant DeviceString u_path_transparent_depth = 7821650266475578543ull;
/* "path:transmission_depth" */
ccl_device_constant DeviceString u_path_transmission_depth = 15113408892323917624ull;
/* "path:portal_depth" */
ccl_device_constant DeviceString u_path_portal_depth = 13191651286699118408ull;
/* "cam:sensor_size" */
ccl_device_constant DeviceString u_sensor_size = 7525693591727141378ull;
/* "cam:image_resolution" */
ccl_device_constant DeviceString u_image_resolution = 5199143367706113607ull;
/* "cam:aperture_aspect_ratio" */
ccl_device_constant DeviceString u_aperture_aspect_ratio = 8708221138893210943ull;
/* "cam:aperture_size" */
ccl_device_constant DeviceString u_aperture_size = 3708482920470008383ull;
/* "cam:aperture_position" */
ccl_device_constant DeviceString u_aperture_position = 12926784411960338650ull;
/* "cam:focal_distance" */
ccl_device_constant DeviceString u_focal_distance = 7162995161881858159ull;

}  // namespace DeviceStrings

/* Closure */

ccl_device_extern ccl_private OSLClosure *osl_mul_closure_color(ccl_private ShaderGlobals *sg,
                                                                ccl_private OSLClosure *a,
                                                                const ccl_private float3 *weight)
{
  if (*weight == zero_float3() || !a) {
    return nullptr;
  }
  else if (*weight == one_float3()) {
    return a;
  }

  ccl_private uint8_t *closure_pool = sg->closure_pool;
  /* Align pointer to closure struct requirement */
  closure_pool = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(closure_pool) + alignof(OSLClosureMul) - 1) &
      (-alignof(OSLClosureMul)));
  sg->closure_pool = closure_pool + sizeof(OSLClosureMul);

  ccl_private OSLClosureMul *const closure = reinterpret_cast<ccl_private OSLClosureMul *>(
      closure_pool);
  closure->id = OSL_CLOSURE_MUL_ID;
  closure->weight = *weight;
  closure->closure = a;

  return closure;
}

ccl_device_extern ccl_private OSLClosure *osl_mul_closure_float(ccl_private ShaderGlobals *sg,
                                                                ccl_private OSLClosure *a,
                                                                const float weight)
{
  if (weight == 0.0f || !a) {
    return nullptr;
  }
  else if (weight == 1.0f) {
    return a;
  }

  ccl_private uint8_t *closure_pool = sg->closure_pool;
  /* Align pointer to closure struct requirement */
  closure_pool = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(closure_pool) + alignof(OSLClosureMul) - 1) &
      (-alignof(OSLClosureMul)));
  sg->closure_pool = closure_pool + sizeof(OSLClosureMul);

  ccl_private OSLClosureMul *const closure = reinterpret_cast<ccl_private OSLClosureMul *>(
      closure_pool);
  closure->id = OSL_CLOSURE_MUL_ID;
  closure->weight = make_float3(weight, weight, weight);
  closure->closure = a;

  return closure;
}

ccl_device_extern ccl_private OSLClosure *osl_add_closure_closure(ccl_private ShaderGlobals *sg,
                                                                  ccl_private OSLClosure *a,
                                                                  ccl_private OSLClosure *b)
{
  if (!a) {
    return b;
  }
  if (!b) {
    return a;
  }

  ccl_private uint8_t *closure_pool = sg->closure_pool;
  /* Align pointer to closure struct requirement */
  closure_pool = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(closure_pool) + alignof(OSLClosureAdd) - 1) &
      (-alignof(OSLClosureAdd)));
  sg->closure_pool = closure_pool + sizeof(OSLClosureAdd);

  ccl_private OSLClosureAdd *const closure = reinterpret_cast<ccl_private OSLClosureAdd *>(
      closure_pool);
  closure->id = OSL_CLOSURE_ADD_ID;
  closure->closureA = a;
  closure->closureB = b;

  return closure;
}

ccl_device_extern ccl_private OSLClosure *osl_allocate_closure_component(
    ccl_private ShaderGlobals *sg, const int id, const int size)
{
  ccl_private uint8_t *closure_pool = sg->closure_pool;
  /* Align pointer to closure struct requirement */
  closure_pool = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(closure_pool) + alignof(OSLClosureComponent) - 1) &
      (-alignof(OSLClosureComponent)));
  sg->closure_pool = closure_pool + sizeof(OSLClosureComponent) + size;

  ccl_private OSLClosureComponent *const closure =
      reinterpret_cast<ccl_private OSLClosureComponent *>(closure_pool);
  closure->id = static_cast<OSLClosureType>(id);
  closure->weight = one_float3();

  return closure;
}

ccl_device_extern ccl_private OSLClosure *osl_allocate_weighted_closure_component(
    ccl_private ShaderGlobals *sg, const int id, const int size, const ccl_private float3 *weight)
{
  ccl_private uint8_t *closure_pool = sg->closure_pool;
  /* Align pointer to closure struct requirement */
  closure_pool = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(closure_pool) + alignof(OSLClosureComponent) - 1) &
      (-alignof(OSLClosureComponent)));
  sg->closure_pool = closure_pool + sizeof(OSLClosureComponent) + size;

  ccl_private OSLClosureComponent *const closure =
      reinterpret_cast<ccl_private OSLClosureComponent *>(closure_pool);
  closure->id = static_cast<OSLClosureType>(id);
  closure->weight = *weight;

  return closure;
}

/* Utilities */

ccl_device_extern void osl_error(ccl_private ShaderGlobals *sg, DeviceString format, void *args) {}

ccl_device_extern void osl_printf(ccl_private ShaderGlobals *sg, DeviceString format, void *args)
{
}

ccl_device_extern void osl_warning(ccl_private ShaderGlobals *sg, DeviceString format, void *args)
{
}

ccl_device_extern void osl_fprintf(ccl_private ShaderGlobals *sg,
                                   DeviceString filename,
                                   DeviceString format,
                                   void *args)
{
}

ccl_device_extern uint osl_range_check_err(const int indexvalue,
                                           const int length,
                                           DeviceString symname,
                                           ccl_private ShaderGlobals *sg,
                                           DeviceString sourcefile,
                                           const int sourceline,
                                           DeviceString groupname,
                                           const int layer,
                                           DeviceString layername,
                                           DeviceString shadername)
{
  const int result = indexvalue < 0 ? 0 : indexvalue >= length ? length - 1 : indexvalue;
#if 0
  if (result != indexvalue) {
    printf("Index [%d] out of range\n", indexvalue);
  }
#endif
  return result;
}

/* Matrix Utilities */

ccl_device_forceinline void copy_matrix(ccl_private float *res, const Transform &tfm)
{
  res[0] = tfm.x.x;
  res[1] = tfm.y.x;
  res[2] = tfm.z.x;
  res[3] = 0.0f;
  res[4] = tfm.x.y;
  res[5] = tfm.y.y;
  res[6] = tfm.z.y;
  res[7] = 0.0f;
  res[8] = tfm.x.z;
  res[9] = tfm.y.z;
  res[10] = tfm.z.z;
  res[11] = 0.0f;
  res[12] = tfm.x.w;
  res[13] = tfm.y.w;
  res[14] = tfm.z.w;
  res[15] = 1.0f;
}
ccl_device_forceinline void copy_matrix(ccl_private float *res, const ProjectionTransform &tfm)
{
  res[0] = tfm.x.x;
  res[1] = tfm.y.x;
  res[2] = tfm.z.x;
  res[3] = tfm.w.x;
  res[4] = tfm.x.y;
  res[5] = tfm.y.y;
  res[6] = tfm.z.y;
  res[7] = tfm.w.y;
  res[8] = tfm.x.z;
  res[9] = tfm.y.z;
  res[10] = tfm.z.z;
  res[11] = tfm.w.z;
  res[12] = tfm.x.w;
  res[13] = tfm.y.w;
  res[14] = tfm.z.w;
  res[15] = tfm.w.w;
}

ccl_device_extern bool osl_get_matrix(ccl_private ShaderGlobals *sg,
                                      ccl_private float *res,
                                      DeviceString from)
{
  if (from == DeviceStrings::u_common || from == DeviceStrings::u_world) {
    copy_matrix(res, projection_identity());
    return true;
  }
  if (from == DeviceStrings::u_shader || from == DeviceStrings::u_object) {
    KernelGlobals kg = nullptr;
    ccl_private ShaderData *const sd = sg->sd;
    int object = sd->object;

    if (object != OBJECT_NONE) {
      const Transform tfm = object_get_transform(kg, sd);
      copy_matrix(res, tfm);
      return true;
    }
  }
  else if (from == DeviceStrings::u_ndc) {
    copy_matrix(res, kernel_data.cam.ndctoworld);
    return true;
  }
  else if (from == DeviceStrings::u_raster) {
    copy_matrix(res, kernel_data.cam.rastertoworld);
    return true;
  }
  else if (from == DeviceStrings::u_screen) {
    copy_matrix(res, kernel_data.cam.screentoworld);
    return true;
  }
  else if (from == DeviceStrings::u_camera) {
    copy_matrix(res, kernel_data.cam.cameratoworld);
    return true;
  }

  return false;
}

ccl_device_extern bool osl_get_inverse_matrix(ccl_private ShaderGlobals *sg,
                                              ccl_private float *res,
                                              DeviceString to)
{
  if (to == DeviceStrings::u_common || to == DeviceStrings::u_world) {
    copy_matrix(res, projection_identity());
    return true;
  }
  if (to == DeviceStrings::u_shader || to == DeviceStrings::u_object) {
    KernelGlobals kg = nullptr;
    ccl_private ShaderData *const sd = sg->sd;
    int object = sd->object;

    if (object != OBJECT_NONE) {
      const Transform itfm = object_get_inverse_transform(kg, sd);
      copy_matrix(res, itfm);
      return true;
    }
  }
  else if (to == DeviceStrings::u_ndc) {
    copy_matrix(res, kernel_data.cam.worldtondc);
    return true;
  }
  else if (to == DeviceStrings::u_raster) {
    copy_matrix(res, kernel_data.cam.worldtoraster);
    return true;
  }
  else if (to == DeviceStrings::u_screen) {
    copy_matrix(res, kernel_data.cam.worldtoscreen);
    return true;
  }
  else if (to == DeviceStrings::u_camera) {
    copy_matrix(res, kernel_data.cam.worldtocamera);
    return true;
  }

  return false;
}

/* Attributes */

ccl_device_template_spec bool set_attribute(const dual1 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;
  const int type_arraylen = type >> 32;

  if (type_basetype == 11 /* TypeDesc::FLOAT */) {
    if ((type_aggregate == 3 /* TypeDesc::VEC3 */) || (type_aggregate == 1 && type_arraylen == 3))
    {
      set_data_float3(make_float3(v), derivatives, val);
      return true;
    }
    if ((type_aggregate == 4 /* TypeDesc::VEC4 */) || (type_aggregate == 1 && type_arraylen == 4))
    {
      set_data_float4(make_float4(make_float3(v)), derivatives, val);
      return true;
    }
    if ((type_aggregate == 1 /* TypeDesc::SCALAR */)) {
      set_data_float(v, derivatives, val);
      return true;
    }
  }

  return false;
}
ccl_device_template_spec bool set_attribute(const dual2 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;
  const int type_arraylen = type >> 32;

  if (type_basetype == 11 /* TypeDesc::FLOAT */) {
    if ((type_aggregate == 3 /* TypeDesc::VEC3 */) || (type_aggregate == 1 && type_arraylen == 3))
    {
      set_data_float3(make_float3(v), derivatives, val);
      return true;
    }
    if ((type_aggregate == 4 /* TypeDesc::VEC4 */) || (type_aggregate == 1 && type_arraylen == 4))
    {
      set_data_float4(make_float4(make_float3(v)), derivatives, val);
    }
    if ((type_aggregate == 1 /* TypeDesc::SCALAR */)) {
      set_data_float(average(v), derivatives, val);
      return true;
    }
  }

  return false;
}
ccl_device_template_spec bool set_attribute(const dual3 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;
  const int type_arraylen = type >> 32;

  if (type_basetype == 11 /* TypeDesc::FLOAT */) {
    if ((type_aggregate == 3 /* TypeDesc::VEC3 */) || (type_aggregate == 1 && type_arraylen == 3))
    {
      set_data_float3(v, derivatives, val);
      return true;
    }
    if ((type_aggregate == 4 /* TypeDesc::VEC4 */) || (type_aggregate == 1 && type_arraylen == 4))
    {
      set_data_float4(make_float4(v), derivatives, val);
      return true;
    }
    if ((type_aggregate == 1 /* TypeDesc::SCALAR */)) {
      set_data_float(average(v), derivatives, val);
      return true;
    }
  }

  return false;
}
ccl_device_template_spec bool set_attribute(const dual4 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;
  const int type_arraylen = type >> 32;

  if (type_basetype == 11 /* TypeDesc::FLOAT */) {
    if ((type_aggregate == 3 /* TypeDesc::VEC3 */) || (type_aggregate == 1 && type_arraylen == 3))
    {
      set_data_float3(make_float3(v), derivatives, val);
      return true;
    }
    if ((type_aggregate == 4 /* TypeDesc::VEC4 */) || (type_aggregate == 1 && type_arraylen == 4))
    {
      set_data_float4(v, derivatives, val);
      return true;
    }
    if ((type_aggregate == 1 /* TypeDesc::SCALAR */)) {
      set_data_float(average(make_float3(v)), derivatives, val);
      return true;
    }
  }

  return false;
}

template<typename T>
ccl_device_inline bool set_attribute(const T f,
                                     const TypeDesc type,
                                     bool derivatives,
                                     ccl_private void *val)
{
  return set_attribute(dual<T>(f), type, derivatives, val);
}

ccl_device_inline bool set_attribute_matrix(const ccl_private Transform &tfm,
                                            const TypeDesc type,
                                            ccl_private void *val)
{
  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;

  if (type_basetype == 11 /* TypeDesc::FLOAT */ && type_aggregate == 16 /* TypeDesc::MATRIX44 */) {
    copy_matrix(static_cast<ccl_private float *>(val), tfm);
    return true;
  }

  return false;
}

ccl_device_template_spec bool set_attribute(const int i,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  ccl_private int *ival = static_cast<ccl_private int *>(val);

  const unsigned char type_basetype = type & 0xFF;
  const unsigned char type_aggregate = (type >> 8) & 0xFF;
  const int type_arraylen = type >> 32;

  if ((type_basetype == 7 /* TypeDesc::INT */) && (type_aggregate == 1 /* TypeDesc::SCALAR */) &&
      type_arraylen == 0)
  {
    ival[0] = i;

    if (derivatives) {
      ival[1] = 0;
      ival[2] = 0;
    }

    return true;
  }

  return false;
}

ccl_device_inline bool get_background_attribute(KernelGlobals kg,
                                                ccl_private ShaderGlobals *sg,
                                                ccl_private ShaderData *sd,
                                                DeviceString name,
                                                const TypeDesc type,
                                                bool derivatives,
                                                ccl_private void *val)
{
  ConstIntegratorState state = (sg->shade_index > 0) ? (sg->shade_index - 1) : -1;
  ConstIntegratorShadowState shadow_state = (sg->shade_index < 0) ? (-sg->shade_index - 1) : -1;
  if (name == DeviceStrings::u_path_ray_length) {
    /* Ray Length */
    float f = sd->ray_length;
    return set_attribute(f, type, derivatives, val);
  }

#define READ_PATH_STATE(elem) \
  ((state != -1)        ? INTEGRATOR_STATE(state, path, elem) : \
   (shadow_state != -1) ? INTEGRATOR_STATE(shadow_state, shadow_path, elem) : \
                          0)

  if (name == DeviceStrings::u_path_ray_depth) {
    /* Ray Depth */
    int f = READ_PATH_STATE(bounce);

    /* Read bounce from different locations depending on if this is a shadow path. For background,
     * light emission and shadow evaluation from a surface or volume we are effectively one bounce
     * further. */
    if (sg->raytype & (PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
      f += 1;
    }

    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_path_diffuse_depth) {
    /* Diffuse Ray Depth */
    const int f = READ_PATH_STATE(diffuse_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_path_glossy_depth) {
    /* Glossy Ray Depth */
    const int f = READ_PATH_STATE(glossy_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_path_transmission_depth) {
    /* Transmission Ray Depth */
    const int f = READ_PATH_STATE(transmission_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_path_transparent_depth) {
    /* Transparent Ray Depth */
    const int f = READ_PATH_STATE(transparent_bounce);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_path_portal_depth) {
    /* Portal Ray Depth */
    const int f = READ_PATH_STATE(portal_bounce);
    return set_attribute(f, type, derivatives, val);
  }
#undef READ_PATH_STATE

  else if (name == DeviceStrings::u_ndc) {
    /* NDC coordinates with special exception for orthographic projection. */
    dual3 ndc;

    if ((sg->raytype & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
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

template<typename T>
ccl_device_inline bool get_object_attribute_impl(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 const AttributeDescriptor &desc,
                                                 const TypeDesc type,
                                                 bool derivatives,
                                                 ccl_private void *val)
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

ccl_device_inline bool get_object_attribute(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            const AttributeDescriptor &desc,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (desc.type == NODE_ATTR_FLOAT) {
    return get_object_attribute_impl<float>(kg, sd, desc, type, derivatives, val);
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    return get_object_attribute_impl<float2>(kg, sd, desc, type, derivatives, val);
  }
  else if (desc.type == NODE_ATTR_FLOAT3) {
    return get_object_attribute_impl<float3>(kg, sd, desc, type, derivatives, val);
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    return get_object_attribute_impl<float4>(kg, sd, desc, type, derivatives, val);
  }
  else if (desc.type == NODE_ATTR_MATRIX) {
    Transform tfm = primitive_attribute_matrix(kg, desc);
    return set_attribute_matrix(tfm, type, val);
  }

  return false;
}

ccl_device_inline bool get_object_standard_attribute(KernelGlobals kg,
                                                     ccl_private ShaderGlobals *sg,
                                                     ccl_private ShaderData *sd,
                                                     DeviceString name,
                                                     const TypeDesc type,
                                                     bool derivatives,
                                                     ccl_private void *val)
{
  /* Object attributes */
  if (name == DeviceStrings::u_object_location) {
    float3 f = object_location(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_object_color) {
    float3 f = object_color(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_object_alpha) {
    float f = object_alpha(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_object_index) {
    float f = object_pass_id(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_object_is_light) {
    float f = ((sd->type & PRIMITIVE_LAMP) != 0);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_geom_dupli_generated) {
    float3 f = object_dupli_generated(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_geom_dupli_uv) {
    float3 f = object_dupli_uv(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_material_index) {
    float f = shader_pass_id(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_object_random) {
    const float f = object_random_number(kg, sd->object);

    return set_attribute(f, type, derivatives, val);
  }

  /* Particle attributes */
  else if (name == DeviceStrings::u_particle_index) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_index(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_particle_random) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = hash_uint2_to_float(particle_index(kg, particle_id), 0);
    return set_attribute(f, type, derivatives, val);
  }

  else if (name == DeviceStrings::u_particle_age) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_age(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_particle_lifetime) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_lifetime(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_particle_location) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_location(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
#if 0 /* unsupported */
  else if (name == DeviceStrings::u_particle_rotation) {
    int particle_id = object_particle_id(kg, sd->object);
    float4 f = particle_rotation(kg, particle_id);
    return set_attribute4(f, type, derivatives, val);
  }
#endif
  else if (name == DeviceStrings::u_particle_size) {
    int particle_id = object_particle_id(kg, sd->object);
    float f = particle_size(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_particle_velocity) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_particle_angular_velocity) {
    int particle_id = object_particle_id(kg, sd->object);
    float3 f = particle_angular_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }

  /* Geometry attributes */
#if 0 /* TODO */
  else if (name == DeviceStrings::u_geom_numpolyvertices) {
    return false;
  }
  else if (name == DeviceStrings::u_geom_trianglevertices ||
            name == DeviceStrings::u_geom_polyvertices) {
    return false;
  }
  else if (name == DeviceStrings::u_geom_name) {
    return false;
  }
#endif
  else if (name == DeviceStrings::u_is_smooth) {
    float f = ((sd->shader & SHADER_SMOOTH_NORMAL) != 0);
    return set_attribute(f, type, derivatives, val);
  }

#ifdef __HAIR__
  /* Hair attributes */
  else if (name == DeviceStrings::u_is_curve) {
    float f = (sd->type & PRIMITIVE_CURVE) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_curve_thickness) {
    float f = curve_thickness(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_curve_tangent_normal) {
    float3 f = curve_tangent_normal(sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_curve_random) {
    float f = curve_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif

#ifdef __POINTCLOUD__
  /* Point attributes */
  else if (name == DeviceStrings::u_is_point) {
    float f = (sd->type & PRIMITIVE_POINT) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_point_radius) {
    float f = point_radius(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_point_position) {
    float3 f = point_position(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_point_random) {
    float f = point_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif

  else if (name == DeviceStrings::u_normal_map_normal) {
    if (sd->type & PRIMITIVE_TRIANGLE) {
      float3 f = triangle_smooth_normal_unnormalized(kg, sd, sd->Ng, sd->prim, sd->u, sd->v);
      return set_attribute(f, type, derivatives, val);
    }
    else {
      return false;
    }
  }
  if (name == DeviceStrings::u_bump_map_normal) {
    dual3 f;
    if (!attribute_bump_map_normal(kg, sd, f)) {
      return false;
    }
    return set_attribute(f, type, derivatives, val);
  }

  return get_background_attribute(kg, sg, sd, name, type, derivatives, val);
}

ccl_device_inline bool get_camera_attribute(ccl_private ShaderGlobals *sg,
                                            KernelGlobals kg,
                                            DeviceString name,
                                            TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (name == DeviceStrings::u_sensor_size) {
    const float2 sensor = make_float2(kernel_data.cam.sensorwidth, kernel_data.cam.sensorheight);
    return set_attribute(sensor, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_image_resolution) {
    const float2 image = make_float2(kernel_data.cam.width, kernel_data.cam.height);
    return set_attribute(image, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_aperture_aspect_ratio) {
    return set_attribute(1.0f / kernel_data.cam.inv_aperture_ratio, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_aperture_size) {
    return set_attribute(kernel_data.cam.aperturesize, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_aperture_position) {
    /* The random numbers for aperture sampling are packed into N. */
    const float2 rand_lens = make_float2(sg->N.x, sg->N.y);
    const float2 pos = camera_sample_aperture(&kernel_data.cam, rand_lens);
    return set_attribute(pos * kernel_data.cam.aperturesize, type, derivatives, val);
  }
  else if (name == DeviceStrings::u_focal_distance) {
    return set_attribute(kernel_data.cam.focaldistance, type, derivatives, val);
  }
  return false;
}

ccl_device_extern bool osl_get_attribute(ccl_private ShaderGlobals *sg,
                                         const int derivatives,
                                         DeviceString object_name,
                                         DeviceString name,
                                         const int array_lookup,
                                         const int index,
                                         const TypeDesc type,
                                         ccl_private void *res)
{
  KernelGlobals kg = nullptr;
  ccl_private ShaderData *const sd = sg->sd;
  int object;

  if (sd == nullptr) {
    /* Camera shader. */
    return get_camera_attribute(sg, kg, name, type, derivatives, res);
  }

  if (object_name != DeviceStrings::_emptystring_) {
    /* TODO: Get object index from name */
    return false;
  }
  else {
    object = sd->object;
  }

  const AttributeDescriptor desc = find_attribute(kg, object, sd->prim, name);
  if (desc.offset != ATTR_STD_NOT_FOUND) {
    return get_object_attribute(kg, sd, desc, type, derivatives, res);
  }
  else {
    return get_object_standard_attribute(kg, sg, sd, name, type, derivatives, res);
  }
}

/* Renderer services */

/* The ABI for these callbacks is different, so DeviceString and TypeDesc don't work here. */
struct RSDeviceString {
  DeviceString val;
};

struct RSTypeDesc {
  unsigned char basetype;
  unsigned char aggregate;
  unsigned char vecsemantics;
  unsigned char reserved;
  int arraylen;
};

ccl_device_extern bool rend_get_userdata(
    RSDeviceString name, ccl_private void *data, int data_size, const RSTypeDesc &type, int index)
{
  if (type.basetype == 14 /* TypeDesc::PTR */) {
    kernel_assert(data_size == sizeof(void *));
    ccl_private void **ptr_data = (ccl_private void **)data;

#ifdef __KERNEL_OPTIX__
    if (name.val == DeviceStrings::u_colorsystem) {
      *ptr_data = kernel_params.osl_colorsystem;
      return true;
    }
#endif
  }
  return false;
}

ccl_device_extern bool rs_texture(ccl_private ShaderGlobals *sg,
                                  RSDeviceString filename,
                                  ccl_private void *texture_handle,
                                  ccl_private void *texture_thread_info,
                                  ccl_private OSLTextureOptions *opt,
                                  const float s,
                                  const float t,
                                  const float dsdx,
                                  const float dtdx,
                                  const float dsdy,
                                  const float dtdy,
                                  const int nchannels,
                                  ccl_private float *result,
                                  ccl_private float *dresultds,
                                  ccl_private float *dresultdt,
                                  ccl_private void *errormessage)
{
  const unsigned int type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const unsigned int slot = OSL_TEXTURE_HANDLE_SLOT(texture_handle);

  switch (type) {
    case OSL_TEXTURE_HANDLE_TYPE_SVM: {
      const float4 rgba = kernel_tex_image_interp(nullptr, slot, s, 1.0f - t);
      if (nchannels > 0) {
        result[0] = rgba.x;
      }
      if (nchannels > 1) {
        result[1] = rgba.y;
      }
      if (nchannels > 2) {
        result[2] = rgba.z;
      }
      if (nchannels > 3) {
        result[3] = rgba.w;
      }
      return true;
    }
    case OSL_TEXTURE_HANDLE_TYPE_IES: {
      if (nchannels > 0) {
        result[0] = kernel_ies_interp(nullptr, slot, s, t);
      }
      return true;
    }
    default: {
      return false;
    }
  }
}

ccl_device_extern bool rs_texture3d(ccl_private ShaderGlobals *sg,
                                    RSDeviceString filename,
                                    ccl_private void *texture_handle,
                                    ccl_private void *texture_thread_info,
                                    ccl_private OSLTextureOptions *opt,
                                    const ccl_private float3 *P,
                                    const ccl_private float3 *dPdx,
                                    const ccl_private float3 *dPdy,
                                    const ccl_private float3 *dPdz,
                                    const int nchannels,
                                    ccl_private float *result,
                                    ccl_private float *dresultds,
                                    ccl_private float *dresultdt,
                                    ccl_private float *dresultdr,
                                    ccl_private void *errormessage)
{
  const unsigned int type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const unsigned int slot = OSL_TEXTURE_HANDLE_SLOT(texture_handle);

  switch (type) {
    case OSL_TEXTURE_HANDLE_TYPE_SVM: {
      const float4 rgba = kernel_tex_image_interp_3d(nullptr, slot, *P, INTERPOLATION_NONE, -1.0f);
      if (nchannels > 0) {
        result[0] = rgba.x;
      }
      if (nchannels > 1) {
        result[1] = rgba.y;
      }
      if (nchannels > 2) {
        result[2] = rgba.z;
      }
      if (nchannels > 3) {
        result[3] = rgba.w;
      }
      return true;
    }
    default: {
      return false;
    }
  }
}

ccl_device_extern bool rs_environment(ccl_private ShaderGlobals *sg,
                                      RSDeviceString filename,
                                      ccl_private void *texture_handle,
                                      ccl_private void *texture_thread_info,
                                      ccl_private OSLTextureOptions *opt,
                                      const ccl_private float3 *R,
                                      const ccl_private float3 *dRdx,
                                      const ccl_private float3 *dRdy,
                                      const int nchannels,
                                      ccl_private float *result,
                                      ccl_private float *dresultds,
                                      ccl_private float *dresultdt,
                                      ccl_private void *errormessage)
{
  if (nchannels > 0) {
    result[0] = 1.0f;
  }
  if (nchannels > 1) {
    result[1] = 0.0f;
  }
  if (nchannels > 2) {
    result[2] = 1.0f;
  }
  if (nchannels > 3) {
    result[3] = 1.0f;
  }

  return false;
}

ccl_device_extern bool rs_get_texture_info(ccl_private ShaderGlobals *sg,
                                           RSDeviceString filename,
                                           ccl_private void *texture_handle,
                                           ccl_private void *texture_thread_info,
                                           int subimage,
                                           RSDeviceString dataname,
                                           RSTypeDesc datatype,
                                           ccl_private void *data,
                                           ccl_private void *errormessage)
{
  return 0;
}

ccl_device_extern bool rs_get_texture_info_st(ccl_private ShaderGlobals *sg,
                                              RSDeviceString filename,
                                              ccl_private void *texture_handle,
                                              const float s,
                                              const float t,
                                              ccl_private void *texture_thread_info,
                                              int subimage,
                                              RSDeviceString dataname,
                                              RSTypeDesc datatype,
                                              ccl_private void *data,
                                              ccl_private void *errormessage)
{
  return 0;
}

ccl_device_extern int rs_pointcloud_search(ccl_private ShaderGlobals *sg,
                                           RSDeviceString filename,
                                           const ccl_private float3 *center,
                                           float radius,
                                           int max_points,
                                           bool sort,
                                           ccl_private int *out_indices,
                                           ccl_private float *out_distances,
                                           int derivs_offset)
{
  return 0;
}

ccl_device_extern int rs_pointcloud_get(ccl_private ShaderGlobals *sg,
                                        RSDeviceString filename,
                                        const ccl_private int *indices,
                                        int count,
                                        RSDeviceString attr_name,
                                        RSTypeDesc attr_type,
                                        ccl_private void *out_data)
{
  return 0;
}

ccl_device_extern bool rs_pointcloud_write(ccl_private ShaderGlobals *sg,
                                           RSDeviceString filename,
                                           const ccl_private float3 *pos,
                                           int nattribs,
                                           const ccl_private DeviceString *names,
                                           const ccl_private RSTypeDesc *types,
                                           const ccl_private void **data)
{
  return false;
}

ccl_device_extern bool rs_trace(ccl_private ShaderGlobals *sg,
                                ccl_private void *options,
                                const ccl_private float3 *P,
                                const ccl_private float3 *dPdx,
                                const ccl_private float3 *dPdy,
                                const ccl_private float3 *R,
                                const ccl_private float3 *dRdx,
                                const ccl_private float3 *dRdy)
{
  return false;
}

ccl_device_extern bool rs_trace_get(ccl_private ShaderGlobals *sg,
                                    RSDeviceString name,
                                    RSTypeDesc type,
                                    ccl_private void *data,
                                    bool derivatives)
{
  return false;
}
