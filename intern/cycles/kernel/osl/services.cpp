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

#include "scene/object.h"

#include "util/colorspace.h"
#include "util/log.h"
#include "util/string.h"

#include "kernel/device/cpu/image.h"

#include "kernel/integrator/state.h"
#include "kernel/integrator/state_util.h"

#include "kernel/geom/primitive.h"
#include "kernel/geom/shader_data.h"

#include "kernel/bvh/bvh.h"

#include "kernel/camera/camera.h"

#include "kernel/svm/ao.h"
#include "kernel/svm/bevel.h"

#include "kernel/util/ies.h"
#include "kernel/util/image_3d.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"
#include "kernel/osl/services_shared.h"
#include "kernel/osl/strings.h"
#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

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
  return osl_shared_get_object_matrix_motion(
      globals->kg, globals->sd, reinterpret_cast<float *>(&result), time);
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
  return osl_shared_get_object_inverse_matrix_motion(
      globals->kg, globals->sd, reinterpret_cast<float *>(&result), time);
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSLUStringHash from,
                                   const float /*time*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  return osl_shared_get_named_matrix(globals->kg, from, reinterpret_cast<float *>(&result));
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSLUStringHash to,
                                           const float /*time*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  return osl_shared_get_named_inverse_matrix(globals->kg, to, reinterpret_cast<float *>(&result));
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSL::TransformationPtr /*xform*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }
  return osl_shared_get_object_matrix(
      globals->kg, globals->sd, reinterpret_cast<float *>(&result));
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSL::TransformationPtr /*xform*/)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  if (globals == nullptr || globals->sd == nullptr) {
    return false;
  }
  return osl_shared_get_object_inverse_matrix(
      globals->kg, globals->sd, reinterpret_cast<float *>(&result));
}

bool OSLRenderServices::get_matrix(OSL::ShaderGlobals *sg,
                                   OSL::Matrix44 &result,
                                   OSLUStringHash from)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  return osl_shared_get_named_matrix(globals->kg, from, reinterpret_cast<float *>(&result));
}

