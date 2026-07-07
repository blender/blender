/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* The volume octree is used to determine the necessary step size when rendering the volume. One
 * volume per object per shader is built, and a node splits in eight when the density difference
 * inside the node exceeds a certain threshold. */

#ifndef __OCTREE_H__
#define __OCTREE_H__

#include "util/boundbox.h"
#include "util/task.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

#include <atomic>
#include <iosfwd>

CCL_NAMESPACE_BEGIN

class Device;
class Object;
class Progress;
class Shader;
struct KernelOctreeNode;

struct OctreeNode {
  /* Bounding box of the node. */
  BoundBox bbox;

  /* Depth of the node in the octree. */
  int depth;

  /* Minimal and maximal volume density inside the node. */
  Extrema<float> sigma = {0.0f, 0.0f};

  OctreeNode() : bbox(BoundBox::empty), depth(0) {}
  OctreeNode(BoundBox bbox_, int depth_) : bbox(bbox_), depth(depth_) {}
  virtual ~OctreeNode() = default;

  /* Visualize node. */
  void visualize(std::string &str) const;
};

struct OctreeInternalNode : public OctreeNode {
  OctreeInternalNode(OctreeNode &node) : children_(8)
  {
    bbox = node.bbox;
    depth = node.depth;
    sigma = node.sigma;
  }

  vector<std::shared_ptr<OctreeNode>> children_;
};

class Octree {
 public:
  Octree(const BoundBox &bbox);
  ~Octree() = default;

  /* Build the octree according to the volume density. */
#ifdef WITH_OPENVDB
  void build(Device *, Progress &, openvdb::BoolGrid::ConstPtr &, const Object *, const Shader *);
#else
  void build(Device *, Progress &, const Object *, const Shader *);
#endif

  /* Convert the octree into an array of nodes for uploading to the kernel. */
  void flatten(KernelOctreeNode *, const int, const std::shared_ptr<OctreeNode> &, int &) const;
  void set_flattened(const bool = true);
  bool is_flattened() const;

  /* Flatten a 3D coordinate in the grid to a 1D index. */
  int flatten_index(int x, int y, int z) const;
  /* Convert from index to the position of the lower left corner of the voxel. */
  float3 index_to_position(int x, int y, int z) const;
  /* Size of a voxel. */
  float3 voxel_size() const;

  int get_num_nodes() const;
  std::shared_ptr<OctreeNode> get_root() const;
  bool is_built() const;

  /* Draw octree nodes as empty boxes with Blender Python API. */
  void visualize(std::ofstream &file, const std::string object_name) const;

 private:
  /* The bounding box of the octree is divided into a regular grid with the same resolution in each
   * dimension. */
  int resolution_;
  /* Extrema of volume densities in the grid. */
  vector<Extrema<float>> sigmas_;
  /* Compute the extrema of all the `sigmas_` in a coordinate bounding box defined by `index_min`
   * and `index_max`. */
  Extrema<float> get_extrema(const int3 index_min, const int3 index_max) const;
  /* Randomly sample positions inside the grid to evaluate the shader for the density. */
#ifdef WITH_OPENVDB
  void evaluate_volume_density(
      Device *, Progress &, openvdb::BoolGrid::ConstPtr &, const Object *, const Shader *);
#else
  void evaluate_volume_density(Device *, Progress &, const Object *, const Shader *);
#endif
  /* Convert from position in object space to grid index space. */
  float3 position_to_index_scale_;
  float3 index_to_position_scale_;
  float3 position_to_index(const float3 p) const;
  int3 position_to_floor_index(const float3 p) const;
  int3 position_to_ceil_index(const float3 p) const;

  /* Whether a node should be split into child nodes. */
  bool should_split(std::shared_ptr<OctreeNode> &node) const;
  /* Scale the node size so that the octree has the similar subdivision levels in viewport and
   * final render. */
  float volume_scale(const Object *object) const;
  float scale_;
  /* Recursively build a node and its child nodes. */
  void recursive_build(std::shared_ptr<OctreeNode> &);
  /* Turn a node into an internal node. */
  std::shared_ptr<OctreeInternalNode> make_internal(std::shared_ptr<OctreeNode> &node);

  /* Root node. */
  std::shared_ptr<OctreeNode> root_;

  /* Bounding box min of the octree, used for computing the indices. */
  float3 bbox_min;

  /* Whether the octree is already built. */
  bool is_built_;

  /* Whether the octree is already flattened into an array. */
  bool is_flattened_;

  /* Number of nodes in the octree. Incremented while building the tree.  */
  std::atomic<int> num_nodes_ = 1;

  /* Task pool for building the octree in parallel. */
  TaskPool task_pool_;
};

CCL_NAMESPACE_END

#endif /* __OCTREE_H__ */
