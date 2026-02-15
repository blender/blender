/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/volume.h"
#include "scene/attribute.h"
#include "scene/background.h"
#include "scene/image_vdb.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/object.h"
#include "scene/scene.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/Morphology.h>
#endif

#include "util/hash.h"
#include "util/log.h"
#include "util/nanovdb.h"
#include "util/path.h"
#include "util/progress.h"
#include "util/types.h"

#include "bvh/octree.h"

#include <OpenImageIO/filesystem.h>

CCL_NAMESPACE_BEGIN

NODE_DEFINE(Volume)
{
  NodeType *type = NodeType::add("volume", create, NodeType::NONE, Mesh::get_node_type());

  SOCKET_FLOAT(step_size, "Step Size", 0.0f);
  SOCKET_BOOLEAN(object_space, "Object Space", false);
  SOCKET_FLOAT(velocity_scale, "Velocity Scale", 1.0f);

  return type;
}

Volume::Volume() : Mesh(get_node_type(), Geometry::VOLUME)
{
  step_size = 0.0f;
  object_space = false;
}

void Volume::clear(bool preserve_shaders)
{
  Mesh::clear(preserve_shaders, true);
}

struct QuadData {
  int v0, v1, v2, v3;

  float3 normal;
};

enum {
  QUAD_X_MIN = 0,
  QUAD_X_MAX = 1,
  QUAD_Y_MIN = 2,
  QUAD_Y_MAX = 3,
  QUAD_Z_MIN = 4,
  QUAD_Z_MAX = 5,
};

#if defined(WITH_OPENVDB) && defined(WITH_NANOVDB)
const int quads_indices[6][4] = {
    /* QUAD_X_MIN */
    {4, 0, 3, 7},
    /* QUAD_X_MAX */
    {1, 5, 6, 2},
    /* QUAD_Y_MIN */
    {4, 5, 1, 0},
    /* QUAD_Y_MAX */
    {3, 2, 6, 7},
    /* QUAD_Z_MIN */
    {0, 1, 2, 3},
    /* QUAD_Z_MAX */
    {5, 4, 7, 6},
};

const float3 quads_normals[6] = {
    /* QUAD_X_MIN */
    make_float3(-1.0f, 0.0f, 0.0f),
    /* QUAD_X_MAX */
    make_float3(1.0f, 0.0f, 0.0f),
    /* QUAD_Y_MIN */
    make_float3(0.0f, -1.0f, 0.0f),
    /* QUAD_Y_MAX */
    make_float3(0.0f, 1.0f, 0.0f),
    /* QUAD_Z_MIN */
    make_float3(0.0f, 0.0f, -1.0f),
    /* QUAD_Z_MAX */
    make_float3(0.0f, 0.0f, 1.0f),
};

static int add_vertex(const int3 v,
                      vector<int3> &vertices,
                      const int3 res,
                      unordered_map<size_t, int> &used_verts)
{
  const size_t vert_key = v.x + v.y * size_t(res.x + 1) +
                          v.z * size_t(res.x + 1) * size_t(res.y + 1);
  const unordered_map<size_t, int>::iterator it = used_verts.find(vert_key);

  if (it != used_verts.end()) {
    return it->second;
  }

  const int vertex_offset = vertices.size();
  used_verts[vert_key] = vertex_offset;
  vertices.push_back(v);
  return vertex_offset;
}

static void create_quad(const int3 corners[8],
                        vector<int3> &vertices,
                        vector<QuadData> &quads,
                        const int3 res,
                        unordered_map<size_t, int> &used_verts,
                        const int face_index)
{
  QuadData quad;
  quad.v0 = add_vertex(corners[quads_indices[face_index][0]], vertices, res, used_verts);
  quad.v1 = add_vertex(corners[quads_indices[face_index][1]], vertices, res, used_verts);
  quad.v2 = add_vertex(corners[quads_indices[face_index][2]], vertices, res, used_verts);
  quad.v3 = add_vertex(corners[quads_indices[face_index][3]], vertices, res, used_verts);
  quad.normal = quads_normals[face_index];

  quads.push_back(quad);
}

/* Create a mesh from a volume.
 *
 * The way the algorithm works is as follows:
 *
 * - The topologies of input OpenVDB grids are merged into a temporary grid.
 * - Voxels of the temporary grid are dilated to account for the padding necessary for volume
 * sampling.
 * - A bounding box of the temporary grid is created.
 */
class VolumeMeshBuilder {
 public:
  /* use a MaskGrid to store the topology to save memory */
  openvdb::MaskGrid::Ptr topology_grid;
  openvdb::CoordBBox bbox;
  bool first_grid;

  VolumeMeshBuilder();

  void add_grid(const nanovdb::GridHandle<> &grid);

  void add_padding(const int pad_size);

  void create_mesh(vector<float3> &vertices, vector<int> &indices, const bool ray_marching);

  void generate_vertices_and_quads(vector<int3> &vertices_is,
                                   vector<QuadData> &quads,
                                   const bool ray_marching);

  void convert_object_space(const vector<int3> &vertices, vector<float3> &out_vertices);

  void convert_quads_to_tris(const vector<QuadData> &quads, vector<int> &tris);

  bool empty_grid() const;
};

VolumeMeshBuilder::VolumeMeshBuilder()
{
  first_grid = true;
}

