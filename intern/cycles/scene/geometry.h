/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"

#include "bvh/params.h"

#include "scene/attribute.h"

#include "util/boundbox.h"
#include "util/set.h"
#include "util/task.h"
#include "util/transform.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class BVH;
class Device;
class DeviceScene;
class Mesh;
class Progress;
class RenderStats;
class Scene;
class SceneParams;
class Shader;
class Volume;
struct PackedBVH;

/* Set of flags used to help determining what data has been modified or needs reallocation, so we
 * can decide which device data to free or update. */
enum {
  DEVICE_CURVE_DATA_MODIFIED = (1 << 0),
  DEVICE_MESH_DATA_MODIFIED = (1 << 1),
  DEVICE_POINT_DATA_MODIFIED = (1 << 2),

  ATTR_FLOAT_MODIFIED = (1 << 3),
  ATTR_FLOAT2_MODIFIED = (1 << 4),
  ATTR_FLOAT3_MODIFIED = (1 << 5),
  ATTR_FLOAT4_MODIFIED = (1 << 6),
  ATTR_UCHAR4_MODIFIED = (1 << 7),

  CURVE_DATA_NEED_REALLOC = (1 << 8),
  MESH_DATA_NEED_REALLOC = (1 << 9),
  POINT_DATA_NEED_REALLOC = (1 << 10),

  ATTR_FLOAT_NEEDS_REALLOC = (1 << 11),
  ATTR_FLOAT2_NEEDS_REALLOC = (1 << 12),
  ATTR_FLOAT3_NEEDS_REALLOC = (1 << 13),
  ATTR_FLOAT4_NEEDS_REALLOC = (1 << 14),

  ATTR_UCHAR4_NEEDS_REALLOC = (1 << 15),

