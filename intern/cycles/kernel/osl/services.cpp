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

#include "util/log.h"
#include "util/string.h"

#include "kernel/geom/shader_data.h"

#include "kernel/bvh/bvh.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"
#include "kernel/osl/services_shared.h"
#include "kernel/osl/strings.h"
#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

/* RenderServices implementation */

ImageManager *OSLRenderServices::image_manager = nullptr;

OSLRenderServices::OSLRenderServices(const int device_type)
    /* Dummy texture system pointer so OSL doesn't create its own. Such an opaque texture system
     * pointer is supported and normally would be done with the OSL_NO_DEFAULT_TEXTURESYSTEM
     * build option, but we want to work with OSL builds that don't have it. */
    : OSL::RendererServices(reinterpret_cast<OSL::TextureSystem *>(1)), device_type_(device_type)
{
}

OSLRenderServices::~OSLRenderServices() = default;

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
  /* Note this mutex is not so bad for performance because this function only gets
   * called once per texture handle to create it, not for every texture access. */
  auto [it, inserted] = textures.find_or_insert(filename,
                                                OSLTextureHandle(OSLTextureHandleType::IMAGE));

  if (inserted) {
    /* Add new texture to image manager. */
    const ImageHandle handle = image_manager->add_image(filename.string(), ImageParams());
    OSLTextureHandle *texture_handle = const_cast<OSLTextureHandle *>(&it->second);
    *texture_handle = OSLTextureHandle(handle);
  }

  /* Construct texture handle. We encode this as a packed integer cast to a pointer,
   * which is also what we use on the GPU. OSL does not dereference these.
   *
   * Note that we must keep the OSLTextureHandle in the map alive, as it holds
   * the ImageHandle that keeps the image loaded in the manager. */
  return reinterpret_cast<OSL::TextureSystem::TextureHandle *>(
      OSL_TEXTURE_HANDLE_ENCODE(it->second.type, it->second.id));
}

bool OSLRenderServices::good(OSL::TextureSystem::TextureHandle *texture_handle)
{
  return OSL_TEXTURE_HANDLE_TYPE(texture_handle) != OSLTextureHandleType::IMAGE ||
         OSL_TEXTURE_HANDLE_ID(texture_handle) != KERNEL_IMAGE_NONE;
}

bool OSLRenderServices::is_udim(OSL::TextureSystem::TextureHandle *texture_handle)
{
  return OSL_TEXTURE_HANDLE_TYPE(texture_handle) == OSLTextureHandleType::IMAGE &&
         OSL_TEXTURE_HANDLE_ID(texture_handle) <= -1;
}

bool OSLRenderServices::texture(OSLUStringHash filename,
                                TextureHandle *texture_handle,
                                TexturePerthread * /*texture_thread_info*/,
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
                                float * /*dresultds*/,
                                float * /*dresultdt*/,
                                OSLUStringHash * /*errormessage*/)
{
  if (texture_handle == nullptr) {
    if (texture_filenames_seen.insert(filename).second) {
      LOG_WARNING << "Open Shading Language texture call can not resolve " << filename.c_str()
                  << ", filename must be a constant";
    }
    return false;
  }

  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  return osl_shared_texture(globals->kg,
                            globals,
                            texture_handle,
                            static_cast<void *>(&options),
                            s,
                            t,
                            dsdx,
                            dtdx,
                            dsdy,
                            dtdy,
                            nchannels,
                            result);
}