void VolumeMeshBuilder::add_grid(const nanovdb::GridHandle<> &nanogrid)
{
  /* set the transform of our grid from the first one */
  openvdb::MaskGrid::Ptr grid = nanovdb_to_openvdb_mask(nanogrid);

  if (first_grid) {
    topology_grid = grid;
    first_grid = false;
    return;
  }

  /* If the transforms do not match, we need to resample one of the grids so that
   * its index space registers with that of the other, here we resample our mask
   * grid so memory usage is kept low */
  if (topology_grid->transform() != grid->transform()) {
    const openvdb::MaskGrid::Ptr temp_grid = topology_grid->copyWithNewTree();
    temp_grid->setTransform(grid->transform().copy());
    openvdb::tools::resampleToMatch<openvdb::tools::BoxSampler>(*topology_grid, *temp_grid);
    topology_grid = temp_grid;
    topology_grid->setTransform(grid->transform().copy());
  }

  topology_grid->topologyUnion(*grid);
}

void VolumeMeshBuilder::add_padding(const int pad_size)
{
  openvdb::tools::dilateActiveValues(
      topology_grid->tree(), pad_size, openvdb::tools::NN_FACE, openvdb::tools::IGNORE_TILES);
}

void VolumeMeshBuilder::create_mesh(vector<float3> &vertices,
                                    vector<int> &indices,
                                    const bool ray_marching)
{
  /* We create vertices in index space (is), and only convert them to object
   * space when done. */
  vector<int3> vertices_is;
  vector<QuadData> quads;

  generate_vertices_and_quads(vertices_is, quads, ray_marching);

  convert_object_space(vertices_is, vertices);

  convert_quads_to_tris(quads, indices);
}

static bool is_non_empty_leaf(const openvdb::MaskGrid::TreeType &tree, const openvdb::Coord coord)
{
  const auto *leaf_node = tree.probeLeaf(coord);
  return (leaf_node && !leaf_node->isEmpty());
}

void VolumeMeshBuilder::generate_vertices_and_quads(vector<ccl::int3> &vertices_is,
                                                    vector<QuadData> &quads,
                                                    const bool ray_marching)
{
  if (ray_marching) {
    /* Make sure we only have leaf nodes in the tree, as tiles are not handled by this algorithm */
    topology_grid->tree().voxelizeActiveTiles();

    const openvdb::MaskGrid::TreeType &tree = topology_grid->tree();
    tree.evalLeafBoundingBox(bbox);

    const int3 resolution = make_int3(bbox.dim().x(), bbox.dim().y(), bbox.dim().z());

    unordered_map<size_t, int> used_verts;
    for (auto iter = tree.cbeginLeaf(); iter; ++iter) {
      if (iter->isEmpty()) {
        continue;
      }
      openvdb::CoordBBox leaf_bbox = iter->getNodeBoundingBox();
      /* +1 to convert from exclusive to include bounds. */
      leaf_bbox.max() = leaf_bbox.max().offsetBy(1);
      int3 min = make_int3(leaf_bbox.min().x(), leaf_bbox.min().y(), leaf_bbox.min().z());
      int3 max = make_int3(leaf_bbox.max().x(), leaf_bbox.max().y(), leaf_bbox.max().z());
      int3 corners[8] = {
          make_int3(min[0], min[1], min[2]),
          make_int3(max[0], min[1], min[2]),
          make_int3(max[0], max[1], min[2]),
          make_int3(min[0], max[1], min[2]),
          make_int3(min[0], min[1], max[2]),
          make_int3(max[0], min[1], max[2]),
          make_int3(max[0], max[1], max[2]),
          make_int3(min[0], max[1], max[2]),
      };
      /* Only create a quad if on the border between an active and an inactive leaf.
       *
       * We verify that a leaf exists by probing a coordinate that is at its center,
       * to do so we compute the center of the current leaf and offset this coordinate
       * by the size of a leaf in each direction.
       */
      static const int LEAF_DIM = openvdb::MaskGrid::TreeType::LeafNodeType::DIM;
      auto center = leaf_bbox.min() + openvdb::Coord(LEAF_DIM / 2);
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x() - LEAF_DIM, center.y(), center.z())))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MIN);
      }
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x() + LEAF_DIM, center.y(), center.z())))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MAX);
      }
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y() - LEAF_DIM, center.z())))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MIN);
      }
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y() + LEAF_DIM, center.z())))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MAX);
      }
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y(), center.z() - LEAF_DIM)))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MIN);
      }
      if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y(), center.z() + LEAF_DIM)))
      {
        create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MAX);
      }
    }
    return;
  }

  bbox = topology_grid->evalActiveVoxelBoundingBox();

  const int3 resolution = make_int3(bbox.dim().x(), bbox.dim().y(), bbox.dim().z());

  /* +1 to convert from exclusive to include bounds. */
  bbox.max() = bbox.max().offsetBy(1);

  int3 min = make_int3(bbox.min().x(), bbox.min().y(), bbox.min().z());
  int3 max = make_int3(bbox.max().x(), bbox.max().y(), bbox.max().z());

  int3 corners[8] = {
      make_int3(min[0], min[1], min[2]),
      make_int3(max[0], min[1], min[2]),
      make_int3(max[0], max[1], min[2]),
      make_int3(min[0], max[1], min[2]),
      make_int3(min[0], min[1], max[2]),
      make_int3(max[0], min[1], max[2]),
      make_int3(max[0], max[1], max[2]),
      make_int3(min[0], max[1], max[2]),
  };

  /* Create 6 quads of the bounding box. */
  unordered_map<size_t, int> used_verts;

  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MIN);
  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MAX);
  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MIN);
  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MAX);
  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MIN);
  create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MAX);
}