bool OSLRenderServices::get_inverse_matrix(OSL::ShaderGlobals *sg,
                                           OSL::Matrix44 &result,
                                           OSLUStringHash to)
{
  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  return osl_shared_get_named_inverse_matrix(globals->kg, to, reinterpret_cast<float *>(&result));
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

bool OSLRenderServices::get_object_standard_attribute(ShaderGlobals *globals,
                                                      ShaderData *sd,
                                                      OSLUStringHash name,
                                                      const TypeDesc type,
                                                      bool derivatives,
                                                      void *val)
{
  return osl_shared_get_object_standard_attribute(
      globals->kg, globals, sd, name, type, derivatives, val);
}

bool OSLRenderServices::get_background_attribute(ShaderGlobals *globals,
                                                 ShaderData *sd,
                                                 OSLUStringHash name,
                                                 const TypeDesc type,
                                                 bool derivatives,
                                                 void *val)
{
  return osl_shared_get_background_attribute(
      globals->kg, globals, sd, name, type, derivatives, val);
}

bool OSLRenderServices::get_camera_attribute(
    ShaderGlobals *globals, OSLUStringHash name, TypeDesc type, bool derivatives, void *val)
{
  return osl_shared_get_camera_attribute(globals->kg, globals, name, type, derivatives, val);
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

  return get_attribute(globals, globals->sd, derivatives, object_name, type, name, val);
}

bool OSLRenderServices::get_attribute(ShaderGlobals *globals,
                                      ShaderData *sd,
                                      bool derivatives,
                                      OSLUStringHash object_name,
                                      const TypeDesc type,
                                      OSLUStringHash name,
                                      void *val)
{

  if (globals == nullptr) {
    return false;
  }

  const ThreadKernelGlobalsCPU *kg = globals->kg;
  if (sd == nullptr) {
    /* Camera shader. */
    return osl_shared_get_camera_attribute(kg, globals, name, type, derivatives, val);
  }

  /* lookup of attribute on another object */
  int object;
  if (object_name != DeviceStrings::u_empty) {
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
    return osl_shared_get_object_attribute(kg, sd, desc, type, derivatives, val);
  }

  /* not found in attribute, check standard object info */
  return osl_shared_get_object_standard_attribute(kg, globals, sd, name, type, derivatives, val);
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
                                                                     it->second.id);
      case OSLTextureHandle::IES:
        if (!it->second.handle.empty() && it->second.handle.get_manager() != image_manager) {
          it.clear();
          break;
        }
        return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(OSL_TEXTURE_HANDLE_TYPE_IES |
                                                                     it->second.id);
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
                                                               handle.kernel_id());
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
      const float4 rgba = kernel_image_interp_with_udim(
          kernel_globals, sd, handle->id, make_float2(s, 1.0f - t));

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
      result[0] = kernel_ies_interp(kernel_globals, handle->id, s, t);
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
    rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
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
      const float3 P_float3 = make_float3(P.x, P.y, P.z);
      float4 rgba = kernel_image_interp_3d(
          kernel_globals, globals->sd, handle->id, P_float3, INTERPOLATION_NONE, false);

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
    rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
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
    rgba_to_nchannels(IMAGE_MISSING_RGBA, nchannels, result);
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
  tracedata->self_hit = false;

  /* Can't ray-trace from shaders like displacement, before BVH exists. */
  if (kernel_data.bvh.bvh_layout == BVH_LAYOUT_NONE) {
    return false;
  }

  if (options.traceset == DeviceStrings::u_traceset_only_local) {
    LocalIntersection local_isect;
    scene_intersect_local(kg, &ray, &local_isect, sd->object, nullptr, 1);
    if (local_isect.num_hits > 0) {
      tracedata->isect = local_isect.hits[0];
      tracedata->hit = true;
      tracedata->self_hit = true;
    }
  }
  else {
    /* Ray-trace, leaving out shadow opaque to avoid early exit. */
    const uint visibility = PATH_RAY_ALL_VISIBILITY - PATH_RAY_SHADOW_OPAQUE;
    tracedata->hit = scene_intersect(kg, &ray, visibility, &tracedata->isect);
    if (tracedata->hit) {
      tracedata->self_hit = tracedata->isect.object == sd->object;
    }
  }
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

  if (source == DeviceStrings::u_trace && tracedata->init) {
    if (name == DeviceStrings::u_hit) {
      return set_attribute<int>(tracedata->hit, type, derivatives, val);
    }
    if (tracedata->hit) {
      if (name == DeviceStrings::u_hitdist) {
        return set_attribute(tracedata->isect.t, type, derivatives, val);
      }

      ShaderData *sd = &tracedata->sd;

      if (!tracedata->setup) {
        /* lazy shader data setup */
        shader_setup_from_ray(kg, sd, &tracedata->ray, &tracedata->isect);
        tracedata->setup = true;
      }

      if (name == DeviceStrings::u_hitself) {
        return set_attribute(float(tracedata->self_hit), type, derivatives, val);
      }
      if (name == DeviceStrings::u_N) {
        return set_attribute(sd->N, type, derivatives, val);
      }
      if (name == DeviceStrings::u_Ng) {
        return set_attribute(sd->Ng, type, derivatives, val);
      }
      if (name == DeviceStrings::u_P) {
        const differential3 dP = differential_from_compact(sd->Ng, sd->dP);
        return set_attribute(dual3(sd->P, dP.dx, dP.dy), type, derivatives, val);
      }
      if (name == DeviceStrings::u_I) {
        const differential3 dI = differential_from_compact(sd->wi, sd->dI);
        return set_attribute(dual3(sd->wi, dI.dx, dI.dy), type, derivatives, val);
      }
      if (name == DeviceStrings::u_u) {
        return set_attribute(dual1(sd->u, sd->du.dx, sd->du.dy), type, derivatives, val);
      }
      if (name == DeviceStrings::u_v) {
        return set_attribute(dual1(sd->v, sd->dv.dx, sd->dv.dy), type, derivatives, val);
      }

      return get_attribute(globals, sd, derivatives, DeviceStrings::u_empty, type, name, val);
    }
  }

  return false;
}

CCL_NAMESPACE_END
