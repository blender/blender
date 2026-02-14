/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* OSL Render Services
 *
 * Implementation of OSL render services, to retriever matrices, attributes,
 * textures and point clouds. In principle this should only be accessing
 * kernel data, but currently we also reach back into the Scene to retrieve
 * attributes.
 */

#include <OSL/oslclosure.h>
#include <OSL/oslexec.h>
#include <OSL/rendererservices.h>

#include <OpenImageIO/unordered_map_concurrent.h>

#include "scene/image.h"

#include "kernel/osl/compat.h"
#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

class Scene;
struct ShaderData;
struct ThreadKernelGlobalsCPU;

/* OSL Texture Handle
 *
 * OSL texture lookups are string based. If those strings are known at compile
 * time, the OSL compiler can cache a texture handle to use instead of a string.
 *
 * By default it uses TextureSystem::TextureHandle. But since we want to support
 * different kinds of textures and color space conversions, this is our own handle
 * with additional data.
 *
 * These are stored in a concurrent hash map, because OSL can compile multiple
 * shaders in parallel.
 *
 * NOTE: The svm_image_texture_ids array contains a compressed mapping of tile to
 * svm_image_texture_ids pairs stored as follows: x:tile_a,
 * y:svm_image_texture_ids_a, z:tile_b, w:svm_image_texture_ids_b etc. */

struct OSLTextureHandle {
  enum Type { OIIO, SVM, IES, BEVEL, AO };

  OSLTextureHandle(Type type, const vector<int4> &svm_image_texture_ids)
      : type(type), svm_image_texture_ids(svm_image_texture_ids)
  {
  }

  OSLTextureHandle(Type type = OIIO, const int svm_image_texture_ids = -1)
      : OSLTextureHandle(type, {make_int4(0, svm_image_texture_ids, -1, -1)})
  {
  }

  OSLTextureHandle(const ImageHandle &handle)
      : type(SVM), svm_image_texture_ids(handle.get_svm_image_texture_ids()), handle(handle)
  {
  }

  Type type;
  vector<int4> svm_image_texture_ids;
  OSL::TextureSystem::TextureHandle *oiio_handle = nullptr;
  ColorSpaceProcessor *processor = nullptr;
  ImageHandle handle;
};

using OSLTextureHandleMap = OIIO::unordered_map_concurrent<OSLUStringHash, OSLTextureHandle>;

/* OSL Render Services
 *
 * Interface for OSL to access attributes, textures and other scene data. */

class OSLRenderServices : public OSL::RendererServices {
 public:
  OSLRenderServices(OSL::TextureSystem *texture_system, const int device_type);
  ~OSLRenderServices() override;

  static void register_closures(OSL::ShadingSystem *ss);

  int supports(string_view feature) const override;

