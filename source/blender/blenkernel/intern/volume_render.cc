/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_volume_types.h"

#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"
#include "BKE_volume_render.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Dense.h>
#endif

/* Dense Voxels */

#ifdef WITH_OPENVDB

template<typename GridType, typename VoxelType>
static void extract_dense_voxels(const openvdb::GridBase &grid,
                                 const openvdb::CoordBBox bbox,
                                 VoxelType *r_voxels)
{
  BLI_assert(grid.isType<GridType>());
  blender::threading::memory_bandwidth_bound_task(bbox.volume() * sizeof(VoxelType), [&]() {
    openvdb::tools::Dense<VoxelType, openvdb::tools::LayoutXYZ> dense(bbox, r_voxels);
    openvdb::tools::copyToDense(static_cast<const GridType &>(grid), dense);
  });
}

static void extract_dense_float_voxels(const VolumeGridType grid_type,
                                       const openvdb::GridBase &grid,
                                       const openvdb::CoordBBox &bbox,
                                       float *r_voxels)
{
  switch (grid_type) {
    case VOLUME_GRID_BOOLEAN: {
      extract_dense_voxels<openvdb::BoolGrid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_FLOAT: {
      extract_dense_voxels<openvdb::FloatGrid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_DOUBLE: {
      extract_dense_voxels<openvdb::DoubleGrid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_INT: {
      extract_dense_voxels<openvdb::Int32Grid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_INT64: {
      extract_dense_voxels<openvdb::Int64Grid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_MASK: {
      extract_dense_voxels<openvdb::MaskGrid, float>(grid, bbox, r_voxels);
      return;
    }
    case VOLUME_GRID_VECTOR_FLOAT: {
      extract_dense_voxels<openvdb::Vec3fGrid, openvdb::Vec3f>(
          grid, bbox, reinterpret_cast<openvdb::Vec3f *>(r_voxels));
      return;
    }
    case VOLUME_GRID_VECTOR_DOUBLE: {
      extract_dense_voxels<openvdb::Vec3dGrid, openvdb::Vec3f>(
          grid, bbox, reinterpret_cast<openvdb::Vec3f *>(r_voxels));
      return;
    }
    case VOLUME_GRID_VECTOR_INT: {
      extract_dense_voxels<openvdb::Vec3IGrid, openvdb::Vec3f>(
          grid, bbox, reinterpret_cast<openvdb::Vec3f *>(r_voxels));
      return;
    }
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN:
      /* Zero channels to copy. */
      break;
  }
}

static void create_texture_to_object_matrix(const openvdb::Mat4d &grid_transform,
                                            const openvdb::CoordBBox &bbox,
                                            float r_texture_to_object[4][4])
{
  float index_to_object[4][4];
  memcpy(index_to_object, openvdb::Mat4s(grid_transform).asPointer(), sizeof(index_to_object));

  float texture_to_index[4][4];
  const openvdb::Vec3f loc = bbox.min().asVec3s() - openvdb::Vec3s(0.5f);
  const openvdb::Vec3f size = bbox.dim().asVec3s();
  size_to_mat4(texture_to_index, size.asV());
  copy_v3_v3(texture_to_index[3], loc.asV());

  mul_m4_m4m4(r_texture_to_object, index_to_object, texture_to_index);
}

#endif

bool BKE_volume_grid_dense_floats(const Volume *volume,
                                  const blender::bke::VolumeGridData *volume_grid,
                                  DenseFloatVolumeGrid *r_dense_grid)
{
#ifdef WITH_OPENVDB
  const VolumeGridType grid_type = volume_grid->grid_type();
  blender::bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &grid = volume_grid->grid(tree_token);

  const openvdb::CoordBBox bbox = grid.evalActiveVoxelBoundingBox();
  if (bbox.empty()) {
    return false;
  }
  const std::array<int64_t, 6> bbox_indices = {UNPACK3(openvdb::math::Abs(bbox.min())),
                                               UNPACK3(openvdb::math::Abs(bbox.max()))};
  const int64_t max_bbox_index = *std::max_element(bbox_indices.begin(), bbox_indices.end());
  if (max_bbox_index > (1 << 30)) {
    /* There is an integer overflow when trying to extract dense voxels when the indices are very
     * large. */
    return false;
  }

  const openvdb::Vec3i resolution = bbox.dim().asVec3i();
  const int64_t num_voxels = int64_t(resolution[0]) * int64_t(resolution[1]) *
                             int64_t(resolution[2]);
  const int channels = blender::bke::volume_grid::get_channels_num(grid_type);
  float *voxels = MEM_malloc_arrayN<float>(size_t(channels) * size_t(num_voxels), __func__);
  if (voxels == nullptr) {
    return false;
  }

  extract_dense_float_voxels(grid_type, grid, bbox, voxels);
  create_texture_to_object_matrix(grid.transform().baseMap()->getAffineMap()->getMat4(),
                                  bbox,
                                  r_dense_grid->texture_to_object);

  r_dense_grid->voxels = voxels;
  r_dense_grid->channels = channels;
  copy_v3_v3_int(r_dense_grid->resolution, resolution.asV());
  return true;
#endif
  UNUSED_VARS(volume, volume_grid, r_dense_grid);
  return false;
}

void BKE_volume_dense_float_grid_clear(DenseFloatVolumeGrid *dense_grid)
{
  if (dense_grid->voxels != nullptr) {
    MEM_freeN(dense_grid->voxels);
  }
}

/* Wireframe */

#ifdef WITH_OPENVDB

/** Returns bounding boxes that approximate the shape of the volume stored in the grid. */
template<typename GridType>
static blender::Vector<openvdb::CoordBBox> get_bounding_boxes(const GridType &grid,
                                                              const bool coarse)
{
  using TreeType = typename GridType::TreeType;
  using Depth2Type = typename TreeType::RootNodeType::ChildNodeType::ChildNodeType;
  using NodeCIter = typename TreeType::NodeCIter;

  blender::Vector<openvdb::CoordBBox> boxes;
  const int depth = coarse ? 2 : 3;

  NodeCIter iter = grid.tree().cbeginNode();
  iter.setMaxDepth(depth);

  for (; iter; ++iter) {
    if (iter.getDepth() != depth) {
      continue;
    }

    openvdb::CoordBBox box;
    if (depth == 2) {
      /* Internal node at depth 2. */
      const Depth2Type *node = nullptr;
      iter.getNode(node);
      if (node) {
        node->evalActiveBoundingBox(box, false);
      }
      else {
        continue;
      }
    }
    else {
      /* Leaf node. */
      if (!iter.getBoundingBox(box)) {
        continue;
      }
    }

    /* +1 to convert from exclusive to inclusive bounds. */
    box.max() = box.max().offsetBy(1);

    boxes.append(box);
  }

  return boxes;
}

struct GetBoundingBoxesOp {
  const openvdb::GridBase &grid;
  const bool coarse;

  template<typename GridType> blender::Vector<openvdb::CoordBBox> operator()()
  {
    return get_bounding_boxes(static_cast<const GridType &>(grid), coarse);
  }
};

static blender::Vector<openvdb::CoordBBox> get_bounding_boxes(VolumeGridType grid_type,
                                                              const openvdb::GridBase &grid,
                                                              const bool coarse)
{
  GetBoundingBoxesOp op{grid, coarse};
  return BKE_volume_grid_type_operation(grid_type, op);
}

static void boxes_to_center_points(blender::Span<openvdb::CoordBBox> boxes,
                                   const openvdb::math::Transform &transform,
                                   blender::MutableSpan<blender::float3> r_verts)
{
  BLI_assert(boxes.size() == r_verts.size());
  for (const int i : boxes.index_range()) {
    openvdb::Vec3d center = transform.indexToWorld(boxes[i].getCenter());
    r_verts[i] = blender::float3(center[0], center[1], center[2]);
  }
}

static void boxes_to_corner_points(blender::Span<openvdb::CoordBBox> boxes,
                                   const openvdb::math::Transform &transform,
                                   blender::MutableSpan<blender::float3> r_verts)
{
  BLI_assert(boxes.size() * 8 == r_verts.size());
  for (const int i : boxes.index_range()) {
    const openvdb::CoordBBox &box = boxes[i];

    /* The ordering of the corner points is lexicographic. */
    std::array<openvdb::Coord, 8> corners;
    box.getCornerPoints(corners.data());

    for (int j = 0; j < 8; j++) {
      openvdb::Coord corner_i = corners[j];
      openvdb::Vec3d corner_d = transform.indexToWorld(corner_i);
      r_verts[8 * i + j] = blender::float3(corner_d[0], corner_d[1], corner_d[2]);
    }
  }
}

static void boxes_to_edge_mesh(blender::Span<openvdb::CoordBBox> boxes,
                               const openvdb::math::Transform &transform,
                               blender::Vector<blender::float3> &r_verts,
                               blender::Vector<std::array<int, 2>> &r_edges)
{
  /* TODO: Deduplicate edges, hide flat edges? */

  const int box_edges[12][2] = {
      {0, 1},
      {0, 2},
      {0, 4},
      {1, 3},
      {1, 5},
      {2, 3},
      {2, 6},
      {3, 7},
      {4, 5},
      {4, 6},
      {5, 7},
      {6, 7},
  };

  int vert_offset = r_verts.size();
  int edge_offset = r_edges.size();

  const int vert_amount = 8 * boxes.size();
  const int edge_amount = 12 * boxes.size();

  r_verts.resize(r_verts.size() + vert_amount);
  r_edges.resize(r_edges.size() + edge_amount);
  boxes_to_corner_points(boxes, transform, r_verts.as_mutable_span().take_back(vert_amount));

  for (int i = 0; i < boxes.size(); i++) {
    for (int j = 0; j < 12; j++) {
      r_edges[edge_offset + j] = {vert_offset + box_edges[j][0], vert_offset + box_edges[j][1]};
    }
    vert_offset += 8;
    edge_offset += 12;
  }
}

static void boxes_to_cube_mesh(blender::Span<openvdb::CoordBBox> boxes,
                               const openvdb::math::Transform &transform,
                               blender::Vector<blender::float3> &r_verts,
                               blender::Vector<std::array<int, 3>> &r_tris)
{
  const int box_tris[12][3] = {
      {0, 1, 4},
      {4, 1, 5},
      {0, 2, 1},
      {1, 2, 3},
      {1, 3, 5},
      {5, 3, 7},
      {6, 4, 5},
      {7, 5, 6},
      {2, 0, 4},
      {2, 4, 6},
      {3, 7, 2},
      {6, 2, 7},
  };

  int vert_offset = r_verts.size();
  int tri_offset = r_tris.size();

  const int vert_amount = 8 * boxes.size();
  const int tri_amount = 12 * boxes.size();

  r_verts.resize(r_verts.size() + vert_amount);
  r_tris.resize(r_tris.size() + tri_amount);
  boxes_to_corner_points(boxes, transform, r_verts.as_mutable_span().take_back(vert_amount));

  for (int i = 0; i < boxes.size(); i++) {
    for (int j = 0; j < 12; j++) {
      r_tris[tri_offset + j] = {vert_offset + box_tris[j][0],
                                vert_offset + box_tris[j][1],
                                vert_offset + box_tris[j][2]};
    }
    vert_offset += 8;
    tri_offset += 12;
  }
}

#endif

void BKE_volume_grid_wireframe(const Volume *volume,
                               const blender::bke::VolumeGridData *volume_grid,
                               BKE_volume_wireframe_cb cb,
                               void *cb_userdata)
{
  if (volume->display.wireframe_type == VOLUME_WIREFRAME_NONE) {
    cb(cb_userdata, nullptr, nullptr, 0, 0);
    return;
  }

#ifdef WITH_OPENVDB
  blender::bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &grid = volume_grid->grid(tree_token);

  if (volume->display.wireframe_type == VOLUME_WIREFRAME_BOUNDS) {
    /* Bounding box. */
    openvdb::CoordBBox box;
    blender::Vector<blender::float3> verts;
    blender::Vector<std::array<int, 2>> edges;
    if (grid.baseTree().evalLeafBoundingBox(box)) {
      boxes_to_edge_mesh({box}, grid.transform(), verts, edges);
    }
    cb(cb_userdata,
       (float (*)[3])verts.data(),
       (int (*)[2])edges.data(),
       verts.size(),
       edges.size());
  }
  else {
    blender::Vector<openvdb::CoordBBox> boxes = get_bounding_boxes(
        volume_grid->grid_type(),
        grid,
        volume->display.wireframe_detail == VOLUME_WIREFRAME_COARSE);

    blender::Vector<blender::float3> verts;
    blender::Vector<std::array<int, 2>> edges;

    if (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS) {
      verts.resize(boxes.size());
      boxes_to_center_points(boxes, grid.transform(), verts);
    }
    else {
      boxes_to_edge_mesh(boxes, grid.transform(), verts, edges);
    }

    cb(cb_userdata,
       (float (*)[3])verts.data(),
       (int (*)[2])edges.data(),
       verts.size(),
       edges.size());
  }

#else
  UNUSED_VARS(volume, volume_grid);
  cb(cb_userdata, nullptr, nullptr, 0, 0);
#endif
}

#ifdef WITH_OPENVDB
static void grow_triangles(blender::MutableSpan<blender::float3> verts,
                           blender::Span<std::array<int, 3>> tris,
                           const float factor)
{
  /* Compute the offset for every vertex based on the connected edges.
   * This formula simply tries increases the length of all edges. */
  blender::Array<blender::float3> offsets(verts.size(), {0, 0, 0});
  blender::Array<float> weights(verts.size(), 0.0f);
  for (const std::array<int, 3> &tri : tris) {
    offsets[tri[0]] += factor * (2 * verts[tri[0]] - verts[tri[1]] - verts[tri[2]]);
    offsets[tri[1]] += factor * (2 * verts[tri[1]] - verts[tri[0]] - verts[tri[2]]);
    offsets[tri[2]] += factor * (2 * verts[tri[2]] - verts[tri[0]] - verts[tri[1]]);
    weights[tri[0]] += 1.0;
    weights[tri[1]] += 1.0;
    weights[tri[2]] += 1.0;
  }
  /* Apply the computed offsets. */
  for (const int i : verts.index_range()) {
    if (weights[i] > 0.0f) {
      verts[i] += offsets[i] / weights[i];
    }
  }
}
#endif /* WITH_OPENVDB */

void BKE_volume_grid_selection_surface(const Volume * /*volume*/,
                                       const blender::bke::VolumeGridData *volume_grid,
                                       BKE_volume_selection_surface_cb cb,
                                       void *cb_userdata)
{
#ifdef WITH_OPENVDB
  blender::bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &grid = volume_grid->grid(tree_token);
  blender::Vector<openvdb::CoordBBox> boxes = get_bounding_boxes(
      volume_grid->grid_type(), grid, true);

  blender::Vector<blender::float3> verts;
  blender::Vector<std::array<int, 3>> tris;
  boxes_to_cube_mesh(boxes, grid.transform(), verts, tris);

  /* By slightly scaling the individual boxes up, we can avoid some artifacts when drawing the
   * selection outline. */
  const float offset_factor = 0.01f;
  grow_triangles(verts, tris, offset_factor);

  cb(cb_userdata, (float (*)[3])verts.data(), (int (*)[3])tris.data(), verts.size(), tris.size());
#else
  UNUSED_VARS(volume_grid);
  cb(cb_userdata, nullptr, nullptr, 0, 0);
#endif
}

/* Render */

float BKE_volume_density_scale(const Volume *volume, const float matrix[4][4])
{
  if (volume->render.space == VOLUME_SPACE_OBJECT) {
    float unit[3] = {1.0f, 1.0f, 1.0f};
    normalize_v3(unit);
    mul_mat3_m4_v3(matrix, unit);
    return 1.0f / len_v3(unit);
  }

  return 1.0f;
}
