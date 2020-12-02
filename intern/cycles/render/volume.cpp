/*
 * Copyright 2020 Blender Foundation
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

#include "render/volume.h"
#include "render/attribute.h"
#include "render/image_vdb.h"
#include "render/scene.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Dense.h>
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/Morphology.h>
#endif

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_openvdb.h"
#include "util/util_progress.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

NODE_DEFINE(Volume)
{
  NodeType *type = NodeType::add("volume", create, NodeType::NONE, Mesh::node_type);

  SOCKET_FLOAT(clipping, "Clipping", 0.001f);
  SOCKET_FLOAT(step_size, "Step Size", 0.0f);
  SOCKET_BOOLEAN(object_space, "Object Space", false);

  return type;
}

Volume::Volume() : Mesh(node_type, Geometry::VOLUME)
{
  clipping = 0.001f;
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

#ifdef WITH_OPENVDB
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

static int add_vertex(int3 v,
                      vector<int3> &vertices,
                      int3 res,
                      unordered_map<size_t, int> &used_verts)
{
  size_t vert_key = v.x + v.y * (res.x + 1) + v.z * (res.x + 1) * (res.y + 1);
  unordered_map<size_t, int>::iterator it = used_verts.find(vert_key);

  if (it != used_verts.end()) {
    return it->second;
  }

  int vertex_offset = vertices.size();
  used_verts[vert_key] = vertex_offset;
  vertices.push_back(v);
  return vertex_offset;
}

static void create_quad(int3 corners[8],
                        vector<int3> &vertices,
                        vector<QuadData> &quads,
                        int3 res,
                        unordered_map<size_t, int> &used_verts,
                        int face_index)
{
  QuadData quad;
  quad.v0 = add_vertex(corners[quads_indices[face_index][0]], vertices, res, used_verts);
  quad.v1 = add_vertex(corners[quads_indices[face_index][1]], vertices, res, used_verts);
  quad.v2 = add_vertex(corners[quads_indices[face_index][2]], vertices, res, used_verts);
  quad.v3 = add_vertex(corners[quads_indices[face_index][3]], vertices, res, used_verts);
  quad.normal = quads_normals[face_index];

  quads.push_back(quad);
}
#endif

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
#ifdef WITH_OPENVDB
  /* use a MaskGrid to store the topology to save memory */
  openvdb::MaskGrid::Ptr topology_grid;
  openvdb::CoordBBox bbox;
#endif
  bool first_grid;

  VolumeMeshBuilder();

#ifdef WITH_OPENVDB
  void add_grid(openvdb::GridBase::ConstPtr grid, bool do_clipping, float volume_clipping);
#endif

  void add_padding(int pad_size);

  void create_mesh(vector<float3> &vertices,
                   vector<int> &indices,
                   vector<float3> &face_normals,
                   const float face_overlap_avoidance);

  void generate_vertices_and_quads(vector<int3> &vertices_is, vector<QuadData> &quads);

  void convert_object_space(const vector<int3> &vertices,
                            vector<float3> &out_vertices,
                            const float face_overlap_avoidance);

  void convert_quads_to_tris(const vector<QuadData> &quads,
                             vector<int> &tris,
                             vector<float3> &face_normals);

  bool empty_grid() const;

#ifdef WITH_OPENVDB
  template<typename GridType>
  void merge_grid(openvdb::GridBase::ConstPtr grid, bool do_clipping, float volume_clipping)
  {
    typename GridType::ConstPtr typed_grid = openvdb::gridConstPtrCast<GridType>(grid);

    if (do_clipping) {
      using ValueType = typename GridType::ValueType;
      typename GridType::Ptr copy = typed_grid->deepCopy();
      typename GridType::ValueOnIter iter = copy->beginValueOn();

      for (; iter; ++iter) {
        if (iter.getValue() < ValueType(volume_clipping)) {
          iter.setValueOff();
        }
      }

      typed_grid = copy;
    }

    topology_grid->topologyUnion(*typed_grid);
  }
#endif
};

VolumeMeshBuilder::VolumeMeshBuilder()
{
  first_grid = true;
}

