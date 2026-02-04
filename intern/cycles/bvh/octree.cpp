/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/octree.h"

#include "scene/object.h"
#include "scene/volume.h"

#include "integrator/shader_eval.h"

#include "util/log.h"
#include "util/progress.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/FindActiveValues.h>
#endif

#include <fstream>

CCL_NAMESPACE_BEGIN

__forceinline int Octree::flatten_index(int x, int y, int z) const
{
  return x + resolution_ * (y + z * resolution_);
}

Extrema<float> Octree::get_extrema(const int3 index_min, const int3 index_max) const
{
  const blocked_range3d<int> range(
      index_min.x, index_max.x, 32, index_min.y, index_max.y, 32, index_min.z, index_max.z, 32);

  const Extrema<float> identity = {FLT_MAX, -FLT_MAX};

  auto reduction_func = [&](const blocked_range3d<int> &r, Extrema<float> init) -> Extrema<float> {
    for (int z = r.cols().begin(); z < r.cols().end(); ++z) {
      for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
        for (int x = r.pages().begin(); x < r.pages().end(); ++x) {
          init = merge(init, sigmas_[flatten_index(x, y, z)]);
        }
      }
    }
    return init;
  };

  auto join_func = [](Extrema<float> a, Extrema<float> b) -> Extrema<float> {
    return merge(a, b);
  };

  return parallel_reduce(range, identity, reduction_func, join_func);
}

__forceinline float3 Octree::position_to_index(const float3 p) const
{
  return (p - bbox_min) * position_to_index_scale_;
}

int3 Octree::position_to_floor_index(const float3 p) const
{
  const float3 index = round(position_to_index(p));
  return clamp(make_int3(int(index.x), int(index.y), int(index.z)), 0, resolution_ - 1);
}

int3 Octree::position_to_ceil_index(const float3 p) const
{
  if (any_zero(position_to_index_scale_)) {
    /* Octree with degenerate shape, force max index. */
    return make_int3(resolution_);
  }
  const float3 index = round(position_to_index(p));
  return clamp(make_int3(int(index.x), int(index.y), int(index.z)), 1, resolution_);
}

__forceinline float3 Octree::index_to_position(int x, int y, int z) const
{
  return bbox_min + make_float3(x, y, z) * index_to_position_scale_;
}

__forceinline float3 Octree::voxel_size() const
{
  return index_to_position_scale_;
}

bool Octree::should_split(std::shared_ptr<OctreeNode> &node) const
{
  const int3 index_min = position_to_floor_index(node->bbox.min);
  const int3 index_max = position_to_ceil_index(node->bbox.max);
  node->sigma = get_extrema(index_min, index_max);

  const float3 bbox_size = node->bbox.size();
  if (any_zero(bbox_size)) {
    /* Octree with degenerate shape, can happen for implicit volume. */
    return false;
  }

  /* The threshold is set so that ideally only one sample needs to be taken per node. Value taken
   * from "Volume Rendering for Pixar's Elemental". */
  return (node->sigma.range() * len(bbox_size) * scale_ > 1.442f &&
          node->depth < VOLUME_OCTREE_MAX_DEPTH);
}

#ifdef WITH_OPENVDB
/* Check if a interior mask grid intersects with a bounding box defined by `p_min` and `p_max`. */
static bool vdb_voxel_intersect(const float3 p_min,
                                const float3 p_max,
                                openvdb::BoolGrid::ConstPtr &grid,
                                const openvdb::tools::FindActiveValues<openvdb::BoolTree> &find)
{
  if (grid->empty()) {
    /* Non-mesh volume or open mesh. */
    return true;
  }

  const openvdb::math::CoordBBox coord_bbox(
      openvdb::Coord::floor(grid->worldToIndex({p_min.x, p_min.y, p_min.z})),
      openvdb::Coord::ceil(grid->worldToIndex({p_max.x, p_max.y, p_max.z})));

  /* Check if the bounding box lies inside or partially overlaps the mesh.
   * For interior mask grids, all the interior voxels are active. */
  return find.anyActiveValues(coord_bbox, true);
}
#endif

