/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 * SPDX-FileCopyrightText: Contributors to the OpenImageIO project
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Shared functions between OSL on CPU and GPU. */

#pragma once

#include "kernel/camera/camera.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/curve.h"
#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/object.h"
#include "kernel/geom/point.h"
#include "kernel/geom/primitive.h"
#include "kernel/geom/triangle.h"

#include "kernel/image.h"
#include "kernel/osl/strings.h"
#include "kernel/util/differential.h"
#include "kernel/util/ies.h"
#include "kernel/util/image_2d.h"
#include "kernel/util/image_3d.h"

#include "util/hash.h"

#ifndef __KERNEL_GPU__
#  include "kernel/svm/ao.h"
#  include "kernel/svm/bevel.h"
#endif

CCL_NAMESPACE_BEGIN

/* For GPU duplicate part of OpenImageIO TypeDesc as we don't use the headers at compile time. */
#ifdef __KERNEL_GPU__
struct TypeDesc {
  enum BASETYPE {
    UNKNOWN = 0,
    NONE = 1,
    UCHAR = 2,
    CHAR = 3,
    USHORT = 4,
    SHORT = 5,
    UINT = 6,
    INT = 7,
    ULONGLONG = 8,
    LONGLONG = 9,
    HALF = 10,
    FLOAT = 11,
    DOUBLE = 12,
    STRING = 13,
    PTR = 14,
    LAST = 15,
  };

  enum AGGREGATE {
    SCALAR = 1,
    VEC2 = 2,
    VEC3 = 3,
    VEC4 = 4,
    MATRIX33 = 15,
    MATRIX44 = 16,
  };

  enum VECSEMANTICS {
    NOSEMANTICS = 0,
    COLOR = 1,
    POINT = 2,
    VECTOR = 3,
    NORMAL = 4,
    TIMECODE = 5,
  };

  unsigned char basetype;
  unsigned char aggregate;
  unsigned char vecsemantics;
  unsigned char reserved;
  int arraylen;

  ccl_device_inline_method constexpr TypeDesc(BASETYPE b = UNKNOWN,
                                              AGGREGATE a = SCALAR,
                                              VECSEMANTICS v = NOSEMANTICS,
                                              int l = 0)
      : basetype(b), aggregate(a), vecsemantics(v), reserved(0), arraylen(l)
  {
  }

  ccl_device_inline_method bool operator==(const ccl_private TypeDesc &other) const
  {
    return basetype == other.basetype && aggregate == other.aggregate &&
           vecsemantics == other.vecsemantics && arraylen == other.arraylen;
  }

  ccl_device_inline_method bool operator!=(const ccl_private TypeDesc &other) const
  {
    return !(*this == other);
  }
};

ccl_device_constant TypeDesc TypeFloat(TypeDesc::FLOAT, TypeDesc::SCALAR);
ccl_device_constant TypeDesc TypeInt(TypeDesc::INT, TypeDesc::SCALAR);
ccl_device_constant TypeDesc TypeString(TypeDesc::STRING, TypeDesc::SCALAR);
ccl_device_constant TypeDesc TypeMatrix(TypeDesc::FLOAT, TypeDesc::MATRIX44);
#endif

/* Type Checking, recognizing both arrays and vectors. */

ccl_device_inline bool is_type_float2(const TypeDesc t)
{
  return (t.basetype == TypeDesc::FLOAT && t.aggregate == TypeDesc::VEC2 && t.arraylen == 0) ||
         t == TypeDesc(TypeDesc::FLOAT, TypeDesc::SCALAR, TypeDesc::NOSEMANTICS, 2);
}

ccl_device_inline bool is_type_float3(const TypeDesc t)
{
  return (t.basetype == TypeDesc::FLOAT && t.aggregate == TypeDesc::VEC3 && t.arraylen == 0) ||
         t == TypeDesc(TypeDesc::FLOAT, TypeDesc::SCALAR, TypeDesc::NOSEMANTICS, 3);
}

ccl_device_inline bool is_type_float4(const TypeDesc t)
{
  return (t.basetype == TypeDesc::FLOAT && t.aggregate == TypeDesc::VEC4 && t.arraylen == 0) ||
         t == TypeDesc(TypeDesc::FLOAT, TypeDesc::SCALAR, TypeDesc::NOSEMANTICS, 4);
}