  ATTRS_NEED_REALLOC = (ATTR_FLOAT_NEEDS_REALLOC | ATTR_FLOAT2_NEEDS_REALLOC |
                        ATTR_FLOAT3_NEEDS_REALLOC | ATTR_FLOAT4_NEEDS_REALLOC |
                        ATTR_UCHAR4_NEEDS_REALLOC),
  DEVICE_MESH_DATA_NEEDS_REALLOC = (MESH_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
  DEVICE_POINT_DATA_NEEDS_REALLOC = (POINT_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
  DEVICE_CURVE_DATA_NEEDS_REALLOC = (CURVE_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
};

/* Geometry
 *
 * Base class for geometric types like Mesh and Hair. */

class Geometry : public Node {
 public:
  NODE_ABSTRACT_DECLARE

  enum Type {
    MESH,
    HAIR,
    VOLUME,
    POINTCLOUD,
    LIGHT,
  };

  Type geometry_type;

  /* Attributes */
  AttributeSet attributes;

  /* Shaders */
  NODE_SOCKET_API_ARRAY(array<Node *>, used_shaders)

  /* Transform */
  BoundBox bounds;
  bool transform_applied;
  bool transform_negative_scaled;
  Transform transform_normal;

  /* Motion Blur */
  NODE_SOCKET_API(uint, motion_steps)
  NODE_SOCKET_API(bool, use_motion_blur)

  /* Maximum number of motion steps supported (due to Embree). */
  static const uint MAX_MOTION_STEPS = 129;

  /* BVH */
  unique_ptr<BVH> bvh;
  size_t attr_map_offset;
  size_t prim_offset;

  /* Shader Properties */
  bool has_volume;         /* Set in the device_update_flags(). */
  bool has_surface_bssrdf; /* Set in the device_update_flags(). */

  /* Update Flags */
  bool need_update_rebuild;
  bool need_update_bvh_for_offset;

  /* Index into scene->geometry (only valid during update) */
  size_t index;

  /* Constructor/Destructor */
  explicit Geometry(const NodeType *node_type, const Type type);
  ~Geometry() override;

  /* Geometry */
  virtual void clear(bool preserve_shaders = false);
  virtual void compute_bounds() = 0;
  virtual void apply_transform(const Transform &tfm, const bool apply_to_motion) = 0;

  /* Attribute Requests */
  bool need_attribute(Scene *scene, AttributeStandard std);
  bool need_attribute(Scene *scene, ustring name);

  AttributeRequestSet needed_attributes();

  /* UDIM */
  virtual void get_uv_tiles(ustring map, unordered_set<int> &tiles) = 0;

  /* Convert between normalized -1..1 motion time and index in the
   * VERTEX_MOTION attribute. */
  float motion_time(const int step) const;
  int motion_step(const float time) const;

  /* BVH */
  void compute_bvh(Device *device,
                   DeviceScene *dscene,
                   SceneParams *params,
                   Progress *progress,
                   const size_t n,
                   size_t total);

  virtual PrimitiveType primitive_type() const = 0;

  /* Check whether the geometry should have own BVH built separately. Briefly,
   * own BVH is needed for geometry, if:
   *
   * - It is instanced multiple times, so each instance object should share the
   *   same BVH tree.
   * - Special ray intersection is needed, for example to limit subsurface rays
   *   to only the geometry itself.
   * - The BVH layout requires the top level to only contain instances.
   */
  bool need_build_bvh(BVHLayout layout) const;

  /* Test if the geometry should be treated as instanced. */
  bool is_instanced() const;

  bool has_true_displacement() const;
  virtual bool has_motion_blur() const;
  bool has_voxel_attributes() const;

  bool is_mesh() const
  {
    return geometry_type == MESH;
  }

  bool is_hair() const
  {
    return geometry_type == HAIR;
  }

  bool is_pointcloud() const
  {
    return geometry_type == POINTCLOUD;
  }

  bool is_volume() const
  {
    return geometry_type == VOLUME;
  }

  bool is_light() const
  {
    return geometry_type == LIGHT;
  }

  /* Updates */
  void tag_update(Scene *scene, bool rebuild);
};

/* Geometry Manager */

class GeometryManager {
  uint32_t update_flags;

  /* Persistent task pool for BVH building, because the Embree scene creates its own
   * task group that has a parent pointer to this one. And if we create a task pool
   * on the stack, that becomes a dangling pointer. See #143662 for details. */
  TaskPool bvh_task_pool_;

 public:
  enum : uint32_t {
    UV_PASS_NEEDED = (1 << 0),
    MOTION_PASS_NEEDED = (1 << 1),
    GEOMETRY_MODIFIED = (1 << 2),
    OBJECT_MANAGER = (1 << 3),
    MESH_ADDED = (1 << 4),
    MESH_REMOVED = (1 << 5),
    HAIR_ADDED = (1 << 6),
    HAIR_REMOVED = (1 << 7),
    POINT_ADDED = (1 << 12),
    POINT_REMOVED = (1 << 13),

    SHADER_ATTRIBUTE_MODIFIED = (1 << 8),
    SHADER_DISPLACEMENT_MODIFIED = (1 << 9),

    GEOMETRY_ADDED = MESH_ADDED | HAIR_ADDED | POINT_ADDED,
    GEOMETRY_REMOVED = MESH_REMOVED | HAIR_REMOVED | POINT_REMOVED,

    TRANSFORM_MODIFIED = (1 << 10),

    VISIBILITY_MODIFIED = (1 << 11),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  /* Update Flags */
  bool need_flags_update;

  /* Constructor/Destructor */
  GeometryManager();
  ~GeometryManager();

  /* Device Updates */
  void device_update_preprocess(Device *device, Scene *scene, Progress &progress);
  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene, bool force_free);

  /* Updates */
  void tag_update(Scene *scene, const uint32_t flag);

  bool need_update() const;

  /* Statistics */
  void collect_statistics(const Scene *scene, RenderStats *stats);

 protected:
  bool displace(Device *device, Scene *scene, Mesh *mesh, Progress &progress);

  void create_volume_mesh(const Scene *scene, Volume *volume, Progress &progress);

  /* Attributes */
  void update_osl_globals(Device *device, Scene *scene);
  void update_svm_attributes(Device *device,
                             DeviceScene *dscene,
                             Scene *scene,
                             vector<AttributeRequestSet> &geom_attributes,
                             vector<AttributeRequestSet> &object_attributes);

  /* Compute verts/triangles/curves offsets in global arrays. */
  void geom_calc_offset(Scene *scene, BVHLayout bvh_layout);

  void device_update_object(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_mesh(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_attributes(Device *device,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress &progress);

  void device_update_bvh(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);

  void device_update_displacement_images(Device *device, Scene *scene, Progress &progress);

  void device_update_volume_images(Device *device, Scene *scene, Progress &progress);
};

CCL_NAMESPACE_END