#ifdef WITH_OPENVDB
void VolumeMeshBuilder::add_grid(openvdb::GridBase::ConstPtr grid,
                                 bool do_clipping,
                                 float volume_clipping)
{
  /* set the transform of our grid from the first one */
  if (first_grid) {
    topology_grid = openvdb::MaskGrid::create();
    topology_grid->setTransform(grid->transform().copy());
    first_grid = false;
  }
  /* if the transforms do not match, we need to resample one of the grids so that
   * its index space registers with that of the other, here we resample our mask
   * grid so memory usage is kept low */
  else if (topology_grid->transform() != grid->transform()) {
    openvdb::MaskGrid::Ptr temp_grid = topology_grid->copyWithNewTree();
    temp_grid->setTransform(grid->transform().copy());
    openvdb::tools::resampleToMatch<openvdb::tools::BoxSampler>(*topology_grid, *temp_grid);
    topology_grid = temp_grid;
    topology_grid->setTransform(grid->transform().copy());
  }

  if (grid->isType<openvdb::FloatGrid>()) {
    merge_grid<openvdb::FloatGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Vec3fGrid>()) {
    merge_grid<openvdb::Vec3fGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Vec4fGrid>()) {
    merge_grid<openvdb::Vec4fGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::BoolGrid>()) {
    merge_grid<openvdb::BoolGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::DoubleGrid>()) {
    merge_grid<openvdb::DoubleGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Int32Grid>()) {
    merge_grid<openvdb::Int32Grid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Int64Grid>()) {
    merge_grid<openvdb::Int64Grid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Vec3IGrid>()) {
    merge_grid<openvdb::Vec3IGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::Vec3dGrid>()) {
    merge_grid<openvdb::Vec3dGrid>(grid, do_clipping, volume_clipping);
  }
  else if (grid->isType<openvdb::MaskGrid>()) {
    topology_grid->topologyUnion(*openvdb::gridConstPtrCast<openvdb::MaskGrid>(grid));
  }
}
#endif

void VolumeMeshBuilder::add_padding(int pad_size)
{
#ifdef WITH_OPENVDB
  openvdb::tools::dilateVoxels(topology_grid->tree(), pad_size);
#else
  (void)pad_size;
#endif
}

void VolumeMeshBuilder::create_mesh(vector<float3> &vertices,
                                    vector<int> &indices,
                                    vector<float3> &face_normals,
                                    const float face_overlap_avoidance)
{
#ifdef WITH_OPENVDB
  /* We create vertices in index space (is), and only convert them to object
   * space when done. */
  vector<int3> vertices_is;
  vector<QuadData> quads;

  /* make sure we only have leaf nodes in the tree, as tiles are not handled by
   * this algorithm */
  topology_grid->tree().voxelizeActiveTiles();

  generate_vertices_and_quads(vertices_is, quads);

  convert_object_space(vertices_is, vertices, face_overlap_avoidance);

  convert_quads_to_tris(quads, indices, face_normals);
#else
  (void)vertices;
  (void)indices;
  (void)face_normals;
  (void)face_overlap_avoidance;
#endif
}