  bool get_matrix(OSL::ShaderGlobals *sg,
                  OSL::Matrix44 &result,
                  OSL::TransformationPtr xform,
                  float time) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          OSL::TransformationPtr xform,
                          float time) override;

  bool get_matrix(OSL::ShaderGlobals *sg,
                  OSL::Matrix44 &result,
                  OSLUStringHash from,
                  float time) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          OSLUStringHash to,
                          float time) override;

  bool get_matrix(OSL::ShaderGlobals *sg,
                  OSL::Matrix44 &result,
                  OSL::TransformationPtr xform) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          OSL::TransformationPtr xform) override;

  bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSLUStringHash from) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          OSLUStringHash to) override;

  bool get_array_attribute(OSL::ShaderGlobals *sg,
                           bool derivatives,
                           OSLUStringHash object,
                           const TypeDesc type,
                           OSLUStringHash name,
                           const int index,
                           void *val) override;
  bool get_attribute(OSL::ShaderGlobals *sg,
                     bool derivatives,
                     OSLUStringHash object,
                     const TypeDesc type,
                     OSLUStringHash name,
                     void *val) override;

  bool get_userdata(bool derivatives,
                    OSLUStringHash name,
                    const TypeDesc type,
                    OSL::ShaderGlobals *sg,
                    void *val) override;

  int pointcloud_search(OSL::ShaderGlobals *sg,
                        OSLUStringHash filename,
                        const OSL::Vec3 &center,
                        const float radius,
                        const int max_points,
                        bool sort,
#if OSL_LIBRARY_VERSION_CODE >= 11400
                        int *out_indices,
#else
                        size_t *out_indices,
#endif
                        float *out_distances,
                        int derivs_offset) override;

  int pointcloud_get(OSL::ShaderGlobals *sg,
                     OSLUStringHash filename,
#if OSL_LIBRARY_VERSION_CODE >= 11400
                     const int *indices,
#else
                     size_t *indices,
#endif

                     const int count,
                     OSLUStringHash attr_name,
                     const TypeDesc attr_type,
                     void *out_data) override;

  bool pointcloud_write(OSL::ShaderGlobals *sg,
                        OSLUStringHash filename,
                        const OSL::Vec3 &pos,
                        const int nattribs,
                        const OSLUStringRep *names,
                        const TypeDesc *types,
                        const void **data) override;

  bool trace(TraceOpt &options,
             OSL::ShaderGlobals *sg,
             const OSL::Vec3 &P,
             const OSL::Vec3 &dPdx,
             const OSL::Vec3 &dPdy,
             const OSL::Vec3 &R,
             const OSL::Vec3 &dRdx,
             const OSL::Vec3 &dRdy) override;

  bool getmessage(OSL::ShaderGlobals *sg,
                  OSLUStringHash source,
                  OSLUStringHash name,
                  const TypeDesc type,
                  void *val,
                  bool derivatives) override;

  OSL::TextureSystem::TextureHandle *get_texture_handle(OSL::ustring filename,
                                                        OSL::ShadingContext *context,
                                                        const OSL::TextureOpt *options) override;
  OSL::TextureSystem::TextureHandle *get_texture_handle(OSLUStringHash filename,
                                                        OSL::ShadingContext *context,
                                                        const OSL::TextureOpt *options) override;

  bool good(OSL::TextureSystem::TextureHandle *texture_handle) override;

  bool texture(OSLUStringHash filename,
               OSL::TextureSystem::TextureHandle *texture_handle,
               TexturePerthread *texture_thread_info,
               OSL::TextureOpt &options,
               OSL::ShaderGlobals *sg,
               const float s,
               const float t,
               const float dsdx,
               const float dtdx,
               const float dsdy,
               const float dtdy,
               const int nchannels,
               float *result,
               float *dresultds,
               float *dresultdt,
               OSLUStringHash *errormessage) override;

  bool texture3d(OSLUStringHash filename,
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
                 OSLUStringHash *errormessage) override;

  bool environment(OSLUStringHash filename,
                   TextureHandle *texture_handle,
                   TexturePerthread *texture_thread_info,
                   OSL::TextureOpt &options,
                   OSL::ShaderGlobals *sg,
                   const OSL::Vec3 &R,
                   const OSL::Vec3 &dRdx,
                   const OSL::Vec3 &dRdy,
                   const int nchannels,
                   float *result,
                   float *dresultds,
                   float *dresultdt,
                   OSLUStringHash *errormessage) override;

  bool get_texture_info(OSLUStringHash filename,
                        TextureHandle *texture_handle,
                        TexturePerthread *texture_thread_info,
                        OSL::ShaderGlobals *sg,
                        const int subimage,
                        OSLUStringHash dataname,
                        const TypeDesc datatype,
                        void *data,
                        OSLUStringHash *errormessage) override;

  static bool get_attribute(ShaderGlobals *globals,
                            ShaderData *sd,
                            bool derivatives,
                            OSLUStringHash object_name,
                            TypeDesc type,
                            OSLUStringHash name,
                            void *val);

  static bool get_background_attribute(ShaderGlobals *globals,
                                       ShaderData *sd,
                                       OSLUStringHash name,
                                       TypeDesc type,
                                       bool derivatives,
                                       void *val);
  static bool get_camera_attribute(
      ShaderGlobals *globals, OSLUStringHash name, TypeDesc type, bool derivatives, void *val);
  static bool get_object_standard_attribute(ShaderGlobals *globals,
                                            ShaderData *sd,
                                            OSLUStringHash name,
                                            TypeDesc type,
                                            bool derivatives,
                                            void *val);

  /* Texture system and texture handle map are part of the services instead of
   * globals to be shared between different render sessions. This saves memory,
   * and is required because texture handles are cached as part of the shared
   * shading system. */
  OSLTextureHandleMap textures;

  static ImageManager *image_manager;

 private:
  int device_type_;
};

CCL_NAMESPACE_END