ccl_device_inline bool is_type_int2(const TypeDesc t)
{
  return (t.basetype == TypeDesc::INT && t.aggregate == TypeDesc::VEC2 && t.arraylen == 0) ||
         t == TypeDesc(TypeDesc::INT, TypeDesc::SCALAR, TypeDesc::NOSEMANTICS, 2);
}

ccl_device_inline bool is_type_int3(const TypeDesc t)
{
  return (t.basetype == TypeDesc::INT && t.aggregate == TypeDesc::VEC3 && t.arraylen == 0) ||
         t == TypeDesc(TypeDesc::INT, TypeDesc::SCALAR, TypeDesc::NOSEMANTICS, 3);
}

/* Attribute Utilities */

template<typename T>
ccl_device_inline bool set_attribute(const dual<T> v,
                                     const TypeDesc type,
                                     bool derivatives,
                                     ccl_private void *val);

ccl_device_inline void set_data_float(const dual1 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  fval[0] = data.val;
  if (derivatives) {
    fval[1] = data.dx;
    fval[2] = data.dy;
  }
}

ccl_device_inline void set_data_float3(const dual3 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  copy_v3_v3(fval, data.val);
  if (derivatives) {
    copy_v3_v3(fval + 3, data.dx);
    copy_v3_v3(fval + 6, data.dy);
  }
}

ccl_device_inline void set_data_float4(const dual4 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  copy_v4_v4(fval, data.val);
  if (derivatives) {
    copy_v4_v4(fval + 4, data.dx);
    copy_v4_v4(fval + 8, data.dy);
  }
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

/* Matrix */

ccl_device_inline bool osl_shared_get_object_matrix(KernelGlobals kg,
                                                    ccl_private const ShaderData *sd,
                                                    ccl_private float *res)
{
  const int object = sd->object;
  if (object != OBJECT_NONE) {
    const Transform tfm = object_get_transform(kg, sd);
    copy_matrix(res, tfm);
    return true;
  }
  return false;
}

ccl_device_inline bool osl_shared_get_object_matrix_motion(KernelGlobals kg,
                                                           ccl_private const ShaderData *sd,
                                                           ccl_private float *res,
                                                           float time)
{
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
    copy_matrix(res, tfm);
    return true;
  }
  return false;
}

ccl_device_inline bool osl_shared_get_object_inverse_matrix(KernelGlobals kg,
                                                            ccl_private const ShaderData *sd,
                                                            ccl_private float *res)
{
  const int object = sd->object;
  if (object != OBJECT_NONE) {
    const Transform tfm = object_get_inverse_transform(kg, sd);
    copy_matrix(res, tfm);
    return true;
  }
  return false;
}

ccl_device_inline bool osl_shared_get_object_inverse_matrix_motion(
    KernelGlobals kg, ccl_private const ShaderData *sd, ccl_private float *res, float time)
{
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
    copy_matrix(res, itfm);
    return true;
  }
  return false;
}

ccl_device_inline bool osl_shared_get_named_matrix(KernelGlobals kg,
                                                   DeviceString from,
                                                   ccl_private float *res)
{
  if (from == DeviceStrings::u_ndc) {
    copy_matrix(res, kernel_data.cam.ndctoworld);
    return true;
  }
  if (from == DeviceStrings::u_raster) {
    copy_matrix(res, kernel_data.cam.rastertoworld);
    return true;
  }
  if (from == DeviceStrings::u_screen) {
    copy_matrix(res, kernel_data.cam.screentoworld);
    return true;
  }
  if (from == DeviceStrings::u_camera) {
    copy_matrix(res, kernel_data.cam.cameratoworld);
    return true;
  }
  if (from == DeviceStrings::u_world) {
    copy_matrix(res, projection_identity());
    return true;
  }
  return false;
}

