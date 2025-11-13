/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OSL
#  include <cstdint> /* Needed before `sdlexec.h` for `int32_t` with GCC 15.1. */
/* So no context pollution happens from indirectly included windows.h */
#  ifdef _WIN32
#    include "util/windows.h"
#  endif
#  include <OSL/oslexec.h>
#endif

#include "kernel/types.h"
#include "scene/attribute.h"

#include "graph/node.h"

#include "util/map.h"
#include "util/param.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/types.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
class Progress;
class Scene;
class ShaderGraph;
struct float3;

enum ShadingSystem { SHADINGSYSTEM_OSL, SHADINGSYSTEM_SVM };

/* Keep those in sync with the python-defined enum. */

enum VolumeSampling {
  VOLUME_SAMPLING_DISTANCE = 0,
  VOLUME_SAMPLING_EQUIANGULAR = 1,
  VOLUME_SAMPLING_MULTIPLE_IMPORTANCE = 2,

  VOLUME_NUM_SAMPLING,
};

enum VolumeInterpolation {
  VOLUME_INTERPOLATION_LINEAR = 0,
  VOLUME_INTERPOLATION_CUBIC = 1,

  VOLUME_NUM_INTERPOLATION,
};

enum DisplacementMethod {
  DISPLACE_BUMP = 0,
  DISPLACE_TRUE = 1,
  DISPLACE_BOTH = 2,

  DISPLACE_NUM_METHODS,
};

/* Shader describing the appearance of a Mesh, Light or Background.
 *
 * While there is only a single shader graph, it has three outputs: surface,
 * volume and displacement, that the shader manager will compile and execute
 * separately. */

class Shader : public Node {
 public:
  NODE_DECLARE

  /* shader graph */
  unique_ptr<ShaderGraph> graph;

  NODE_SOCKET_API(int, pass_id)

  /* sampling */
  NODE_SOCKET_API(EmissionSampling, emission_sampling_method)
  NODE_SOCKET_API(bool, use_transparent_shadow)
  NODE_SOCKET_API(bool, use_bump_map_correction)
  NODE_SOCKET_API(VolumeSampling, volume_sampling_method)
  NODE_SOCKET_API(int, volume_interpolation_method)
  NODE_SOCKET_API(float, volume_step_rate)

  /* displacement */
  NODE_SOCKET_API(DisplacementMethod, displacement_method)

  float prev_volume_step_rate;
  bool prev_has_surface_shadow_transparency;

  /* synchronization */
  bool need_update_uvs;
  bool need_update_attribute;
  bool need_update_displacement;
  bool need_update_shadow_transparency;
  bool shadow_transparency_needs_realloc;

  /* If the shader has only volume components, the surface is assumed to
   * be transparent.
   * However, graph optimization might remove the volume subgraph, but
   * since the user connected something to the volume output the surface
   * should still be transparent.
   * Therefore, has_volume_connected stores whether some volume sub-tree
   * was connected before optimization. */
  bool has_volume_connected;

  /* information about shader after compiling */
  bool has_surface;
  bool has_surface_transparent;
  bool has_surface_raytrace;
  bool has_volume;
  bool has_displacement;
  bool has_surface_bssrdf;
  bool has_bump;
  bool has_bssrdf_bump;
  bool has_surface_spatial_varying;
  bool has_volume_spatial_varying;
  bool has_volume_attribute_dependency;
  bool has_light_path_node;

  float3 emission_estimate;
  EmissionSampling emission_sampling;
  bool emission_is_constant;

  /* requested mesh attributes */
  AttributeRequestSet attributes;

  /* determined before compiling */
  uint id;

#ifdef WITH_OSL
  /* Compiled osl shading state references. */
  struct OSLCache {
    OSL::ShaderGroupRef surface;
    OSL::ShaderGroupRef bump;
    OSL::ShaderGroupRef displacement;
    OSL::ShaderGroupRef volume;
  };
  map<Device *, OSLCache> osl_cache;
#endif