/* Fill in coordinates for shading the volume density. */
static void fill_shader_input(device_vector<KernelShaderEvalInput> &d_input,
                              const Octree *octree,
                              const Object *object,
                              const Shader *shader,
#ifdef WITH_OPENVDB
                              openvdb::BoolGrid::ConstPtr &interior_mask,
#endif
                              const int resolution)
{
  const int object_id = object->get_device_index();
  const uint shader_id = shader->id;

  KernelShaderEvalInput *d_input_data = d_input.data();

  const float3 voxel_size = octree->voxel_size();
  /* Dilate the voxel in case we miss features at the boundary. */
  const float3 pad = 0.2f * voxel_size;
  const float3 padded_size = voxel_size + pad * 2.0f;

  const blocked_range3d<int> range(0, resolution, 8, 0, resolution, 8, 0, resolution, 8);
  parallel_for(range, [&](const blocked_range3d<int> &r) {
#ifdef WITH_OPENVDB
    /* One accessor per thread is important for cached access. */
    const auto find = openvdb::tools::FindActiveValues(interior_mask->tree());
#endif

    for (int z = r.cols().begin(); z < r.cols().end(); ++z) {
      for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
        for (int x = r.pages().begin(); x < r.pages().end(); ++x) {
          const int offset = octree->flatten_index(x, y, z);
          const float3 p = octree->index_to_position(x, y, z);

#ifdef WITH_OPENVDB
          /* Zero density for cells outside of the mesh. */
          if (!vdb_voxel_intersect(p, p + voxel_size, interior_mask, find)) {
            d_input_data[offset * 2].object = OBJECT_NONE;
            d_input_data[offset * 2 + 1].object = SHADER_NONE;
            continue;
          }
#endif

          KernelShaderEvalInput in;
          in.object = object_id;
          in.prim = __float_as_int(p.x - pad.x);
          in.u = p.y - pad.y;
          in.v = p.z - pad.z;
          d_input_data[offset * 2] = in;

          in.object = shader_id;
          in.prim = __float_as_int(padded_size.x);
          in.u = padded_size.y;
          in.v = padded_size.z;
          d_input_data[offset * 2 + 1] = in;
        }
      }
    }
  });
}

/* Read back the volume density. */
static void read_shader_output(const device_vector<float> &d_output,
                               const Octree *octree,
                               const int num_channels,
                               const int resolution,
                               vector<Extrema<float>> &sigmas)
{
  const float *d_output_data = d_output.data();
  const blocked_range3d<int> range(0, resolution, 32, 0, resolution, 32, 0, resolution, 32);

  parallel_for(range, [&](const blocked_range3d<int> &r) {
    for (int z = r.cols().begin(); z < r.cols().end(); ++z) {
      for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
        for (int x = r.pages().begin(); x < r.pages().end(); ++x) {
          const int index = octree->flatten_index(x, y, z);
          sigmas[index].min = d_output_data[index * num_channels + 0];
          sigmas[index].max = d_output_data[index * num_channels + 1];
        }
      }
    }
  });
}

void Octree::evaluate_volume_density(Device *device,
                                     Progress &progress,
#ifdef WITH_OPENVDB
                                     openvdb::BoolGrid::ConstPtr &interior_mask,
#endif
                                     const Object *object,
                                     const Shader *shader)
{
  /* For heterogeneous volume, the grid resolution is 2^max_depth in each 3D dimension;
   * for homogeneous volume, only one grid is needed. */
  resolution_ = VolumeManager::is_homogeneous_volume(object, shader) ?
                    1 :
                    power_of_2(VOLUME_OCTREE_MAX_DEPTH);
  index_to_position_scale_ = root_->bbox.size() / float(resolution_);
  position_to_index_scale_ = safe_divide(one_float3(), index_to_position_scale_);

  /* Initialize density field. */
  /* TODO(weizhen): maybe lower the resolution depending on the object size. */
  const int size = resolution_ * resolution_ * resolution_;
  sigmas_.resize(size);
  parallel_for(0, size, [&](int i) { sigmas_[i] = {0.0f, 0.0f}; });

  /* Min and max. */
  const int num_channels = 2;

  /* Need the size of two `KernelShaderEvalInput`s per voxel for evaluating the shader. */
  const int num_inputs = size * 2;

  /* Evaluate shader on device. */
  ShaderEval shader_eval(device, progress);
  shader_eval.eval(
      SHADER_EVAL_VOLUME_DENSITY,
      num_inputs,
      num_channels,
      [&](device_vector<KernelShaderEvalInput> &d_input) {
#ifdef WITH_OPENVDB
        fill_shader_input(d_input, this, object, shader, interior_mask, resolution_);
#else
        fill_shader_input(d_input, this, object, shader, resolution_);
#endif
        return size;
      },
      [&](device_vector<float> &d_output) {
        read_shader_output(d_output, this, num_channels, resolution_, sigmas_);
      });
}

