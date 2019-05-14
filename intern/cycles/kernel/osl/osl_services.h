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

#ifndef __OSL_SERVICES_H__
#define __OSL_SERVICES_H__

/* OSL Render Services
 *
 * Implementation of OSL render services, to retriever matrices, attributes,
 * textures and point clouds. In principle this should only be accessing
 * kernel data, but currently we also reach back into the Scene to retrieve
 * attributes.
 */

#include <OSL/oslexec.h>
#include <OSL/oslclosure.h>

#ifdef WITH_PTEX
class PtexCache;
#endif

CCL_NAMESPACE_BEGIN

class Object;
class Scene;
class Shader;
struct ShaderData;
struct float3;
struct KernelGlobals;

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
 * shaders in parallel. */

struct OSLTextureHandle : public OIIO::RefCnt {
  enum Type { OIIO, SVM, IES, BEVEL, AO };

  OSLTextureHandle(Type type = OIIO, int svm_slot = -1)
      : type(type), svm_slot(svm_slot), oiio_handle(NULL), processor(NULL)
  {
  }

  Type type;
  int svm_slot;
  OSL::TextureSystem::TextureHandle *oiio_handle;
  ColorSpaceProcessor *processor;
};

typedef OIIO::intrusive_ptr<OSLTextureHandle> OSLTextureHandleRef;
typedef OIIO::unordered_map_concurrent<ustring, OSLTextureHandleRef, ustringHash>
    OSLTextureHandleMap;

/* OSL Render Services
 *
 * Interface for OSL to access attributes, textures and other scene data. */

class OSLRenderServices : public OSL::RendererServices {
 public:
  OSLRenderServices(OSL::TextureSystem *texture_system);
  ~OSLRenderServices();

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
                  ustring from,
                  float time) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          ustring to,
                          float time) override;

  bool get_matrix(OSL::ShaderGlobals *sg,
                  OSL::Matrix44 &result,
                  OSL::TransformationPtr xform) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg,
                          OSL::Matrix44 &result,
                          OSL::TransformationPtr xform) override;

  bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from) override;
  bool get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from) override;

  bool get_array_attribute(OSL::ShaderGlobals *sg,
                           bool derivatives,
                           ustring object,
                           TypeDesc type,
                           ustring name,
                           int index,
                           void *val) override;
  bool get_attribute(OSL::ShaderGlobals *sg,
                     bool derivatives,
                     ustring object,
                     TypeDesc type,
                     ustring name,
                     void *val) override;
  bool get_attribute(ShaderData *sd,
                     bool derivatives,
                     ustring object_name,
                     TypeDesc type,
                     ustring name,
                     void *val);

  bool get_userdata(
      bool derivatives, ustring name, TypeDesc type, OSL::ShaderGlobals *sg, void *val) override;

  int pointcloud_search(OSL::ShaderGlobals *sg,
                        ustring filename,
                        const OSL::Vec3 &center,
                        float radius,
                        int max_points,
                        bool sort,
                        size_t *out_indices,
                        float *out_distances,
                        int derivs_offset) override;

  int pointcloud_get(OSL::ShaderGlobals *sg,
                     ustring filename,
                     size_t *indices,
                     int count,
                     ustring attr_name,
                     TypeDesc attr_type,
                     void *out_data) override;

  bool pointcloud_write(OSL::ShaderGlobals *sg,
                        ustring filename,
                        const OSL::Vec3 &pos,
                        int nattribs,
                        const ustring *names,
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
                  ustring source,
                  ustring name,
                  TypeDesc type,
                  void *val,
                  bool derivatives) override;

  TextureSystem::TextureHandle *get_texture_handle(ustring filename) override;

  bool good(TextureSystem::TextureHandle *texture_handle) override;

  bool texture(ustring filename,
               TextureSystem::TextureHandle *texture_handle,
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
               ustring *errormessage) override;

  bool texture3d(ustring filename,
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
                 ustring *errormessage) override;

  bool environment(ustring filename,
                   TextureHandle *texture_handle,
                   TexturePerthread *texture_thread_info,
                   TextureOpt &options,
                   OSL::ShaderGlobals *sg,
                   const OSL::Vec3 &R,
                   const OSL::Vec3 &dRdx,
                   const OSL::Vec3 &dRdy,
                   int nchannels,
                   float *result,
                   float *dresultds,
                   float *dresultdt,
                   ustring *errormessage) override;

  bool get_texture_info(OSL::ShaderGlobals *sg,
                        ustring filename,
                        TextureHandle *texture_handle,
                        int subimage,
                        ustring dataname,
                        TypeDesc datatype,
                        void *data) override;

  static bool get_background_attribute(
      KernelGlobals *kg, ShaderData *sd, ustring name, TypeDesc type, bool derivatives, void *val);
  static bool get_object_standard_attribute(
      KernelGlobals *kg, ShaderData *sd, ustring name, TypeDesc type, bool derivatives, void *val);

  static ustring u_distance;
  static ustring u_index;
  static ustring u_world;
  static ustring u_camera;
  static ustring u_screen;
  static ustring u_raster;
  static ustring u_ndc;
  static ustring u_object_location;
  static ustring u_object_index;
  static ustring u_geom_dupli_generated;
  static ustring u_geom_dupli_uv;
  static ustring u_material_index;
  static ustring u_object_random;
  static ustring u_particle_index;
  static ustring u_particle_random;
  static ustring u_particle_age;
  static ustring u_particle_lifetime;
  static ustring u_particle_location;
  static ustring u_particle_rotation;
  static ustring u_particle_size;
  static ustring u_particle_velocity;
  static ustring u_particle_angular_velocity;
  static ustring u_geom_numpolyvertices;
  static ustring u_geom_trianglevertices;
  static ustring u_geom_polyvertices;
  static ustring u_geom_name;
  static ustring u_geom_undisplaced;
  static ustring u_is_smooth;
  static ustring u_is_curve;
  static ustring u_curve_thickness;
  static ustring u_curve_tangent_normal;
  static ustring u_curve_random;
  static ustring u_path_ray_length;
  static ustring u_path_ray_depth;
  static ustring u_path_diffuse_depth;
  static ustring u_path_glossy_depth;
  static ustring u_path_transparent_depth;
  static ustring u_path_transmission_depth;
  static ustring u_trace;
  static ustring u_hit;
  static ustring u_hitdist;
  static ustring u_N;
  static ustring u_Ng;
  static ustring u_P;
  static ustring u_I;
  static ustring u_u;
  static ustring u_v;
  static ustring u_empty;
  static ustring u_at_bevel;
  static ustring u_at_ao;

  /* Texture system and texture handle map are part of the services instead of
   * globals to be shared between different render sessions. This saves memory,
   * and is required because texture handles are cached as part of the shared
   * shading system. */
  OSL::TextureSystem *texture_system;
  OSLTextureHandleMap textures;
};

CCL_NAMESPACE_END

#endif /* __OSL_SERVICES_H__  */