void VolumeMeshBuilder::convert_object_space(const vector<int3> &vertices,
                                             vector<float3> &out_vertices)
{
  out_vertices.reserve(vertices.size());

  for (size_t i = 0; i < vertices.size(); ++i) {
    openvdb::math::Vec3d p = topology_grid->indexToWorld(
        openvdb::math::Vec3d(vertices[i].x, vertices[i].y, vertices[i].z));
    const float3 vertex = make_float3((float)p.x(), (float)p.y(), (float)p.z());
    out_vertices.push_back(vertex);
  }
}

void VolumeMeshBuilder::convert_quads_to_tris(const vector<QuadData> &quads, vector<int> &tris)
{
  int index_offset = 0;
  tris.resize(quads.size() * 6);

  for (size_t i = 0; i < quads.size(); ++i) {
    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v2;
    tris[index_offset++] = quads[i].v1;

    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v3;
    tris[index_offset++] = quads[i].v2;
  }
}

bool VolumeMeshBuilder::empty_grid() const
{
  return !topology_grid ||
         (!topology_grid->tree().hasActiveTiles() && topology_grid->tree().leafCount() == 0);
}

/* -------------------------------------------------------------------- */
/* Compute the average and variance of active values in a nanovdb grid, separately in all
 * dimensions. Adapted from `nanovdb/tools/GridStats.h`.
 *
 * \{ */

struct Vec3Stats {
  double avg[3] = {0.0, 0.0, 0.0};
  double var[3] = {0.0, 0.0, 0.0};
  uint size = 0;

  /* Numerically stable way of computing online mean and variance, from Donald Knuth in “The Art
   * Of Computer Programming” (1998). */
  void add(const double value[3])
  {
    size++;
    for (int i = 0; i < 3; i++) {
      const double delta = value[i] - avg[i];
      avg[i] += delta / double(size);
      var[i] += delta * (value[i] - avg[i]);
    }
  }

  void add(const Vec3Stats &other)
  {
    if (other.size > 0) {
      const double denom = 1.0 / (double(size + other.size));
      for (int i = 0; i < 3; i++) {
        const double delta = other.avg[i] - avg[i];
        avg[i] += denom * delta * double(other.size);
        var[i] += other.var[i] + denom * delta * delta * double(size) * double(other.size);
      }
      size += other.size;
    }
  }

  void finalize()
  {
    if (size < 2) {
      var[0] = var[1] = var[2] = 0.0;
    }
    else {
      for (int i = 0; i < 3; i++) {
        var[i] /= double(size);
      }
    }
  }
};

template<typename ChildT> static Vec3Stats compute_stats(const nanovdb::LeafNode<ChildT> &leaf)
{
  Vec3Stats stats;
  for (auto value_it = leaf.cbeginValueOn(); value_it; ++value_it) {
    const double value[3] = {(*value_it)[0], (*value_it)[1], (*value_it)[2]};
    stats.add(value);
  }
  return stats;
}

template<typename ChildT> static Vec3Stats compute_stats(const nanovdb::InternalNode<ChildT> &node)
{
  const uint32_t num_leaf = node.mChildMask.countOn();

  std::unique_ptr<const ChildT *[]> childNodes(new const ChildT *[num_leaf]);
  const ChildT **ptr = childNodes.get();
  for (auto it = node.mChildMask.beginOn(); it; ++it) {
    *ptr++ = node.getChild(*it);
  }

  auto reduction_func = [&](const blocked_range<uint32_t> &r, Vec3Stats init) -> Vec3Stats {
    for (uint32_t i = r.begin(); i < r.end(); ++i) {
      init.add(compute_stats(*childNodes[i]));
    }
    return init;
  };

  auto join_func = [](Vec3Stats a, Vec3Stats b) -> Vec3Stats {
    a.add(b);
    return a;
  };

  const tbb::blocked_range<uint32_t> range(0, num_leaf);

  return parallel_reduce(range, Vec3Stats(), reduction_func, join_func);
}

/** \} */

static int estimate_required_velocity_padding(const nanovdb::GridHandle<> &grid,
                                              const float velocity_scale)
{
  const auto *typed_grid = grid.template grid<nanovdb::Vec3f>(0);

  if (typed_grid == nullptr) {
    return 0;
  }

  const nanovdb::Vec3d voxel_size = typed_grid->voxelSize();

  /* We should only have uniform grids, so x = y = z, but we never know. */
  const double max_voxel_size = openvdb::math::Max(voxel_size[0], voxel_size[1], voxel_size[2]);
  if (max_voxel_size == 0.0 || velocity_scale == 0.0f) {
    return 0;
  }

  Vec3Stats stats;
  for (auto internal = typed_grid->tree().root().cbeginChild(); internal; ++internal) {
    stats.add(compute_stats(*internal));
  }
  stats.finalize();

  /* A standard score of 2.32635 makes sure only 1% of the values are above `avg + score * std`. */
  const double score = 2.32635;
  double estimated_padding = 0.0f;
  for (int i = 0; i < 3; i++) {
    const double max_velocity = max(std::fabs(stats.avg[i] + score * sqrt(stats.var[i])),
                                    std::fabs(stats.avg[i] - score * sqrt(stats.var[i])));
    const double max_dist_in_voxel = max_velocity * double(velocity_scale) / voxel_size[i];

    /* Clamp padding to half of the volume size, and find the max padding in all 3 dimensions. */
    estimated_padding = max(
        min(max_dist_in_voxel, 0.5 * double(typed_grid->tree().bbox().dim()[i])),
        estimated_padding);
  }

  return static_cast<int>(std::ceil(estimated_padding));
}
#endif