float Octree::volume_scale(const Object *object) const
{
  const Geometry *geom = object->get_geometry();
  if (geom->is_volume()) {
    const Volume *volume = static_cast<const Volume *>(geom);
    if (volume->get_object_space()) {
      /* The density changes with object scale, we scale the density accordingly in the final
       * render. */
      if (volume->transform_applied) {
        const float3 unit = normalize(one_float3());
        return 1.0f / len(transform_direction(&object->get_tfm(), unit));
      }
    }
    else {
      /* The density does not change with object scale, we scale the node in the viewport to it's
       * true size. */
      if (!volume->transform_applied) {
        const float3 unit = normalize(one_float3());
        return len(transform_direction(&object->get_tfm(), unit));
      }
    }
  }
  else {
    /* TODO(weizhen): use the maximal scale of all instances. */
    if (!geom->transform_applied) {
      const float3 unit = normalize(one_float3());
      return len(transform_direction(&object->get_tfm(), unit));
    }
  }

  return 1.0f;
}

std::shared_ptr<OctreeInternalNode> Octree::make_internal(std::shared_ptr<OctreeNode> &node)
{
  num_nodes_ += 8;
  auto internal = std::make_shared<OctreeInternalNode>(*node);

  /* Create bounding boxes for children. */
  const float3 center = internal->bbox.center();
  for (int i = 0; i < 8; i++) {
    const float3 t = make_float3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
    const BoundBox bbox(mix(internal->bbox.min, center, t), mix(center, internal->bbox.max, t));
    internal->children_[i] = std::make_shared<OctreeNode>(bbox, internal->depth + 1);
  }

  return internal;
}

void Octree::recursive_build(std::shared_ptr<OctreeNode> &octree_node)
{
  if (!should_split(octree_node)) {
    return;
  }

  /* Make the current node an internal node. */
  auto internal = make_internal(octree_node);

  for (auto &child : internal->children_) {
    task_pool_.push([&] { recursive_build(child); });
  }

  octree_node = internal;
}

void Octree::flatten(KernelOctreeNode *knodes,
                     const int current_index,
                     const std::shared_ptr<OctreeNode> &node,
                     int &child_index) const
{
  KernelOctreeNode &knode = knodes[current_index];
  knode.sigma = node->sigma;

  if (auto internal_ptr = std::dynamic_pointer_cast<OctreeInternalNode>(node)) {
    knode.first_child = child_index;
    child_index += 8;
    /* Loop through all the children and flatten in breadth-first manner, so that children are
     * stored in contiguous indices. */
    for (int i = 0; i < 8; i++) {
      knodes[knode.first_child + i].parent = current_index;
      flatten(knodes, knode.first_child + i, internal_ptr->children_[i], child_index);
    }
  }
  else {
    knode.first_child = -1;
  }
}

void Octree::set_flattened(const bool flattened)
{
  is_flattened_ = flattened;
}

bool Octree::is_flattened() const
{
  return is_flattened_;
}

void Octree::build(Device *device,
                   Progress &progress,
#ifdef WITH_OPENVDB
                   openvdb::BoolGrid::ConstPtr &interior_mask,
#endif
                   const Object *object,
                   const Shader *shader)
{
  const char *name = object->get_asset_name().c_str();
  progress.set_substatus(string_printf("Evaluating density for %s", name));

#ifdef WITH_OPENVDB
  evaluate_volume_density(device, progress, interior_mask, object, shader);
#else
  evaluate_volume_density(device, progress, object, shader);
#endif
  if (progress.get_cancel()) {
    return;
  }

  progress.set_substatus(string_printf("Building octree for %s", name));

  scale_ = volume_scale(object);
  recursive_build(root_);

  task_pool_.wait_work();

  is_built_ = true;
  sigmas_.clear();
}

