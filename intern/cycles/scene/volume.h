/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"

#include "scene/mesh.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

CCL_NAMESPACE_BEGIN

class Object;
class Octree;

class Volume : public Mesh {
 public:
  NODE_DECLARE

  Volume();

  NODE_SOCKET_API(float, step_size)
  NODE_SOCKET_API(bool, object_space)
  NODE_SOCKET_API(float, velocity_scale)

  /* Merge attributes for efficiency, call right after creating them. */
  void merge_grids(const Scene *scene);

  void clear(bool preserve_shaders = false) override;
};

class VolumeManager {
 public:
  VolumeManager();
  ~VolumeManager();

  void device_update(Device *, DeviceScene *, const Scene *, Progress &);
  void device_free(DeviceScene *);

  /* Tag volume octree for update when scene changes. */
  void tag_update();
  void tag_update(const Shader *shader);
  void tag_update(const Object *object, const uint32_t flag);
  void tag_update(const Geometry *geometry);
  void tag_update_indices();
  void tag_update_algorithm();

  /* Check whether the shader is a homogeneous volume. */
  static bool is_homogeneous_volume(const Object *, const Shader *);

  bool need_update_step_size;

 private:
  /* Initialize octrees from the volumes in the scene. */
  void initialize_octree(const Scene *, Progress &);

  /* Build octrees based on the volume density. */
  void build_octree(Device *, Progress &);

  /* Update the object and shader index of octree root nodes. */
  void update_root_indices(DeviceScene *, const Scene *) const;

  /* Converting the octrees into an array for uploading to the kernel. */
  void flatten_octree(DeviceScene *, const Scene *) const;

  /* Count all the nodes of the octrees. */
  void update_num_octree_nodes();
  int num_octree_nodes() const;
  int num_octree_roots() const;

  /* When running Blender with environment variable `CYCLES_VOLUME_OCTREE_DUMP`, an octree
   * visualization is written to `filename`, which is a Python script that can be run inside
   * Blender. */
  std::string visualize_octree(const char *filename) const;

  /* Step size for ray marching. */
  void update_step_size(const Scene *, DeviceScene *, Progress &progress);

  /* One octree per object per shader. */
  std::map<std::pair<const Object *, const Shader *>, std::shared_ptr<Octree>> object_octrees_;

  /* TODO(weizhen): replace booleans with enum `update_flags`? */
  bool update_root_indices_ = false;
  bool need_rebuild_;
  bool update_visualization_ = false;
  bool algorithm_modified_ = true;

  int num_octree_nodes_;
  int num_octree_roots_;

#ifdef WITH_OPENVDB
  /* Create SDF grid for mesh volumes, to determine whether a certain point is in the
   * interior of the mesh. This reduces evaluation time needed for heterogeneous volume. */
  openvdb::BoolGrid::ConstPtr mesh_to_sdf_grid(const Mesh *mesh,
                                               const Shader *shader,
                                               const float half_width);
  openvdb::BoolGrid::ConstPtr get_vdb(const Geometry *, const Shader *) const;
  std::map<std::pair<const Geometry *, const Shader *>, openvdb::BoolGrid::ConstPtr> vdb_map_;
#endif
};

CCL_NAMESPACE_END