#ifdef WITH_OPENVDB
static openvdb::FloatGrid::ConstPtr get_vdb_for_attribute(Volume *volume, AttributeStandard std)
{
  Attribute *attr = volume->attributes.find(std);
  if (!attr) {
    return nullptr;
  }

  const ImageHandle &handle = attr->data_voxel();
  VDBImageLoader *vdb_loader = handle.vdb_loader();
  if (!vdb_loader) {
    return nullptr;
  }

  const openvdb::GridBase::ConstPtr grid = vdb_loader->get_grid();
  if (!grid) {
    return nullptr;
  }

  if (!grid->isType<openvdb::FloatGrid>()) {
    return nullptr;
  }

  return openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid);
}

class MergeScalarGrids {
  using ScalarTree = openvdb::FloatTree;

  openvdb::tree::ValueAccessor<const ScalarTree> m_acc_x, m_acc_y, m_acc_z;

 public:
  MergeScalarGrids(const ScalarTree *x_tree, const ScalarTree *y_tree, const ScalarTree *z_tree)
      : m_acc_x(*x_tree), m_acc_y(*y_tree), m_acc_z(*z_tree)
  {
  }

  MergeScalarGrids(const MergeScalarGrids &other)

      = default;

  void operator()(const openvdb::Vec3STree::ValueOnIter &it) const
  {
    using namespace openvdb;

    const math::Coord xyz = it.getCoord();
    const float x = m_acc_x.getValue(xyz);
    const float y = m_acc_y.getValue(xyz);
    const float z = m_acc_z.getValue(xyz);

    it.setValue(math::Vec3s(x, y, z));
  }
};

static void merge_scalar_grids_for_velocity(const Scene *scene, Volume *volume)
{
  if (volume->attributes.find(ATTR_STD_VOLUME_VELOCITY)) {
    /* A vector grid for velocity is already available. */
    return;
  }

  const openvdb::FloatGrid::ConstPtr vel_x_grid = get_vdb_for_attribute(
      volume, ATTR_STD_VOLUME_VELOCITY_X);
  const openvdb::FloatGrid::ConstPtr vel_y_grid = get_vdb_for_attribute(
      volume, ATTR_STD_VOLUME_VELOCITY_Y);
  const openvdb::FloatGrid::ConstPtr vel_z_grid = get_vdb_for_attribute(
      volume, ATTR_STD_VOLUME_VELOCITY_Z);

  if (!(vel_x_grid && vel_y_grid && vel_z_grid)) {
    return;
  }

  const openvdb::Vec3fGrid::Ptr vecgrid = openvdb::Vec3SGrid::create(openvdb::Vec3s(0.0f));

  /* Activate voxels in the vector grid based on the scalar grids to ensure thread safety during
   * the merge. */
  vecgrid->tree().topologyUnion(vel_x_grid->tree());
  vecgrid->tree().topologyUnion(vel_y_grid->tree());
  vecgrid->tree().topologyUnion(vel_z_grid->tree());

  MergeScalarGrids op(&vel_x_grid->tree(), &vel_y_grid->tree(), &vel_z_grid->tree());
  openvdb::tools::foreach(vecgrid->beginValueOn(), op, true, false);

  /* Assume all grids have the same transformation. */
  const openvdb::math::Transform::Ptr transform = openvdb::ConstPtrCast<openvdb::math::Transform>(
      vel_x_grid->transformPtr());
  vecgrid->setTransform(transform);

  /* Make an attribute for it. */
  Attribute *attr = volume->attributes.add(ATTR_STD_VOLUME_VELOCITY);
  unique_ptr<ImageLoader> loader = make_unique<VDBImageLoader>(vecgrid, "merged_velocity");
  const ImageParams params;
  attr->data_voxel() = scene->image_manager->add_image(std::move(loader), params);
}
#endif /* defined(WITH_OPENVDB) && defined(WITH_NANOVDB) */

/* ************************************************************************** */

