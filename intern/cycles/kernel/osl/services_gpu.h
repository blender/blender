/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#include "kernel/geom/attribute.h"

#include "kernel/util/ies.h"
#include "kernel/util/image_3d.h"

#include "kernel/osl/services_shared.h"
#include "kernel/osl/strings.h"

#include "util/types_image.h"

#ifndef __KERNEL_GPU__
CCL_NAMESPACE_BEGIN
#endif

/* Closure */

ccl_device_extern ccl_private OSLClosure *osl_mul_closure_color(ccl_private ShaderGlobals *sg,
                                                                ccl_private OSLClosure *a,
                                                                const ccl_private float3 *weight)
{
  if (*weight == zero_float3() || !a) {
    return nullptr;
  }
  if (*weight == one_float3()) {
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
  if (weight == 1.0f) {
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

ccl_device_extern void osl_error(ccl_private ShaderGlobals * /*sg*/,
                                 DeviceString /*format*/,
                                 void * /*args*/)
{
}

ccl_device_extern void osl_printf(ccl_private ShaderGlobals * /*sg*/,
                                  DeviceString /*format*/,
                                  void * /*args*/)
{
}

ccl_device_extern void osl_warning(ccl_private ShaderGlobals * /*sg*/,
                                   DeviceString /*format*/,
                                   void * /*args*/)
{
}

ccl_device_extern void osl_fprintf(ccl_private ShaderGlobals * /*sg*/,
                                   DeviceString /*filename*/,
                                   DeviceString /*format*/,
                                   void * /*args*/)
{
}

ccl_device_extern uint osl_range_check_err(const int indexvalue,
                                           const int length,
                                           DeviceString /*symname*/,
                                           ccl_private ShaderGlobals * /*sg*/,
                                           DeviceString /*sourcefile*/,
                                           const int /*sourceline*/,
                                           DeviceString /*groupname*/,
                                           const int /*layer*/,
                                           DeviceString /*layername*/,
                                           DeviceString /*shadername*/)
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

ccl_device_extern bool osl_get_matrix(ccl_private ShaderGlobals *sg,
                                      ccl_private float *res,
                                      DeviceString from)
{
  KernelGlobals kg = nullptr;

  if (from == DeviceStrings::u_common) {
    copy_matrix(res, projection_identity());
    return true;
  }
  if (from == DeviceStrings::u_shader || from == DeviceStrings::u_object) {
    return osl_shared_get_object_matrix(kg, sg->sd, res);
  }
  return osl_shared_get_named_matrix(kg, from, res);
}

ccl_device_extern bool osl_get_inverse_matrix(ccl_private ShaderGlobals *sg,
                                              ccl_private float *res,
                                              DeviceString to)
{
  KernelGlobals kg = nullptr;

  if (to == DeviceStrings::u_common) {
    copy_matrix(res, projection_identity());
    return true;
  }
  if (to == DeviceStrings::u_shader || to == DeviceStrings::u_object) {
    return osl_shared_get_object_inverse_matrix(kg, sg->sd, res);
  }
  return osl_shared_get_named_inverse_matrix(kg, to, res);
}

/* The ABI for these callbacks is different, so DeviceString and TypeDesc don't work here. */
using RSTypeDesc = long long;

struct RSDeviceString {
  DeviceString val;
};

/* Attributes */

ccl_device_extern bool osl_get_attribute(ccl_private ShaderGlobals *sg,
                                         const int derivatives,
                                         DeviceString object_name,
                                         DeviceString name,
                                         const int /*array_lookup*/,
                                         const int /*index*/,
                                         const RSTypeDesc type_abi,
                                         ccl_private void *res)
{
  const TypeDesc type = *reinterpret_cast<const TypeDesc *>(&type_abi);
  KernelGlobals kg = nullptr;
  ccl_private ShaderData *const sd = sg->sd;

  if (sd == nullptr) {
    /* Camera shader. */
    return osl_shared_get_camera_attribute(kg, sg, name, type, derivatives, res);
  }

  if (object_name != DeviceStrings::u_empty) {
    /* TODO: Get object index from name */
    return false;
  }

  const int object = sd->object;

  const AttributeDescriptor desc = find_attribute(kg, object, sd->prim, name);
  if (desc.offset != ATTR_STD_NOT_FOUND) {
    return osl_shared_get_object_attribute(kg, sd, desc, type, derivatives, res);
  }
  return osl_shared_get_object_standard_attribute(kg, sg, sd, name, type, derivatives, res);
}

ccl_device_extern bool rend_get_userdata(RSDeviceString name,
                                         ccl_private void *data,
                                         int data_size,
                                         const TypeDesc &type,
                                         int /*index*/)
{
  if (type.basetype == TypeDesc::PTR) {
    kernel_assert(data_size == sizeof(void *));
    ccl_private void **ptr_data = (ccl_private void **)data;

    if (name.val == DeviceStrings::u_colorsystem) {
#ifdef __KERNEL_OPTIX__
      *ptr_data = kernel_params.osl_colorsystem;
      return true;
#else
      (void)ptr_data;
      return false;
#endif
    }
  }
  return false;
}

ccl_device_extern bool rs_texture(ccl_private ShaderGlobals * /*sg*/,
                                  RSDeviceString /*filename*/,
                                  ccl_private void *texture_handle,
                                  ccl_private void * /*texture_thread_info*/,
                                  ccl_private OSLTextureOptions * /*opt*/,
                                  const float s,
                                  const float t,
                                  const float /*dsdx*/,
                                  const float /*dtdx*/,
                                  const float /*dsdy*/,
                                  const float /*dtdy*/,
                                  const int nchannels,
                                  ccl_private float *result,
                                  ccl_private float * /*dresultds*/,
                                  ccl_private float * /*dresultdt*/,
                                  ccl_private void * /*errormessage*/)
{
  const unsigned int type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const unsigned int image_texture_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  switch (type) {
    case OSL_TEXTURE_HANDLE_TYPE_SVM: {
      const float4 rgba = kernel_image_interp(nullptr, image_texture_id, s, 1.0f - t);
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
        result[0] = kernel_ies_interp(nullptr, image_texture_id, s, t);
      }
      return true;
    }
    default: {
      return false;
    }
  }
}

ccl_device_extern bool rs_texture3d(ccl_private ShaderGlobals *sg,
                                    RSDeviceString /*filename*/,
                                    ccl_private void *texture_handle,
                                    ccl_private void * /*texture_thread_info*/,
                                    ccl_private OSLTextureOptions * /*opt*/,
                                    const ccl_private float3 *P,
                                    const ccl_private float3 * /*dPdx*/,
                                    const ccl_private float3 * /*dPdy*/,
                                    const ccl_private float3 * /*dPdz*/,
                                    const int nchannels,
                                    ccl_private float *result,
                                    ccl_private float * /*dresultds*/,
                                    ccl_private float * /*dresultdt*/,
                                    ccl_private float * /*dresultdr*/,
                                    ccl_private void * /*errormessage*/)
{
  const unsigned int type = OSL_TEXTURE_HANDLE_TYPE(texture_handle);
  const unsigned int image_texture_id = OSL_TEXTURE_HANDLE_ID(texture_handle);

  switch (type) {
    case OSL_TEXTURE_HANDLE_TYPE_SVM: {
      const float4 rgba = kernel_image_interp_3d(
          nullptr, sg->sd, image_texture_id, *P, INTERPOLATION_NONE, false);
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

ccl_device_extern bool rs_environment(ccl_private ShaderGlobals * /*sg*/,
                                      RSDeviceString /*filename*/,
                                      ccl_private void * /*texture_handle */,
                                      ccl_private void * /*texture_thread_info*/,
                                      ccl_private OSLTextureOptions * /*opt*/,
                                      const ccl_private float3 * /*R*/,
                                      const ccl_private float3 * /*dRdx*/,
                                      const ccl_private float3 * /*dRdy*/,
                                      const int nchannels,
                                      ccl_private float *result,
                                      ccl_private float * /*dresultds*/,
                                      ccl_private float * /*dresultdt*/,
                                      ccl_private void * /*errormessage*/)
{
  rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
  return false;
}

ccl_device_extern bool rs_get_texture_info(ccl_private ShaderGlobals * /*sg*/,
                                           RSDeviceString /*filename*/,
                                           ccl_private void * /*texture_handle*/,
                                           ccl_private void * /*texture_thread_info*/,
                                           int /*subimage*/,
                                           RSDeviceString /*dataname*/,
                                           TypeDesc /* datatype*/,
                                           ccl_private void * /*data*/,
                                           ccl_private void * /*errormessage*/)
{
  return false;
}

ccl_device_extern bool rs_get_texture_info_st(ccl_private ShaderGlobals * /*sg*/,
                                              RSDeviceString /*filename*/,
                                              ccl_private void * /*texture_handle*/,
                                              const float /*s*/,
                                              const float /*t*/,
                                              ccl_private void * /*texture_thread_info*/,
                                              int /*subimage*/,
                                              RSDeviceString /*dataname*/,
                                              TypeDesc /*datatype*/,
                                              ccl_private void * /*data*/,
                                              ccl_private void * /*errormessage*/)
{
  return false;
}

ccl_device_extern int rs_pointcloud_search(ccl_private ShaderGlobals * /*sg*/,
                                           RSDeviceString /*filename*/,
                                           const ccl_private float3 * /*center*/,
                                           float /*radius*/,
                                           int /*max_points*/,
                                           bool /*sort*/,
                                           ccl_private int * /*out_indices*/,
                                           ccl_private float * /*out_distances*/,
                                           int /*derivs_offset*/)
{
  return 0;
}

ccl_device_extern int rs_pointcloud_get(ccl_private ShaderGlobals * /*sg*/,
                                        RSDeviceString /*filename*/,
                                        const ccl_private int * /*indices*/,
                                        int /*count*/,
                                        RSDeviceString /*attr_name*/,
                                        TypeDesc /*attr_type*/,
                                        ccl_private void * /*out_data*/)
{
  return 0;
}

ccl_device_extern bool rs_pointcloud_write(ccl_private ShaderGlobals * /*sg*/,
                                           RSDeviceString /*filename*/,
                                           const ccl_private float3 * /*pos*/,
                                           int /*nattribs*/,
                                           const ccl_private DeviceString * /*names*/,
                                           const ccl_private TypeDesc * /*types*/,
                                           const ccl_private void ** /*data*/)
{
  return false;
}

ccl_device_extern bool rs_trace(ccl_private ShaderGlobals * /*sg*/,
                                ccl_private void * /*options*/,
                                const ccl_private float3 * /*P*/,
                                const ccl_private float3 * /*dPdx*/,
                                const ccl_private float3 * /*dPdy*/,
                                const ccl_private float3 * /*R*/,
                                const ccl_private float3 * /*dRdx*/,
                                const ccl_private float3 * /*dRdy*/)
{
  return false;
}

ccl_device_extern bool rs_trace_get(ccl_private ShaderGlobals * /*sg*/,
                                    RSDeviceString /*name*/,
                                    TypeDesc /*type*/,
                                    ccl_private void * /*data*/,
                                    bool /*derivatives*/)
{
  return false;
}

/* These osl_ functions are supposed to be implemented by OSL itself, but they are not yet.
 * See: https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/pull/1951
 * So we have to keep them around for now.
 *
 * The 1.14.4 based beta used for Blender 4.5 does not need them though, so we check the
 * version for that. */
#if (OSL_LIBRARY_VERSION_CODE >= 11405) || (OSL_LIBRARY_VERSION_CODE < 11403)
ccl_device_extern void osl_texture_set_firstchannel(ccl_private OSLTextureOptions * /*opt*/,
                                                    const int /*firstchannel*/)
{
}

ccl_device_extern int osl_texture_decode_wrapmode(DeviceString /*name_*/)
{
  return 0;
}

ccl_device_extern void osl_texture_set_swrap_code(ccl_private OSLTextureOptions * /*opt*/,
                                                  const int /*mode*/)
{
}

ccl_device_extern void osl_texture_set_twrap_code(ccl_private OSLTextureOptions * /*opt*/,
                                                  const int /*mode*/)
{
}

ccl_device_extern void osl_texture_set_rwrap_code(ccl_private OSLTextureOptions * /*opt*/,
                                                  const int /*mode*/)
{
}

ccl_device_extern void osl_texture_set_stwrap_code(ccl_private OSLTextureOptions * /*opt*/,
                                                   const int /*mode*/)
{
}

ccl_device_extern void osl_texture_set_sblur(ccl_private OSLTextureOptions * /*opt*/,
                                             const float /*blur*/)
{
}

ccl_device_extern void osl_texture_set_tblur(ccl_private OSLTextureOptions * /*opt*/,
                                             const float /*blur*/)
{
}

ccl_device_extern void osl_texture_set_rblur(ccl_private OSLTextureOptions * /*opt*/,
                                             const float /*blur*/)
{
}

ccl_device_extern void osl_texture_set_stblur(ccl_private OSLTextureOptions * /*opt*/,
                                              const float /*blur*/)
{
}

ccl_device_extern void osl_texture_set_swidth(ccl_private OSLTextureOptions * /*opt*/,
                                              const float /*width*/)
{
}

ccl_device_extern void osl_texture_set_twidth(ccl_private OSLTextureOptions * /*opt*/,
                                              const float /*width*/)
{
}

ccl_device_extern void osl_texture_set_rwidth(ccl_private OSLTextureOptions * /*opt*/,
                                              const float /*width*/)
{
}

ccl_device_extern void osl_texture_set_stwidth(ccl_private OSLTextureOptions * /*opt*/,
                                               const float /*width*/)
{
}

ccl_device_extern void osl_texture_set_fill(ccl_private OSLTextureOptions * /*opt*/,
                                            const float /*fill*/)
{
}

ccl_device_extern void osl_texture_set_time(ccl_private OSLTextureOptions * /*opt*/,
                                            const float /*time*/)
{
}

ccl_device_extern void osl_texture_set_interp_code(ccl_private OSLTextureOptions * /*opt*/,
                                                   const int /*mode*/)
{
}

ccl_device_extern void osl_texture_set_subimage(ccl_private OSLTextureOptions * /*opt*/,
                                                const int /*subimage*/)
{
}

ccl_device_extern void osl_texture_set_subimagename(ccl_private OSLTextureOptions * /*opt*/,
                                                    DeviceString /*subimagename_*/)
{
}

ccl_device_extern void osl_texture_set_missingcolor_arena(ccl_private OSLTextureOptions * /*opt*/,
                                                          ccl_private float3 * /*color*/)
{
}

ccl_device_extern void osl_texture_set_missingcolor_alpha(ccl_private OSLTextureOptions * /*opt*/,
                                                          const int /*nchannels*/,
                                                          const float /*alpha*/)
{
}

ccl_device_extern void osl_init_trace_options(ccl_private void * /*oec*/,
                                              ccl_private void * /*opt*/)
{
}

ccl_device_extern void osl_trace_set_mindist(ccl_private void * /*opt*/, float /*x*/) {}

ccl_device_extern void osl_trace_set_maxdist(ccl_private void * /*opt*/, float /*x*/) {}

ccl_device_extern void osl_trace_set_shade(ccl_private void * /*opt*/, int /*x*/) {}

ccl_device_extern void osl_trace_set_traceset(ccl_private void * /*opt*/, const DeviceString /*x*/)
{
}
#endif

#ifndef __KERNEL_GPU__
CCL_NAMESPACE_END
#endif
