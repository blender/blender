/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __SHADER_H__
#define __SHADER_H__

#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util/windows.h"
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
  ShaderGraph *graph;

  NODE_SOCKET_API(int, pass_id)

  /* sampling */
  NODE_SOCKET_API(EmissionSampling, emission_sampling_method)
  NODE_SOCKET_API(bool, use_transparent_shadow)
  NODE_SOCKET_API(bool, heterogeneous_volume)
  NODE_SOCKET_API(VolumeSampling, volume_sampling_method)
  NODE_SOCKET_API(int, volume_interpolation_method)
  NODE_SOCKET_API(float, volume_step_rate)

  /* displacement */
  NODE_SOCKET_API(DisplacementMethod, displacement_method)

  float prev_volume_step_rate;

  /* synchronization */
  bool need_update_uvs;
  bool need_update_attribute;
  bool need_update_displacement;

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
  bool has_integrator_dependency;

  float3 emission_estimate;
  EmissionSampling emission_sampling;
  bool emission_is_constant;

  /* requested mesh attributes */
  AttributeRequestSet attributes;

  /* determined before compiling */
  uint id;

#ifdef WITH_OSL
  /* osl shading state references */
  OSL::ShaderGroupRef osl_surface_ref;
  OSL::ShaderGroupRef osl_surface_bump_ref;
  OSL::ShaderGroupRef osl_volume_ref;
  OSL::ShaderGroupRef osl_displacement_ref;
#endif

  Shader();
  ~Shader();

  /* Estimate emission of this shader based on the shader graph. This works only in very simple
   * cases. But it helps improve light importance sampling in common cases.
   *
   * If the emission is fully constant, returns true, so that shader evaluation can be skipped
   * entirely for a light. */
  void estimate_emission();

  void set_graph(ShaderGraph *graph);
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
    INTEGRATOR_MODIFIED = (1 << 3),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  static ShaderManager *create(int shadingsystem, Device *device);
  virtual ~ShaderManager();

  virtual void reset(Scene *scene) = 0;

  virtual bool use_osl()
  {
    return false;
  }

  /* device update */
  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  virtual void device_update_specific(Device *device,
                                      DeviceScene *dscene,
                                      Scene *scene,
                                      Progress &progress) = 0;
  virtual void device_free(Device *device, DeviceScene *dscene, Scene *scene) = 0;

  void device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free_common(Device *device, DeviceScene *dscene, Scene *scene);

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

  static void free_memory();

  float linear_rgb_to_gray(float3 c);
  float3 rec709_to_scene_linear(float3 c);

  string get_cryptomatte_materials(Scene *scene);

  void tag_update(Scene *scene, uint32_t flag);

  bool need_update() const;

  void init_xyz_transforms();

 protected:
  ShaderManager();

  uint32_t update_flags;

  typedef unordered_map<ustring, uint64_t, ustringHash> AttributeIDMap;
  AttributeIDMap unique_attribute_id;

  static thread_mutex lookup_table_mutex;

  unordered_map<const float *, size_t> bsdf_tables;

  template<std::size_t n>
  size_t ensure_bsdf_table(DeviceScene *dscene, Scene *scene, const float (&table)[n])
  {
    return ensure_bsdf_table_impl(dscene, scene, table, n);
  }
  size_t ensure_bsdf_table_impl(DeviceScene *dscene, Scene *scene, const float *table, size_t n);

  uint get_graph_kernel_features(ShaderGraph *graph);

  thread_spin_lock attribute_lock_;

  float3 xyz_to_r;
  float3 xyz_to_g;
  float3 xyz_to_b;
  float3 rgb_to_y;
  float3 rec709_to_r;
  float3 rec709_to_g;
  float3 rec709_to_b;
  bool is_rec709;
};

CCL_NAMESPACE_END

#endif /* __SHADER_H__ */