void GeometryManager::create_volume_mesh(const Scene *scene, Volume *volume, Progress &progress)
{
  const string msg = string_printf("Computing Volume Mesh %s", volume->name.c_str());
  progress.set_status("Updating Mesh", msg);

  /* Find shader and compute padding based on volume shader interpolation settings. */
  Shader *volume_shader = nullptr;
  int pad_size = 0;

  for (Node *node : volume->get_used_shaders()) {
    Shader *shader = static_cast<Shader *>(node);

    if (!shader->has_volume) {
      continue;
    }

    volume_shader = shader;

    if (shader->get_volume_interpolation_method() == VOLUME_INTERPOLATION_LINEAR) {
      pad_size = max(1, pad_size);
    }
    else if (shader->get_volume_interpolation_method() == VOLUME_INTERPOLATION_CUBIC) {
      pad_size = max(2, pad_size);
    }

    break;
  }

  /* Clear existing volume mesh, done here in case we early out due to
   * empty grid or missing volume shader.
   * Also keep the shaders to avoid infinite loops when synchronizing, as this will tag the shaders
   * as having changed. */
  volume->clear(true);
  volume->need_update_rebuild = true;

  if (!volume_shader) {
    return;
  }

#if defined(WITH_OPENVDB) && defined(WITH_NANOVDB)
  /* Create volume mesh builder. */
  VolumeMeshBuilder builder;

  for (Attribute &attr : volume->attributes.attributes) {
    if (attr.element != ATTR_ELEMENT_VOXEL) {
      continue;
    }

    ImageHandle &handle = attr.data_voxel();

    if (handle.empty()) {
      continue;
    }

    /* Create NanoVDB grid handle from image memory. */
    device_image *image = handle.vdb_image_memory();
    if (image == nullptr || image->host_pointer == nullptr ||
        image->info.data_type == IMAGE_DATA_TYPE_NANOVDB_EMPTY ||
        !is_nanovdb_type(image->info.data_type))
    {
      continue;
    }

    nanovdb::GridHandle grid(
        nanovdb::HostBuffer::createFull(image->memory_size(), image->host_pointer));

    /* Add padding based on the maximum velocity vector. */
    if (attr.std == ATTR_STD_VOLUME_VELOCITY && scene->need_motion() != Scene::MOTION_NONE) {
      pad_size = max(pad_size,
                     estimate_required_velocity_padding(grid, volume->get_velocity_scale()));
    }

    builder.add_grid(grid);
  }

  /* If nothing to build, early out. */
  if (builder.empty_grid()) {
    LOG_DEBUG << "Memory usage volume mesh: 0 Mb. (empty grid)";
    return;
  }

  builder.add_padding(pad_size);

  /* Create mesh. */
  vector<float3> vertices;
  vector<int> indices;
  const bool ray_marching = scene->integrator->get_volume_ray_marching();
  builder.create_mesh(vertices, indices, ray_marching);

  volume->reserve_mesh(vertices.size(), indices.size() / 3);
  volume->used_shaders.clear();
  volume->used_shaders.push_back_slow(volume_shader);

  for (size_t i = 0; i < vertices.size(); ++i) {
    volume->add_vertex(vertices[i]);
  }

  for (size_t i = 0; i < indices.size(); i += 3) {
    volume->add_triangle(indices[i], indices[i + 1], indices[i + 2], 0, false);
  }

  /* Print stats. */
  LOG_DEBUG << "Memory usage volume mesh: "
            << (vertices.size() * sizeof(float3) + indices.size() * sizeof(int)) /
                   (1024.0 * 1024.0)
            << "Mb.";
#else
  (void)scene;
#endif /* defined(WITH_OPENVDB) && defined(WITH_NANOVDB) */
}

void Volume::merge_grids(const Scene *scene)
{
#ifdef WITH_OPENVDB
  merge_scalar_grids_for_velocity(scene, this);
#else
  (void)scene;
#endif
}

VolumeManager::VolumeManager()
{
  need_rebuild_ = true;
  need_update_step_size = true;
}

void VolumeManager::tag_update()
{
  need_rebuild_ = true;
}

/* Remove changed object from the list of octrees and tag for rebuild. */
void VolumeManager::tag_update(const Object *object, uint32_t flag)
{
  if (object_octrees_.empty()) {
    /* Volume object is not in the octree, can happen when using ray marching. */
    return;
  }

  if (flag & ObjectManager::VISIBILITY_MODIFIED) {
    tag_update();
  }

  for (const Node *node : object->get_geometry()->get_used_shaders()) {
    const Shader *shader = static_cast<const Shader *>(node);
    if (shader->has_volume_spatial_varying || (flag & ObjectManager::OBJECT_REMOVED)) {
      /* TODO(weizhen): no need to update if the spatial variation is not in world space. */
      tag_update();
      object_octrees_.erase({object, shader});
    }
  }

  if (!need_rebuild_ && (flag & ObjectManager::TRANSFORM_MODIFIED)) {
    /* Octree is not tagged for rebuild, but the transformation changed, so a redraw is needed. */
    update_visualization_ = true;
  }
}

/* Remove object with changed shader from the list of octrees and tag for rebuild. */
void VolumeManager::tag_update(const Shader *shader)
{
  tag_update();
  for (auto it = object_octrees_.begin(); it != object_octrees_.end();) {
    if (it->first.second == shader) {
      it = object_octrees_.erase(it);
    }
    else {
      it++;
    }
  }
}

/* Remove object with changed geometry from the list of octrees and tag for rebuild. */
void VolumeManager::tag_update(const Geometry *geometry)
{
  tag_update();
  /* Tag Octree for update. */
  for (auto it = object_octrees_.begin(); it != object_octrees_.end();) {
    const Object *object = it->first.first;
    if (object->get_geometry() == geometry) {
      it = object_octrees_.erase(it);
    }
    else {
      it++;
    }
  }

#ifdef WITH_OPENVDB
  /* Tag VDB map for update. */
  for (auto it = vdb_map_.begin(); it != vdb_map_.end();) {
    if (it->first.first == geometry) {
      it = vdb_map_.erase(it);
    }
    else {
      it++;
    }
  }
#endif
}