Octree::Octree(const BoundBox &bbox)
{
  bbox_min = bbox.min;
  root_ = std::make_shared<OctreeNode>(bbox, 0);
  is_built_ = false;
  is_flattened_ = false;
}

bool Octree::is_built() const
{
  return is_built_;
}

int Octree::get_num_nodes() const
{
  return num_nodes_;
}

std::shared_ptr<OctreeNode> Octree::get_root() const
{
  return root_;
}

void OctreeNode::visualize(std::string &str) const
{
  const auto *internal = dynamic_cast<const OctreeInternalNode *>(this);

  if (!internal) {
    /* Skip leaf nodes. */
    return;
  }

  /* Create three orthogonal faces for inner nodes. */
  const float3 mid = bbox.center();
  const float3 max = bbox.max;
  const float3 min = bbox.min;
  const std::string mid_x = to_string(mid.x), mid_y = to_string(mid.y), mid_z = to_string(mid.z),
                    min_x = to_string(min.x), min_y = to_string(min.y), min_z = to_string(min.z),
                    max_x = to_string(max.x), max_y = to_string(max.y), max_z = to_string(max.z);
  // clang-format off
  str += "(" + mid_x + "," + mid_y + "," + min_z + "), "
         "(" + mid_x + "," + mid_y + "," + max_z + "), "
         "(" + mid_x + "," + max_y + "," + max_z + "), "
         "(" + mid_x + "," + max_y + "," + min_z + "), "
         "(" + mid_x + "," + min_y + "," + min_z + "), "
         "(" + mid_x + "," + min_y + "," + max_z + "), ";
  str += "(" + min_x + "," + mid_y + "," + mid_z + "), "
         "(" + max_x + "," + mid_y + "," + mid_z + "), "
         "(" + max_x + "," + mid_y + "," + max_z + "), "
         "(" + min_x + "," + mid_y + "," + max_z + "), "
         "(" + min_x + "," + mid_y + "," + min_z + "), "
         "(" + max_x + "," + mid_y + "," + min_z + "), ";
  str += "(" + mid_x + "," + min_y + "," + mid_z + "), "
         "(" + mid_x + "," + max_y + "," + mid_z + "), "
         "(" + max_x + "," + max_y + "," + mid_z + "), "
         "(" + max_x + "," + min_y + "," + mid_z + "), "
         "(" + min_x + "," + min_y + "," + mid_z + "), "
         "(" + min_x + "," + max_y + "," + mid_z + "), ";
  // clang-format on
  for (const auto &child : internal->children_) {
    child->visualize(str);
  }
}

void Octree::visualize(std::ofstream &file, const std::string object_name) const
{
  std::string str = "vertices = [";
  root_->visualize(str);
  str +=
      "]\nr = range(len(vertices))\n"
      "edges = [(i, i+1 if i%6<5 else i-4) for i in r]\n"
      "mesh = bpy.data.meshes.new('Octree')\n"
      "mesh.from_pydata(vertices, edges, [])\n"
      "mesh.update()\n"
      "obj = bpy.data.objects.new('" +
      object_name +
      "', mesh)\n"
      "octree.objects.link(obj)\n"
      "bpy.context.view_layer.objects.active = obj\n"
      "bpy.ops.object.mode_set(mode='EDIT')\n";
  file << str;

  const float3 center = root_->bbox.center();
  const float3 size = root_->bbox.size() * 0.5f;
  file << "bpy.ops.mesh.primitive_cube_add(location = " << center << ", scale = " << size << ")\n";
  file << "bpy.ops.mesh.delete(type='ONLY_FACE')\n"
          "bpy.ops.object.mode_set(mode='OBJECT')\n"
          "obj.select_set(True)\n";
}

CCL_NAMESPACE_END
