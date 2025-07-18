/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/volume.h"
#include "scene/attribute.h"
#include "scene/image_vdb.h"
#include "scene/scene.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/Morphology.h>
#endif

#include "util/hash.h"
#include "util/log.h"
#include "util/nanovdb.h"
#include "util/progress.h"
#include "util/types.h"

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
 * - Quads are created on the boundary between active and inactive leaf nodes of the temporary
 * grid.
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

  void create_mesh(vector<float3> &vertices,
                   vector<int> &indices,
                   const float face_overlap_avoidance);

  void generate_vertices_and_quads(vector<int3> &vertices_is, vector<QuadData> &quads);

  void convert_object_space(const vector<int3> &vertices,
                            vector<float3> &out_vertices,
                            const float face_overlap_avoidance);

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
                                    const float face_overlap_avoidance)
{
  /* We create vertices in index space (is), and only convert them to object
   * space when done. */
  vector<int3> vertices_is;
  vector<QuadData> quads;

  /* make sure we only have leaf nodes in the tree, as tiles are not handled by
   * this algorithm */
  topology_grid->tree().voxelizeActiveTiles();

  generate_vertices_and_quads(vertices_is, quads);

  convert_object_space(vertices_is, vertices, face_overlap_avoidance);

  convert_quads_to_tris(quads, indices);
}

static bool is_non_empty_leaf(const openvdb::MaskGrid::TreeType &tree, const openvdb::Coord coord)
{
  const auto *leaf_node = tree.probeLeaf(coord);
  return (leaf_node && !leaf_node->isEmpty());
}

void VolumeMeshBuilder::generate_vertices_and_quads(vector<ccl::int3> &vertices_is,
                                                    vector<QuadData> &quads)
{
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

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x() - LEAF_DIM, center.y(), center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MIN);
    }

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x() + LEAF_DIM, center.y(), center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MAX);
    }

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y() - LEAF_DIM, center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MIN);
    }

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y() + LEAF_DIM, center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MAX);
    }

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y(), center.z() - LEAF_DIM))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MIN);
    }

    if (!is_non_empty_leaf(tree, openvdb::Coord(center.x(), center.y(), center.z() + LEAF_DIM))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MAX);
    }
  }
}

void VolumeMeshBuilder::convert_object_space(const vector<int3> &vertices,
                                             vector<float3> &out_vertices,
                                             const float face_overlap_avoidance)
{
  /* compute the offset for the face overlap avoidance */
  bbox = topology_grid->evalActiveVoxelBoundingBox();
  openvdb::Coord dim = bbox.dim();

  const float3 cell_size = make_float3(1.0f / dim.x(), 1.0f / dim.y(), 1.0f / dim.z());
  const float3 point_offset = cell_size * face_overlap_avoidance;

  out_vertices.reserve(vertices.size());

  for (size_t i = 0; i < vertices.size(); ++i) {
    openvdb::math::Vec3d p = topology_grid->indexToWorld(
        openvdb::math::Vec3d(vertices[i].x, vertices[i].y, vertices[i].z));
    const float3 vertex = make_float3((float)p.x(), (float)p.y(), (float)p.z());
    out_vertices.push_back(vertex + point_offset);
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
  if (max_voxel_size == 0.0) {
    return 0;
  }

  /* TODO: we may need to also find outliers and clamp them to avoid adding too much padding. */
  const nanovdb::Vec3f mn = typed_grid->tree().root().minimum();
  const nanovdb::Vec3f mx = typed_grid->tree().root().maximum();
  float max_value = 0.0f;
  max_value = max(max_value, fabsf(mx[0]));
  max_value = max(max_value, fabsf(mx[1]));
  max_value = max(max_value, fabsf(mx[2]));
  max_value = max(max_value, fabsf(mn[0]));
  max_value = max(max_value, fabsf(mn[1]));
  max_value = max(max_value, fabsf(mn[2]));

  const double estimated_padding = max_value * static_cast<double>(velocity_scale) /
                                   max_voxel_size;

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

    /* Create NanoVDB grid handle from texture memory. */
    device_texture *texture = handle.image_memory();
    if (texture == nullptr || texture->host_pointer == nullptr ||
        !is_nanovdb_type(texture->info.data_type))
    {
      continue;
    }

    nanovdb::GridHandle grid(
        nanovdb::HostBuffer::createFull(texture->memory_size(), texture->host_pointer));

    /* Add padding based on the maximum velocity vector. */
    if (attr.std == ATTR_STD_VOLUME_VELOCITY && scene->need_motion() != Scene::MOTION_NONE) {
      pad_size = max(pad_size,
                     estimate_required_velocity_padding(grid, volume->get_velocity_scale()));
    }

    builder.add_grid(grid);
  }

  /* If nothing to build, early out. */
  if (builder.empty_grid()) {
    LOG_WORK << "Memory usage volume mesh: 0 Mb. (empty grid)";
    return;
  }

  builder.add_padding(pad_size);

  /* Slightly offset vertex coordinates to avoid overlapping faces with other
   * volumes or meshes. The proper solution would be to improve intersection in
   * the kernel to support robust handling of multiple overlapping faces or use
   * an all-hit intersection similar to shadows. */
  const float face_overlap_avoidance = 0.1f *
                                       hash_uint_to_float(hash_string(volume->name.c_str()));

  /* Create mesh. */
  vector<float3> vertices;
  vector<int> indices;
  builder.create_mesh(vertices, indices, face_overlap_avoidance);

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
  LOG_WORK << "Memory usage volume mesh: "
           << (vertices.size() * sizeof(float3) + indices.size() * sizeof(int)) / (1024.0 * 1024.0)
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

CCL_NAMESPACE_END