void VolumeManager::tag_update_indices()
{
  update_root_indices_ = true;
}

void VolumeManager::tag_update_algorithm()
{
  need_rebuild_ = true;
  algorithm_modified_ = true;
}

bool VolumeManager::is_homogeneous_volume(const Object *object, const Shader *shader)
{
  if (!shader->has_volume || shader->has_volume_spatial_varying) {
    return false;
  }

  if (shader->has_volume_attribute_dependency) {
    for (Attribute &attr : object->get_geometry()->attributes.attributes) {
      /* If both the shader and the object needs volume attributes, the volume is heterogeneous. */
      if (attr.element == ATTR_ELEMENT_VOXEL) {
        return false;
      }
    }
  }

  return true;
}

#ifdef WITH_OPENVDB
/* Given a mesh, check if every edge has exactly two incident triangles, and if the two triangles
 * have the same orientation. */
static bool mesh_is_closed(const std::vector<openvdb::Vec3I> &triangles)
{
  const size_t num_triangles = triangles.size();
  if (num_triangles % 2) {
    return false;
  }

  /* Store the two vertices that forms an edge. */
  std::multiset<std::pair<int, int>> edges;
  int num_edges = 0;

  for (const auto &tri : triangles) {
    for (int i = 0; i < 3; i++) {
      const std::pair<int, int> e = {tri[i], tri[(i + 1) % 3]};
      if (edges.count(e)) {
        /* Same edge exists. */
        return false;
      }

      /* Check if an edge in the opposite order exists. */
      const auto count = edges.count({e.second, e.first});
      if (count > 1) {
        /* Edge has more than 2 incident faces. */
        return false;
      }
      if (count == 1) {
        /* If an edge in the opposite order exists, increment the count. */
        edges.insert({e.second, e.first});
      }
      else {
        /* Insert a new edge. */
        num_edges++;
        edges.insert(e);
      }
    }
  }

  /* Until this point, the count of each element in the set is at most 2; to check if they are
   * exactly 2, we just need to compare the total numbers. */
  return num_triangles * 3 == num_edges * 2;
}

openvdb::BoolGrid::ConstPtr VolumeManager::mesh_to_sdf_grid(const Mesh *mesh,
                                                            const Shader *shader,
                                                            const float half_width)
{
  const int num_verts = mesh->get_verts().size();
  std::vector<openvdb::Vec3f> points(num_verts);
  parallel_for(0, num_verts, [&](int i) {
    const float3 &vert = mesh->get_verts()[i];
    points[i] = openvdb::Vec3f(vert.x, vert.y, vert.z);
  });

  const int max_num_triangles = mesh->num_triangles();
  std::vector<openvdb::Vec3I> triangles;
  triangles.reserve(max_num_triangles);
  for (int i = 0; i < max_num_triangles; i++) {
    /* Only push triangles with matching shader. */
    const int shader_index = mesh->get_shader()[i];
    if (static_cast<const Shader *>(mesh->get_used_shaders()[shader_index]) == shader) {
      triangles.emplace_back(mesh->get_triangles()[i * 3],
                             mesh->get_triangles()[i * 3 + 1],
                             mesh->get_triangles()[i * 3 + 2]);
    }
  }

  if (!mesh_is_closed(triangles)) {
    /* `meshToLevelSet()` requires a closed mesh, otherwise we can not determine the interior of
     * the mesh. Evaluate the whole bounding box in this case. */
    return openvdb::BoolGrid::create();
  }

  /* TODO(weizhen): Should consider object instead of mesh size. */
  const float3 mesh_size = mesh->bounds.size();
  const auto vdb_voxel_size = openvdb::Vec3d(mesh_size.x, mesh_size.y, mesh_size.z) /
                              double(1 << VOLUME_OCTREE_MAX_DEPTH);

  auto xform = openvdb::math::Transform::createLinearTransform(1.0);
  xform->postScale(vdb_voxel_size);

  auto sdf_grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
      *xform, points, triangles, half_width);

  return openvdb::tools::sdfInteriorMask(*sdf_grid, 0.5 * vdb_voxel_size.length());
}

openvdb::BoolGrid::ConstPtr VolumeManager::get_vdb(const Geometry *geom,
                                                   const Shader *shader) const
{
  if (geom->is_mesh()) {
    if (auto it = vdb_map_.find({geom, shader}); it != vdb_map_.end()) {
      return it->second;
    }
  }
  /* Create empty grid. */
  return openvdb::BoolGrid::create();
}
#endif