ccl_device_inline bool osl_shared_get_named_inverse_matrix(KernelGlobals kg,
                                                           DeviceString to,
                                                           ccl_private float *res)
{
  if (to == DeviceStrings::u_ndc) {
    copy_matrix(res, kernel_data.cam.worldtondc);
    return true;
  }
  if (to == DeviceStrings::u_raster) {
    copy_matrix(res, kernel_data.cam.worldtoraster);
    return true;
  }
  if (to == DeviceStrings::u_screen) {
    copy_matrix(res, kernel_data.cam.worldtoscreen);
    return true;
  }
  if (to == DeviceStrings::u_camera) {
    copy_matrix(res, kernel_data.cam.worldtocamera);
    return true;
  }
  if (to == DeviceStrings::u_world) {
    copy_matrix(res, projection_identity());
    return true;
  }
  return false;
}

/* Attribute Setting */

ccl_device_template_spec bool set_attribute(const dual1 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (is_type_float4(type)) {
    set_data_float4(make_float4(make_float3(v)), derivatives, val);
    return true;
  }
  if (is_type_float3(type)) {
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
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (is_type_float4(type)) {
    set_data_float4(make_float4(make_float3(v)), derivatives, val);
    return true;
  }
  if (is_type_float3(type)) {
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
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (is_type_float4(type)) {
    set_data_float4(make_float4(v), derivatives, val);
    return true;
  }
  if (is_type_float3(type)) {
    set_data_float3(v, derivatives, val);
    return true;
  }
  if (type == TypeFloat) {
    set_data_float(average(v), derivatives, val);
    return true;
  }

  return false;
}

ccl_device_template_spec bool set_attribute(const dual4 v,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (is_type_float4(type)) {
    set_data_float4(v, derivatives, val);
    return true;
  }
  if (is_type_float3(type)) {
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
ccl_device_inline bool set_attribute(const T f,
                                     const TypeDesc type,
                                     bool derivatives,
                                     ccl_private void *val)
{
  return set_attribute(dual<T>(f), type, derivatives, val);
}

ccl_device_template_spec bool set_attribute(const int i,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (type == TypeInt) {
    ccl_private int *ival = static_cast<ccl_private int *>(val);
    ival[0] = i;

    if (derivatives) {
      ival[1] = 0;
      ival[2] = 0;
    }

    return true;
  }

  return false;
}

#ifndef __KERNEL_GPU__
ccl_device_template_spec bool set_attribute(ustring str,
                                            const TypeDesc type,
                                            bool derivatives,
                                            ccl_private void *val)
{
  if (type == TypeString) {
    OSLUStringHash *sval = static_cast<OSLUStringHash *>(val);
    sval[0] = str;

    if (derivatives) {
      sval[1] = OSLUStringHash();
      sval[2] = OSLUStringHash();
    }

    return true;
  }

  return false;
}
#endif

ccl_device_inline bool set_attribute_matrix(const Transform &tfm,
                                            const TypeDesc type,
                                            ccl_private void *val)
{
  if (type == TypeMatrix) {
    copy_matrix(static_cast<ccl_private float *>(val), tfm);
    return true;
  }

  return false;
}

ccl_device_inline bool set_attribute_float3_3(const float3 P[3],
                                              TypeDesc type,
                                              bool derivatives,
                                              ccl_private void *val)
{
  if (type.vecsemantics == TypeDesc::POINT && type.arraylen >= 3) {
    ccl_private float *fval = static_cast<ccl_private float *>(val);

    copy_v3_v3(fval, P[0]);
    copy_v3_v3(fval + 3, P[1]);
    copy_v3_v3(fval + 6, P[2]);

    if (type.arraylen > 3) {
      for (int i = 3 * 3; i < type.arraylen * 3; i++) {
        fval[i] = 0.0f;
      }
    }
    if (derivatives) {
      for (int i = type.arraylen * 3; i < type.arraylen * 3 * 3; i++) {
        fval[i] = 0.0f;
      }
    }

    return true;
  }

  return false;
}

ccl_device bool attribute_bump_map_normal(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          ccl_private dual3 &f)
{
  if (!(sd->type & PRIMITIVE_TRIANGLE) || !(sd->shader & SHADER_SMOOTH_NORMAL)) {
    /* TODO: implement for curve. */
    return false;
  }

  const bool backfacing = (sd->flag & SD_BACKFACING);

  /* Fallback when the smooth normal is zero. */
  float3 Ng = backfacing ? -sd->Ng : sd->Ng;
  object_inverse_normal_transform(kg, sd, &Ng);

  if (sd->type == PRIMITIVE_TRIANGLE) {
    f.val = triangle_smooth_normal(
        kg, Ng, sd->object, sd->object_flag, sd->prim, sd->u, sd->v, sd->du, sd->dv, f.dx, f.dy);
  }
  else {
    assert(sd->type & PRIMITIVE_MOTION_TRIANGLE);
    f.val = motion_triangle_smooth_normal(
        kg, Ng, sd->object, sd->prim, sd->time, sd->u, sd->v, sd->du, sd->dv, f.dx, f.dy);
  }

  if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
    /* Transform to local space. */
    object_inverse_normal_transform(kg, sd, &f.val);
    object_inverse_normal_transform(kg, sd, &f.dx);
    object_inverse_normal_transform(kg, sd, &f.dy);
  }

  if (backfacing) {
    f = -f;
  }

  f.dx -= f.val;
  f.dy -= f.val;

  return true;
}

/* Textures */

ccl_device_forceinline void rgba_to_nchannels(const float4 rgba,
                                              const int nchannels,
                                              ccl_private float *result)
{
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
}

ccl_device bool osl_shared_get_texture_info(KernelGlobals kg,
                                            ccl_private void *texture_handle,
                                            const float2 uv,
                                            const bool use_uv,
                                            DeviceString dataname,
                                            const TypeDesc datatype,
                                            void *data)
{
  const OSLTextureHandleType texture_type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const int image_texture_or_udim_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  if (texture_type != OSLTextureHandleType::IMAGE) {
    return false;
  }

  int image_texture_id = image_texture_or_udim_id;
  if (use_uv) {
    float2 local_uv = uv;
    image_texture_id = kernel_image_udim_map(kg, image_texture_or_udim_id, local_uv);
  }
  if (image_texture_id == KERNEL_IMAGE_NONE) {
    return false;
  }

  const ccl_global KernelImageTexture &tex = kernel_data_fetch(image_textures, image_texture_id);
  if (tex.image_info_id == KERNEL_IMAGE_NONE) {
    return false;
  }

  if (dataname == DeviceStrings::u_resolution) {
    if (is_type_int2(datatype)) {
      int *res = static_cast<int *>(data);
      res[0] = int(tex.width);
      res[1] = int(tex.height);
      return true;
    }
    if (is_type_float2(datatype)) {
      float *res = static_cast<float *>(data);
      res[0] = float(tex.width);
      res[1] = float(tex.height);
      return true;
    }
    if (is_type_int3(datatype)) {
      int *res = static_cast<int *>(data);
      res[0] = int(tex.width);
      res[1] = int(tex.height);
      res[2] = 1;
      return true;
    }
    if (is_type_float3(datatype)) {
      float *res = static_cast<float *>(data);
      res[0] = float(tex.width);
      res[1] = float(tex.height);
      res[2] = 1.0f;
      return true;
    }
  }
  else if (dataname == DeviceStrings::u_channels) {
    if (datatype == TypeInt) {
      const int image_info_id = tex.image_info_id;
      const ccl_global KernelImageInfo &info = kernel_data_fetch(image_info, image_info_id);
      int channels = 4;
      switch (info.data_type) {
        case IMAGE_DATA_TYPE_FLOAT4:
        case IMAGE_DATA_TYPE_BYTE4:
        case IMAGE_DATA_TYPE_HALF4:
        case IMAGE_DATA_TYPE_USHORT4:
        case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
          channels = 4;
          break;
        case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
          channels = 3;
          break;
        case IMAGE_DATA_TYPE_FLOAT:
        case IMAGE_DATA_TYPE_BYTE:
        case IMAGE_DATA_TYPE_HALF:
        case IMAGE_DATA_TYPE_USHORT:
        case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
        case IMAGE_DATA_TYPE_NANOVDB_FPN:
        case IMAGE_DATA_TYPE_NANOVDB_FP16:
          channels = 1;
          break;
        case IMAGE_DATA_TYPE_NANOVDB_EMPTY:
          channels = 0;
          break;
      }
      *static_cast<int *>(data) = channels;
      return true;
    }
  }
  else if (dataname == DeviceStrings::u_exists) {
    if (datatype == TypeInt) {
      *static_cast<int *>(data) = 1;
      return true;
    }
  }

  return false;
}

ccl_device bool osl_shared_texture(KernelGlobals kg,
                                   ccl_private ShaderGlobals *sg,
                                   ccl_private void *texture_handle,
                                   ccl_private void *opt_void,
                                   float s,
                                   float t,
                                   float dsdx,
                                   float dtdx,
                                   float dsdy,
                                   float dtdy,
                                   int nchannels,
                                   float *result)
{
  const OSLTextureHandleType type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const int image_texture_or_udim_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  ccl_private ShaderData *sd = sg->sd;
  bool status = false;

  switch (type) {
    case OSLTextureHandleType::IMAGE: {
      const dual2 uv({s, t}, {dsdx, dtdx}, {dsdy, dtdy});
      const float4 rgba = kernel_image_interp_with_udim(kg, sd, image_texture_or_udim_id, uv);
      rgba_to_nchannels(rgba, nchannels, result);

      status = true;
      break;
    }
    case OSLTextureHandleType::IES: {
      if (nchannels > 0) {
        result[0] = kernel_ies_interp(kg, image_texture_or_udim_id, s, t);
      }
      status = true;
      break;
    }
    case OSLTextureHandleType::BEVEL: {
#ifndef __KERNEL_GPU__
      /* Bevel shader hack. */
      ConstIntegratorState state = sg->path_state;
      if (nchannels >= 3 && state != nullptr) {
        const int num_samples = int(s);
        const float radius = t;
        const float3 N = svm_bevel(kg, state, sd, radius, num_samples);
        result[0] = N.x;
        result[1] = N.y;
        result[2] = N.z;
        status = true;
      }
#endif
      break;
    }
    case OSLTextureHandleType::AO: {
#ifndef __KERNEL_GPU__
      /* AO shader hack. */
      ConstIntegratorState state = sg->path_state;
      const OSL::TextureOpt *options = static_cast<const OSL::TextureOpt *>(opt_void);
      if (state != nullptr) {
        const int num_samples = int(s);
        const float radius = t;
        const float3 N = make_float3(dsdx, dtdx, dsdy);
        int flags = 0;
        if (int(dtdy)) {
          flags |= NODE_AO_INSIDE;
        }
        if (int(options->sblur)) {
          flags |= NODE_AO_ONLY_LOCAL;
        }
        if (int(options->tblur)) {
          flags |= NODE_AO_GLOBAL_RADIUS;
        }
        result[0] = svm_ao(kg, state, sd, N, radius, num_samples, flags);
        status = true;
      }
#endif
      break;
    }
  }

  if (!status) {
    rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
  }

  return status;
}

ccl_device bool osl_shared_texture3d(KernelGlobals kg,
                                     ccl_private ShaderGlobals *sg,
                                     ccl_private void *texture_handle,
                                     float3 P,
                                     float3 /*dPdx*/,
                                     float3 /*dPdy*/,
                                     float3 /*dPdz*/,
                                     int nchannels,
                                     float *result)
{
  const OSLTextureHandleType type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const int image_texture_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  bool status = false;

  switch (type) {
    case OSLTextureHandleType::IMAGE: {
      const float4 rgba = kernel_image_interp_3d(
          kg, sg->sd, image_texture_id, P, INTERPOLATION_NONE, false);

      rgba_to_nchannels(rgba, nchannels, result);
      status = true;
      break;
    }
    default:
      break;
  }

  if (!status) {
    rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
  }

  return status;
}

ccl_device bool osl_shared_environment(KernelGlobals kg,
                                       ccl_private ShaderGlobals *sg,
                                       ccl_private void *texture_handle,
                                       float3 R,
                                       float3 dRdx,
                                       float3 dRdy,
                                       int nchannels,
                                       float *result)
{
  const OSLTextureHandleType type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const int image_texture_or_udim_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  if (type == OSLTextureHandleType::IMAGE) {
    ccl_private ShaderData *sd = sg->sd;
    const dual3 R_dual(R, dRdx, dRdy);
    /* Environment call is always equirectangular. */
    const dual2 uv(direction_to_equirectangular(R_dual.val));
    const float4 rgba = kernel_image_interp_with_udim(kg, sd, image_texture_or_udim_id, uv);
    rgba_to_nchannels(rgba, nchannels, result);
    return true;
  }

  rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);

  return false;
}

/* Object Attribute Retrieval */

template<typename T>
ccl_device_inline bool osl_shared_get_object_attribute_impl(KernelGlobals kg,
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

ccl_device_inline bool osl_shared_get_object_attribute(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       const AttributeDescriptor &desc,
                                                       const TypeDesc type,
                                                       bool derivatives,
                                                       ccl_private void *val)
{
  if (desc.type == NODE_ATTR_FLOAT) {
    return osl_shared_get_object_attribute_impl<float>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT2) {
    return osl_shared_get_object_attribute_impl<float2>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT3) {
    return osl_shared_get_object_attribute_impl<float3>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    return osl_shared_get_object_attribute_impl<float4>(kg, sd, desc, type, derivatives, val);
  }
  if (desc.type == NODE_ATTR_MATRIX) {
    const Transform tfm = primitive_attribute_matrix(kg, desc);
    return set_attribute_matrix(tfm, type, val);
  }
  return false;
}

/* Background Attributes */

ccl_device_inline bool osl_shared_get_background_attribute(KernelGlobals kg,
                                                           ccl_private ShaderGlobals *sg,
                                                           ccl_private ShaderData *sd,
                                                           DeviceString name,
                                                           const TypeDesc type,
                                                           bool derivatives,
                                                           ccl_private void *val)
{
#ifdef __KERNEL_GPU__
  ConstIntegratorState state = (sg->shade_index > 0) ? (sg->shade_index - 1) : -1;
  ConstIntegratorShadowState shadow_state = (sg->shade_index < 0) ? (-sg->shade_index - 1) : -1;
#  define READ_PATH_STATE(elem) \
    ((state != -1)        ? INTEGRATOR_STATE(state, path, elem) : \
     (shadow_state != -1) ? INTEGRATOR_STATE(shadow_state, shadow_path, elem) : \
                            0)
#else
  const IntegratorStateCPU *state = sg->path_state;
  const IntegratorShadowStateCPU *shadow_state = sg->shadow_path_state;
#  define READ_PATH_STATE(elem) \
    ((state != nullptr)        ? state->path.elem : \
     (shadow_state != nullptr) ? shadow_state->shadow_path.elem : \
                                 0)
#endif

  if (name == DeviceStrings::u_path_ray_length) {
    /* Ray Length */
    const float f = sd->ray_length;
    return set_attribute(f, type, derivatives, val);
  }
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

  if (name == DeviceStrings::u_ndc) {
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

/* Object Standard Attributes */

ccl_device_inline bool osl_shared_get_object_standard_attribute(KernelGlobals kg,
                                                                ccl_private ShaderGlobals *sg,
                                                                ccl_private ShaderData *sd,
                                                                DeviceString name,
                                                                const TypeDesc type,
                                                                bool derivatives,
                                                                ccl_private void *val)
{
  /* Object Attributes */
  if (name == DeviceStrings::u_object_location) {
    const float3 f = object_location(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_object_color) {
    const float3 f = object_color(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_object_alpha) {
    const float f = object_alpha(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_object_index) {
    const float f = object_pass_id(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_object_is_light) {
    const float f = (sd->type & PRIMITIVE_LAMP) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_geom_dupli_generated) {
    const float3 f = object_dupli_generated(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_geom_dupli_uv) {
    const float3 f = object_dupli_uv(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_material_index) {
    const float f = shader_pass_id(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_object_random) {
    const float f = object_random_number(kg, sd->object);
    return set_attribute(f, type, derivatives, val);
  }

  /* Particle Attributes */
  if (name == DeviceStrings::u_particle_index) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_index(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_random) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = hash_uint2_to_float(particle_index(kg, particle_id), 0);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_age) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_age(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_lifetime) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_lifetime(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_location) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_location(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
#if 0 /* unsupported */
  if (name == DeviceStrings::u_particle_rotation) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float4 f = particle_rotation(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
#endif
  if (name == DeviceStrings::u_particle_size) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float f = particle_size(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_velocity) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_particle_angular_velocity) {
    const int particle_id = object_particle_id(kg, sd->object);
    const float3 f = particle_angular_velocity(kg, particle_id);
    return set_attribute(f, type, derivatives, val);
  }

  /* Geometry Attributes */
  if (name == DeviceStrings::u_geom_numpolyvertices) {
    return set_attribute(3, type, derivatives, val);
  }
  if ((name == DeviceStrings::u_geom_trianglevertices ||
       name == DeviceStrings::u_geom_polyvertices) &&
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
#ifndef __KERNEL_GPU__
  if (name == DeviceStrings::u_geom_name) {
    const ustring object_name = kg->osl.globals->object_names[sd->object];
    return set_attribute(object_name, type, derivatives, val);
  }
#endif
  if (name == DeviceStrings::u_is_smooth) {
    const float f = ((sd->shader & SHADER_SMOOTH_NORMAL) != 0);
    return set_attribute(f, type, derivatives, val);
  }
#ifdef __HAIR__
  /* Hair Attributes */
  if (name == DeviceStrings::u_is_curve) {
    const float f = (sd->type & PRIMITIVE_CURVE) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_curve_thickness) {
    const float f = curve_thickness(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_curve_tangent_normal) {
    const float3 f = curve_tangent_normal(sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_curve_random) {
    const float f = curve_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif
#ifdef __POINTCLOUD__
  /* Point Attributes */
  if (name == DeviceStrings::u_is_point) {
    const float f = (sd->type & PRIMITIVE_POINT) != 0;
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_point_radius) {
    const float f = point_radius(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_point_position) {
    const float3 f = point_position(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
  if (name == DeviceStrings::u_point_random) {
    const float f = point_random(kg, sd);
    return set_attribute(f, type, derivatives, val);
  }
#endif
  if (name == DeviceStrings::u_normal_map_normal) {
    if (sd->type & PRIMITIVE_TRIANGLE) {
      const AttributeDescriptor desc = find_attribute(
          kg, sd->object, sd->prim, ATTR_STD_NORMAL_UNDISPLACED);
      if (desc.offset != ATTR_STD_NOT_FOUND) {
        return osl_shared_get_object_attribute(kg, sd, desc, type, derivatives, val);
      }
      const float3 f = triangle_smooth_normal_unnormalized_object_space(kg, sd);
      return set_attribute(f, type, derivatives, val);
    }
    return false;
  }
  if (name == DeviceStrings::u_bump_map_normal) {
    dual3 f;
    if (!attribute_bump_map_normal(kg, sd, f)) {
      return false;
    }
    return set_attribute(f, type, derivatives, val);
  }
  return osl_shared_get_background_attribute(kg, sg, sd, name, type, derivatives, val);
}

/* Camera Attributes */

ccl_device_inline bool osl_shared_get_camera_attribute(KernelGlobals kg,
                                                       ccl_private ShaderGlobals *sg,
                                                       DeviceString name,
                                                       const TypeDesc type,
                                                       bool derivatives,
                                                       ccl_private void *val)
{
  if (name == DeviceStrings::u_sensor_size) {
    const float2 sensor = make_float2(kernel_data.cam.sensorwidth, kernel_data.cam.sensorheight);
    return set_attribute(sensor, type, derivatives, val);
  }
  if (name == DeviceStrings::u_image_resolution) {
    const float2 image = make_float2(kernel_data.cam.width, kernel_data.cam.height);
    return set_attribute(image, type, derivatives, val);
  }
  if (name == DeviceStrings::u_aperture_aspect_ratio) {
    return set_attribute(1.0f / kernel_data.cam.inv_aperture_ratio, type, derivatives, val);
  }
  if (name == DeviceStrings::u_aperture_size) {
    return set_attribute(kernel_data.cam.aperturesize, type, derivatives, val);
  }
  if (name == DeviceStrings::u_aperture_position) {
    /* The random numbers for aperture sampling are packed into N. */
    const float2 rand_lens = make_float2(sg->N.x, sg->N.y);
    const float2 pos = camera_sample_aperture(&kernel_data.cam, rand_lens);
    return set_attribute(pos * kernel_data.cam.aperturesize, type, derivatives, val);
  }
  if (name == DeviceStrings::u_focal_distance) {
    return set_attribute(kernel_data.cam.focaldistance, type, derivatives, val);
  }
  return false;
}

CCL_NAMESPACE_END