void VolumeMeshBuilder::generate_vertices_and_quads(vector<ccl::int3> &vertices_is,
                                                    vector<QuadData> &quads)
{
#ifdef WITH_OPENVDB
  const openvdb::MaskGrid::TreeType &tree = topology_grid->tree();
  tree.evalLeafBoundingBox(bbox);

  const int3 resolution = make_int3(bbox.dim().x(), bbox.dim().y(), bbox.dim().z());

  unordered_map<size_t, int> used_verts;

  for (auto iter = tree.cbeginLeaf(); iter; ++iter) {
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

    if (!tree.probeLeaf(openvdb::Coord(center.x() - LEAF_DIM, center.y(), center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MIN);
    }

    if (!tree.probeLeaf(openvdb::Coord(center.x() + LEAF_DIM, center.y(), center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_X_MAX);
    }

    if (!tree.probeLeaf(openvdb::Coord(center.x(), center.y() - LEAF_DIM, center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MIN);
    }

    if (!tree.probeLeaf(openvdb::Coord(center.x(), center.y() + LEAF_DIM, center.z()))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Y_MAX);
    }

    if (!tree.probeLeaf(openvdb::Coord(center.x(), center.y(), center.z() - LEAF_DIM))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MIN);
    }

    if (!tree.probeLeaf(openvdb::Coord(center.x(), center.y(), center.z() + LEAF_DIM))) {
      create_quad(corners, vertices_is, quads, resolution, used_verts, QUAD_Z_MAX);
    }
  }
#else
  (void)vertices_is;
  (void)quads;
#endif
}

void VolumeMeshBuilder::convert_object_space(const vector<int3> &vertices,
                                             vector<float3> &out_vertices,
                                             const float face_overlap_avoidance)
{
#ifdef WITH_OPENVDB
  /* compute the offset for the face overlap avoidance */
  bbox = topology_grid->evalActiveVoxelBoundingBox();
  openvdb::Coord dim = bbox.dim();

  float3 cell_size = make_float3(1.0f / dim.x(), 1.0f / dim.y(), 1.0f / dim.z());
  float3 point_offset = cell_size * face_overlap_avoidance;

  out_vertices.reserve(vertices.size());

  for (size_t i = 0; i < vertices.size(); ++i) {
    openvdb::math::Vec3d p = topology_grid->indexToWorld(
        openvdb::math::Vec3d(vertices[i].x, vertices[i].y, vertices[i].z));
    float3 vertex = make_float3((float)p.x(), (float)p.y(), (float)p.z());
    out_vertices.push_back(vertex + point_offset);
  }
#else
  (void)vertices;
  (void)out_vertices;
  (void)face_overlap_avoidance;
#endif
}

void VolumeMeshBuilder::convert_quads_to_tris(const vector<QuadData> &quads,
                                              vector<int> &tris,
                                              vector<float3> &face_normals)
{
  int index_offset = 0;
  tris.resize(quads.size() * 6);
  face_normals.reserve(quads.size() * 2);

  for (size_t i = 0; i < quads.size(); ++i) {
    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v2;
    tris[index_offset++] = quads[i].v1;

    face_normals.push_back(quads[i].normal);

    tris[index_offset++] = quads[i].v0;
    tris[index_offset++] = quads[i].v3;
    tris[index_offset++] = quads[i].v2;

    face_normals.push_back(quads[i].normal);
  }
}

bool VolumeMeshBuilder::empty_grid() const
{
#ifdef WITH_OPENVDB
  return !topology_grid || topology_grid->tree().leafCount() == 0;
#else
  return true;
#endif
}

#ifdef WITH_OPENVDB
template<typename GridType>
static openvdb::GridBase::ConstPtr openvdb_grid_from_device_texture(device_texture *image_memory,
                                                                    float volume_clipping,
                                                                    Transform transform_3d)
{
  using ValueType = typename GridType::ValueType;

  openvdb::CoordBBox dense_bbox(0,
                                0,
                                0,
                                image_memory->data_width - 1,
                                image_memory->data_height - 1,
                                image_memory->data_depth - 1);

  typename GridType::Ptr sparse = GridType::create(ValueType(0.0f));
  if (dense_bbox.empty()) {
    return sparse;
  }

  openvdb::tools::Dense<ValueType, openvdb::tools::MemoryLayout::LayoutXYZ> dense(
      dense_bbox, static_cast<ValueType *>(image_memory->host_pointer));

  openvdb::tools::copyFromDense(dense, *sparse, ValueType(volume_clipping));

  /* #copyFromDense will remove any leaf node that contains constant data and replace it with a
   * tile, however, we need to preserve the leaves in order to generate the mesh, so re-voxelize
   * the leaves that were pruned. This should not affect areas that were skipped due to the
   * volume_clipping parameter. */
  sparse->tree().voxelizeActiveTiles();

  /* Compute index to world matrix. */
  float3 voxel_size = make_float3(1.0f / image_memory->data_width,
                                  1.0f / image_memory->data_height,
                                  1.0f / image_memory->data_depth);

  transform_3d = transform_inverse(transform_3d);

  openvdb::Mat4R index_to_world_mat((double)(voxel_size.x * transform_3d[0][0]),
                                    0.0,
                                    0.0,
                                    0.0,
                                    0.0,
                                    (double)(voxel_size.y * transform_3d[1][1]),
                                    0.0,
                                    0.0,
                                    0.0,
                                    0.0,
                                    (double)(voxel_size.z * transform_3d[2][2]),
                                    0.0,
                                    (double)transform_3d[0][3],
                                    (double)transform_3d[1][3],
                                    (double)transform_3d[2][3],
                                    1.0);

  openvdb::math::Transform::Ptr index_to_world_tfm =
      openvdb::math::Transform::createLinearTransform(index_to_world_mat);

  sparse->setTransform(index_to_world_tfm);

  return sparse;
}
#endif

/* ************************************************************************** */

void GeometryManager::create_volume_mesh(Volume *volume, Progress &progress)
{
  string msg = string_printf("Computing Volume Mesh %s", volume->name.c_str());
  progress.set_status("Updating Mesh", msg);

  /* Find shader and compute padding based on volume shader interpolation settings. */
  Shader *volume_shader = NULL;
  int pad_size = 0;

  foreach (Node *node, volume->get_used_shaders()) {
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

  /* Create volume mesh builder. */
  VolumeMeshBuilder builder;

#ifdef WITH_OPENVDB
  foreach (Attribute &attr, volume->attributes.attributes) {
    if (attr.element != ATTR_ELEMENT_VOXEL) {
      continue;
    }

    bool do_clipping = false;

    ImageHandle &handle = attr.data_voxel();

    /* Try building from OpenVDB grid directly. */
    VDBImageLoader *vdb_loader = handle.vdb_loader();
    openvdb::GridBase::ConstPtr grid;
    if (vdb_loader) {
      grid = vdb_loader->get_grid();

      /* If building from an OpenVDB grid, we need to manually clip the values. */
      do_clipping = true;
    }

    /* Else fall back to creating an OpenVDB grid from the dense volume data. */
    if (!grid) {
      device_texture *image_memory = handle.image_memory();

      if (image_memory->data_elements == 1) {
        grid = openvdb_grid_from_device_texture<openvdb::FloatGrid>(
            image_memory, volume->get_clipping(), handle.metadata().transform_3d);
      }
      else if (image_memory->data_elements == 3) {
        grid = openvdb_grid_from_device_texture<openvdb::Vec3fGrid>(
            image_memory, volume->get_clipping(), handle.metadata().transform_3d);
      }
      else if (image_memory->data_elements == 4) {
        grid = openvdb_grid_from_device_texture<openvdb::Vec4fGrid>(
            image_memory, volume->get_clipping(), handle.metadata().transform_3d);
      }
    }

    if (grid) {
      builder.add_grid(grid, do_clipping, volume->get_clipping());
    }
  }
#endif

  /* If nothing to build, early out. */
  if (builder.empty_grid()) {
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
  vector<float3> face_normals;
  builder.create_mesh(vertices, indices, face_normals, face_overlap_avoidance);

  volume->reserve_mesh(vertices.size(), indices.size() / 3);
  volume->used_shaders.clear();
  volume->used_shaders.push_back_slow(volume_shader);

  for (size_t i = 0; i < vertices.size(); ++i) {
    volume->add_vertex(vertices[i]);
  }

  for (size_t i = 0; i < indices.size(); i += 3) {
    volume->add_triangle(indices[i], indices[i + 1], indices[i + 2], 0, false);
  }

  Attribute *attr_fN = volume->attributes.add(ATTR_STD_FACE_NORMAL);
  float3 *fN = attr_fN->data_float3();

  for (size_t i = 0; i < face_normals.size(); ++i) {
    fN[i] = face_normals[i];
  }

  /* Print stats. */
  VLOG(1) << "Memory usage volume mesh: "
          << ((vertices.size() + face_normals.size()) * sizeof(float3) +
              indices.size() * sizeof(int)) /
                 (1024.0 * 1024.0)
          << "Mb.";
}

CCL_NAMESPACE_END