void VolumeManager::initialize_octree(const Scene *scene, Progress &progress)
{
  /* Instanced objects without spatial variation can share one octree. */
  std::map<std::pair<const Geometry *, const Shader *>, std::shared_ptr<Octree>> geometry_octrees;
  for (const auto &it : object_octrees_) {
    const Shader *shader = it.first.second;
    if (!shader->has_volume_spatial_varying) {
      if (const Object *object = it.first.first) {
        geometry_octrees[{object->get_geometry(), shader}] = it.second;
      }
    }
  }

  /* Loop through the volume objects to initialize their root nodes. */
  for (const Object *object : scene->objects) {
    const Geometry *geom = object->get_geometry();
    if (!geom->has_volume) {
      continue;
    }

    /* Create Octree. */
    for (const Node *node : geom->get_used_shaders()) {
      const Shader *shader = static_cast<const Shader *>(node);
      if (!shader->has_volume) {
        continue;
      }

      if (object_octrees_.find({object, shader}) == object_octrees_.end()) {
        if (geom->is_light()) {
          const Light *light = static_cast<const Light *>(geom);
          if (light->get_light_type() == LIGHT_BACKGROUND) {
            /* World volume is unbounded, use some practical large number instead. */
            const float3 size = make_float3(10000.0f);
            object_octrees_[{object, shader}] = std::make_shared<Octree>(BoundBox(-size, size));
          }
        }
        else {
          const Mesh *mesh = static_cast<const Mesh *>(geom);
          if (is_zero(mesh->bounds.size())) {
            continue;
          }
          if (!shader->has_volume_spatial_varying) {
            /* TODO(weizhen): check object attribute. */
            if (auto it = geometry_octrees.find({geom, shader}); it != geometry_octrees.end()) {
              /* Share octree with other instances. */
              object_octrees_[{object, shader}] = it->second;
            }
            else {
              auto octree = std::make_shared<Octree>(mesh->bounds);
              geometry_octrees[{geom, shader}] = octree;
              object_octrees_[{object, shader}] = octree;
            }
          }
          else {
            /* TODO(weizhen): we can still share the octree if the spatial variation is in object
             * space, but that might be tricky to determine. */
            object_octrees_[{object, shader}] = std::make_shared<Octree>(mesh->bounds);
          }
        }
      }

#ifdef WITH_OPENVDB
      if (geom->is_mesh() && !VolumeManager::is_homogeneous_volume(object, shader) &&
          vdb_map_.find({geom, shader}) == vdb_map_.end())
      {
        const Mesh *mesh = static_cast<const Mesh *>(geom);
        const float3 dim = mesh->bounds.size();
        if (dim.x > 0.0f && dim.y > 0.0f && dim.z > 0.0f) {
          const char *name = object->get_asset_name().c_str();
          progress.set_substatus(string_printf("Creating SDF grid for %s", name));
          vdb_map_[{geom, shader}] = mesh_to_sdf_grid(mesh, shader, 1.0f);
        }
      }
#else
      (void)progress;
#endif
    }
  }
}

void VolumeManager::update_num_octree_nodes()
{
  num_octree_nodes_ = 0;
  num_octree_roots_ = 0;

  std::set<const Octree *> unique_octrees;
  for (const auto &it : object_octrees_) {
    const Octree *octree = it.second.get();
    if (unique_octrees.find(octree) != unique_octrees.end()) {
      continue;
    }

    unique_octrees.insert(octree);

    num_octree_roots_++;
    num_octree_nodes_ += octree->get_num_nodes();
  }
}

int VolumeManager::num_octree_nodes() const
{
  return num_octree_nodes_;
}

int VolumeManager::num_octree_roots() const
{
  return num_octree_roots_;
}

void VolumeManager::build_octree(Device *device, Progress &progress)
{
  const double start_time = time_dt();

  for (auto &it : object_octrees_) {
    if (it.second->is_built()) {
      continue;
    }

    const Object *object = it.first.first;
    const Shader *shader = it.first.second;
#ifdef WITH_OPENVDB
    openvdb::BoolGrid::ConstPtr interior_mask = get_vdb(object->get_geometry(), shader);
    it.second->build(device, progress, interior_mask, object, shader);
#else
    it.second->build(device, progress, object, shader);
#endif
  }

  update_num_octree_nodes();

  const double build_time = time_dt() - start_time;

  LOG_DEBUG << object_octrees_.size() << " volume octree(s) with a total of " << num_octree_nodes()
            << " nodes are built in " << build_time << " seconds.";
}

void VolumeManager::update_root_indices(DeviceScene *dscene, const Scene *scene) const
{
  if (object_octrees_.empty()) {
    return;
  }

  /* Keep track of the root index of the unique octrees. */
  std::map<const Octree *, int> octree_root_indices;

  int *roots = dscene->volume_tree_root_ids.alloc(scene->objects.size());

  int root_index = 0;
  for (const auto &it : object_octrees_) {
    const Object *object = it.first.first;
    const int object_id = object->get_device_index();
    const Octree *octree = it.second.get();
    auto entry = octree_root_indices.find(octree);
    if (entry == octree_root_indices.end()) {
      roots[object_id] = root_index;
      octree_root_indices[octree] = root_index;

      root_index++;
    }
    else {
      /* Instances share the same octree. */
      roots[object_id] = entry->second;
    }
  }

  dscene->volume_tree_root_ids.copy_to_device();
}