  Shader();

  /* Estimate emission of this shader based on the shader graph. This works only in very simple
   * cases. But it helps improve light importance sampling in common cases.
   *
   * If the emission is fully constant, returns true, so that shader evaluation can be skipped
   * entirely for a light. */
  void estimate_emission();

  void set_graph(unique_ptr<ShaderGraph> &&graph);
  void tag_update(Scene *scene);
  void tag_used(Scene *scene);

  /* Return true when either of the surface or displacement socket of the output node is linked.
   * This should be used to ensure that surface attributes are also requested even when only the
   * displacement socket is linked. */
  bool has_surface_link() const
  {
    return has_surface || has_displacement;
  }

  bool need_update_geometry() const;

  bool has_surface_shadow_transparency() const;
};

/* Shader Manager virtual base class
 *
 * From this the SVM and OSL shader managers are derived, that do the actual
 * shader compiling and device updating. */

class ShaderManager {
 public:
  enum : uint32_t {
    SHADER_ADDED = (1 << 0),
    SHADER_MODIFIED = (1 << 2),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  static unique_ptr<ShaderManager> create(const int shadingsystem);
  virtual ~ShaderManager();

  virtual bool use_osl()
  {
    return false;
  }

  /* device update */
  void device_update_pre(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_update_post(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  virtual void device_free(Device *device, DeviceScene *dscene, Scene *scene) = 0;

  /* get globally unique id for a type of attribute */
  virtual uint64_t get_attribute_id(ustring name);
  virtual uint64_t get_attribute_id(AttributeStandard std);

  /* get shader id for mesh faces */
  int get_shader_id(Shader *shader, bool smooth = false);

  /* add default shaders to scene, to use as default for things that don't
   * have any shader assigned explicitly */
  static void add_default(Scene *scene);

  /* Selective nodes compilation. */
  uint get_kernel_features(Scene *scene);

  float linear_rgb_to_gray(const float3 c);
  float3 rec709_to_scene_linear(const float3 c);

  string get_cryptomatte_materials(Scene *scene);

  void tag_update(Scene *scene, const uint32_t flag);

  bool need_update() const;

  void init_xyz_transforms();

  enum class SceneLinearSpace { Rec709, Rec2020, ACEScg, Unknown };

  SceneLinearSpace get_scene_linear_space()
  {
    return scene_linear_space;
  }

 protected:
  ShaderManager();

  uint32_t update_flags;

  using AttributeIDMap = unordered_map<ustring, uint64_t>;
  AttributeIDMap unique_attribute_id;

  static thread_mutex lookup_table_mutex;

  unordered_map<const float *, size_t> bsdf_tables;
  size_t thin_film_table_offset_;

  thread_spin_lock attribute_lock_;

  float3 xyz_to_r;
  float3 xyz_to_g;
  float3 xyz_to_b;
  float3 rgb_to_y;
  float3 white_xyz;
  float3 rec709_to_r;
  float3 rec709_to_g;
  float3 rec709_to_b;
  SceneLinearSpace scene_linear_space;
  vector<float> thin_film_table;

  template<std::size_t n>
  size_t ensure_bsdf_table(DeviceScene *dscene, Scene *scene, const float (&table)[n])
  {
    return ensure_bsdf_table_impl(dscene, scene, table, n);
  }
  size_t ensure_bsdf_table_impl(DeviceScene *dscene,
                                Scene *scene,
                                const float *table,
                                const size_t n);

  void compute_thin_film_table(const Transform &xyz_to_rgb);

  uint get_graph_kernel_features(ShaderGraph *graph);

  virtual void device_update_specific(Device *device,
                                      DeviceScene *dscene,
                                      Scene *scene,
                                      Progress &progress) = 0;

  void device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free_common(Device *device, DeviceScene *dscene, Scene *scene);
};

CCL_NAMESPACE_END