bool OSLRenderServices::texture3d(OSLUStringHash filename,
                                  TextureHandle *texture_handle,
                                  TexturePerthread * /*texture_thread_info*/,
                                  OSL::TextureOpt & /*options*/,
                                  OSL::ShaderGlobals *sg,
                                  const OSL::Vec3 &P,
                                  const OSL::Vec3 &dPdx,
                                  const OSL::Vec3 &dPdy,
                                  const OSL::Vec3 &dPdz,
                                  const int nchannels,
                                  float *result,
                                  float * /*dresultds*/,
                                  float * /*dresultdt*/,
                                  float * /*dresultdr*/,
                                  OSLUStringHash * /*errormessage*/)
{
  if (texture_handle == nullptr) {
    if (texture_filenames_seen.insert(filename).second) {
      LOG_WARNING << "Open Shading Language texture3d call can not resolve " << filename.c_str()
                  << ", filename must be a constant";
    }
    return false;
  }

  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  return osl_shared_texture3d(globals->kg,
                              globals,
                              texture_handle,
                              make_float3(P.x, P.y, P.z),
                              make_float3(dPdx.x, dPdx.y, dPdx.z),
                              make_float3(dPdy.x, dPdy.y, dPdy.z),
                              make_float3(dPdz.x, dPdz.y, dPdz.z),
                              nchannels,
                              result);
}

bool OSLRenderServices::environment(OSLUStringHash filename,
                                    TextureHandle *texture_handle,
                                    TexturePerthread * /*thread_info*/,
                                    OSL::TextureOpt & /*options*/,
                                    OSL::ShaderGlobals *sg,
                                    const OSL::Vec3 &R,
                                    const OSL::Vec3 &dRdx,
                                    const OSL::Vec3 &dRdy,
                                    const int nchannels,
                                    float *result,
                                    float * /*dresultds*/,
                                    float * /*dresultdt*/,
                                    OSLUStringHash * /*errormessage*/)
{
  if (texture_handle == nullptr) {
    if (texture_filenames_seen.insert(filename).second) {
      LOG_WARNING << "Open Shading Language environment call can not resolve " << filename.c_str()
                  << ", filename must be a constant";
    }
    return false;
  }

  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);

  return osl_shared_environment(globals->kg,
                                globals,
                                texture_handle,
                                make_float3(R.x, R.y, R.z),
                                make_float3(dRdx.x, dRdx.y, dRdx.z),
                                make_float3(dRdy.x, dRdy.y, dRdy.z),
                                nchannels,
                                result);
}

bool OSLRenderServices::get_texture_info(OSLUStringHash filename,
                                         TextureHandle *texture_handle,
                                         TexturePerthread * /*texture_thread_info*/,
                                         OSL::ShaderGlobals *sg,
                                         const int /*subimage*/,
                                         OSLUStringHash dataname,
                                         const TypeDesc datatype,
                                         void *data,
                                         OSLUStringHash * /*errormessage*/)
{
  if (texture_handle == nullptr) {
    if (texture_filenames_seen.insert(filename).second) {
      LOG_WARNING << "Open Shading Language gettextureinfo call can not resolve "
                  << filename.c_str() << ", filename must be a constant";
    }
    return false;
  }

  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  return osl_shared_get_texture_info(
      kg, texture_handle, make_float2(0.0f, 0.0f), false, dataname, datatype, data);
}

bool OSLRenderServices::get_texture_info(OSLUStringHash filename,
                                         TextureHandle *texture_handle,
                                         float s,
                                         float t,
                                         TexturePerthread * /*texture_thread_info*/,
                                         OSL::ShaderGlobals *sg,
                                         const int /*subimage*/,
                                         OSLUStringHash dataname,
                                         const TypeDesc datatype,
                                         void *data,
                                         OSLUStringHash * /*errormessage*/)
{
  if (texture_handle == nullptr) {
    if (texture_filenames_seen.insert(filename).second) {
      LOG_WARNING << "Open Shading Language gettextureinfo call can not resolve "
                  << filename.c_str() << ", filename must be a constant";
    }
    return false;
  }

  ShaderGlobals *globals = reinterpret_cast<ShaderGlobals *>(sg);
  const ThreadKernelGlobalsCPU *kg = globals->kg;

  return osl_shared_get_texture_info(
      kg, texture_handle, make_float2(s, t), true, dataname, datatype, data);
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