void VolumeManager::flatten_octree(DeviceScene *dscene, const Scene *scene) const
{
  if (object_octrees_.empty()) {
    return;
  }

  update_root_indices(dscene, scene);

  for (const auto &it : object_octrees_) {
    /* Octrees need to be re-flattened. */
    it.second->set_flattened(false);
  }

  KernelOctreeRoot *kroots = dscene->volume_tree_roots.alloc(num_octree_roots());
  KernelOctreeNode *knodes = dscene->volume_tree_nodes.alloc(num_octree_nodes());

  int node_index = 0;
  int root_index = 0;
  for (const auto &it : object_octrees_) {
    std::shared_ptr<Octree> octree = it.second;
    if (octree->is_flattened()) {
      continue;
    }

    /* If an object has multiple shaders, the root index is overwritten, so we also write the
     * shader id, and perform a linear search in the kernel to find the correct octree. */
    kroots[root_index].shader = it.first.second->id;
    kroots[root_index].id = node_index;

    /* Transform from object space into octree space. */
    auto root = octree->get_root();
    const float3 scale = 1.0f / root->bbox.size();
    kroots[root_index].scale = scale;
    kroots[root_index].translation = -root->bbox.min * scale + 1.0f;

    root_index++;

    /* Flatten octree. */
    const uint current_index = node_index++;
    knodes[current_index].parent = -1;
    octree->flatten(knodes, current_index, root, node_index);
    octree->set_flattened();
  }

  dscene->volume_tree_nodes.copy_to_device();
  dscene->volume_tree_roots.copy_to_device();

  LOG_DEBUG << "Memory usage of volume octrees: "
            << (dscene->volume_tree_nodes.size() * sizeof(KernelOctreeNode) +
                dscene->volume_tree_roots.size() * sizeof(KernelOctreeRoot) +
                dscene->volume_tree_root_ids.size() * sizeof(int)) /
                   (1024.0 * 1024.0)
            << "Mb.";
}

/* Dump octree as python script, enabled by `CYCLES_VOLUME_OCTREE_DUMP` environment variable. */
std::string VolumeManager::visualize_octree(const char *filename) const
{
  const std::string filename_full = path_join(OIIO::Filesystem::current_path(), filename);

  std::ofstream file(filename_full);
  if (file.is_open()) {
    std::ostringstream buffer;
    file << "# Visualize volume octree.\n\n"
            "import bpy\nimport mathutils\n\n"
            "if bpy.context.active_object:\n"
            "    bpy.context.active_object.select_set(False)\n\n"
            "octree = bpy.data.collections.new(name='Octree')\n"
            "bpy.context.scene.collection.children.link(octree)\n\n";

    for (const auto &it : object_octrees_) {
      /* Draw Octree. */
      const auto octree = it.second;
      const std::string object_name = it.first.first->get_asset_name().string();
      octree->visualize(file, object_name);

      /* Apply transform. */
      const Object *object = it.first.first;
      const Geometry *geom = object->get_geometry();
      if (!geom->is_light() && !geom->transform_applied) {
        const Transform t = object->get_tfm();
        file << "obj.matrix_world = mathutils.Matrix((" << t.x << ", " << t.y << ", " << t.z
             << ", (" << 0 << "," << 0 << "," << 0 << "," << 1 << ")))\n\n";
      }
    }

    file.close();
  }

  return filename_full;
}

void VolumeManager::update_step_size(const Scene *scene, DeviceScene *dscene, Progress &progress)
{
  assert(scene->integrator->get_volume_ray_marching());

  if (!need_update_step_size && !dscene->volume_step_size.is_modified() &&
      !scene->integrator->volume_step_rate_is_modified() && !algorithm_modified_)
  {
    return;
  }

  if (dscene->volume_step_size.need_realloc()) {
    dscene->volume_step_size.alloc(scene->objects.size());
  }

  float *volume_step_size = dscene->volume_step_size.data();

  for (const Object *object : scene->objects) {
    const Geometry *geom = object->get_geometry();
    if (!geom->has_volume) {
      continue;
    }

    volume_step_size[object->index] = scene->integrator->get_volume_step_rate() *
                                      object->compute_volume_step_size(progress);
  }

  dscene->volume_step_size.copy_to_device();
  dscene->volume_step_size.clear_modified();
  need_update_step_size = false;
}

void VolumeManager::device_update(Device *device,
                                  DeviceScene *dscene,
                                  const Scene *scene,
                                  Progress &progress)
{
  if (scene->integrator->get_volume_ray_marching()) {
    /* No need to update octree for ray marching. */
    if (algorithm_modified_) {
      dscene->volume_tree_nodes.free();
      dscene->volume_tree_roots.free();
      dscene->volume_tree_root_ids.free();
    }
    update_step_size(scene, dscene, progress);
    algorithm_modified_ = false;
    return;
  }

  if (need_rebuild_) {
    /* Data needed for volume shader evaluation. */
    device->const_copy_to("data", &dscene->data, sizeof(dscene->data));

    initialize_octree(scene, progress);
    build_octree(device, progress);
    flatten_octree(dscene, scene);

    update_visualization_ = true;
    need_rebuild_ = false;
    update_root_indices_ = false;
  }
  else if (update_root_indices_) {
    update_root_indices(dscene, scene);
    update_root_indices_ = false;
  }

  if (update_visualization_) {
    static const bool dump_octree = getenv("CYCLES_VOLUME_OCTREE_DUMP") != nullptr;
    if (dump_octree) {
      const std::string octree_path = visualize_octree("octree.py");
      LOG_INFO << "Octree visualization has been written to " << octree_path;
    }
    update_visualization_ = false;
  }

  if (algorithm_modified_) {
    dscene->volume_step_size.free();
    algorithm_modified_ = false;
  }
}

void VolumeManager::device_free(DeviceScene *dscene)
{
  dscene->volume_tree_nodes.free();
  dscene->volume_tree_roots.free();
  dscene->volume_tree_root_ids.free();
  dscene->volume_step_size.free();
}

VolumeManager::~VolumeManager()
{
#ifdef WITH_OPENVDB
  for (auto &it : vdb_map_) {
    it.second.reset();
  }
#endif
}

CCL_NAMESPACE_END
