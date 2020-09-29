/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_float3.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DNA_volume_types.h"

#include "BKE_volume.h"
#include "BKE_volume_render.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Dense.h>
#endif

/* Dense Voxels */

bool BKE_volume_grid_dense_bounds(const Volume *volume,
                                  VolumeGrid *volume_grid,
                                  int64_t min[3],
                                  int64_t max[3])
{
#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

  openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();
  if (!bbox.empty()) {
    /* OpenVDB bbox is inclusive, so add 1 to convert. */
    min[0] = bbox.min().x();
    min[1] = bbox.min().y();
    min[2] = bbox.min().z();
    max[0] = bbox.max().x() + 1;
    max[1] = bbox.max().y() + 1;
    max[2] = bbox.max().z() + 1;
    return true;
  }
#else
  UNUSED_VARS(volume, volume_grid);
#endif

  min[0] = 0;
  min[1] = 0;
  min[2] = 0;
  max[0] = 0;
  max[1] = 0;
  max[2] = 0;
  return false;
}

/* Transform matrix from unit cube to object space, for 3D texture sampling. */
void BKE_volume_grid_dense_transform_matrix(const VolumeGrid *volume_grid,
                                            const int64_t min[3],
                                            const int64_t max[3],
                                            float mat[4][4])
{
#ifdef WITH_OPENVDB
  float index_to_world[4][4];
  BKE_volume_grid_transform_matrix(volume_grid, index_to_world);

  float texture_to_index[4][4];
  float loc[3] = {(float)min[0], (float)min[1], (float)min[2]};
  float size[3] = {(float)(max[0] - min[0]), (float)(max[1] - min[1]), (float)(max[2] - min[2])};
  size_to_mat4(texture_to_index, size);
  copy_v3_v3(texture_to_index[3], loc);

  mul_m4_m4m4(mat, index_to_world, texture_to_index);
#else
  UNUSED_VARS(volume_grid, min, max);
  unit_m4(mat);
#endif
}

void BKE_volume_grid_dense_voxels(const Volume *volume,
                                  VolumeGrid *volume_grid,
                                  const int64_t min[3],
                                  const int64_t max[3],
                                  float *voxels)
{
#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

  /* Convert to OpenVDB inclusive bbox with -1. */
  openvdb::CoordBBox bbox(min[0], min[1], min[2], max[0] - 1, max[1] - 1, max[2] - 1);

  switch (BKE_volume_grid_type(volume_grid)) {
    case VOLUME_GRID_BOOLEAN: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::BoolGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_FLOAT: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_DOUBLE: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::DoubleGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_INT: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int32Grid>(grid), dense);
      break;
    }
    case VOLUME_GRID_INT64: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int64Grid>(grid), dense);
      break;
    }
    case VOLUME_GRID_MASK: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::MaskGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_FLOAT: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3fGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_DOUBLE: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3dGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_INT: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3IGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_STRING:
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN: {
      /* Zero channels to copy. */
      break;
    }
  }
#else
  UNUSED_VARS(volume, volume_grid, min, max, voxels);
#endif
}

/* Wireframe */

#ifdef WITH_OPENVDB

/** Returns bounding boxes that approximate the shape of the volume stored in the grid. */
template<typename GridType>
static blender::Vector<openvdb::CoordBBox> get_bounding_boxes(openvdb::GridBase::ConstPtr gridbase,
                                                              const bool coarse)
{
  using TreeType = typename GridType::TreeType;
  using Depth2Type = typename TreeType::RootNodeType::ChildNodeType::ChildNodeType;
  using NodeCIter = typename TreeType::NodeCIter;
  using GridConstPtr = typename GridType::ConstPtr;

  GridConstPtr grid = openvdb::gridConstPtrCast<GridType>(gridbase);
  blender::Vector<openvdb::CoordBBox> boxes;
  const int depth = coarse ? 2 : 3;

  NodeCIter iter = grid->tree().cbeginNode();
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

static blender::Vector<openvdb::CoordBBox> get_bounding_boxes(VolumeGridType grid_type,
                                                              openvdb::GridBase::ConstPtr grid,
                                                              const bool coarse)
{
  switch (grid_type) {
    case VOLUME_GRID_BOOLEAN: {
      return get_bounding_boxes<openvdb::BoolGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_FLOAT: {
      return get_bounding_boxes<openvdb::FloatGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_DOUBLE: {
      return get_bounding_boxes<openvdb::DoubleGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_INT: {
      return get_bounding_boxes<openvdb::Int32Grid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_INT64: {
      return get_bounding_boxes<openvdb::Int64Grid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_MASK: {
      return get_bounding_boxes<openvdb::MaskGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_VECTOR_FLOAT: {
      return get_bounding_boxes<openvdb::Vec3fGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_VECTOR_DOUBLE: {
      return get_bounding_boxes<openvdb::Vec3dGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_VECTOR_INT: {
      return get_bounding_boxes<openvdb::Vec3IGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_STRING: {
      return get_bounding_boxes<openvdb::StringGrid>(grid, coarse);
      break;
    }
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN: {
      break;
    }
  }
  return {};
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
                               VolumeGrid *volume_grid,
                               BKE_volume_wireframe_cb cb,
                               void *cb_userdata)
{
  if (volume->display.wireframe_type == VOLUME_WIREFRAME_NONE) {
    cb(cb_userdata, NULL, NULL, 0, 0);
    return;
  }

#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

  if (volume->display.wireframe_type == VOLUME_WIREFRAME_BOUNDS) {
    /* Bounding box. */
    openvdb::CoordBBox box;
    blender::Vector<blender::float3> verts;
    blender::Vector<std::array<int, 2>> edges;
    if (grid->baseTree().evalLeafBoundingBox(box)) {
      boxes_to_edge_mesh({box}, grid->transform(), verts, edges);
    }
    cb(cb_userdata,
       (float(*)[3])verts.data(),
       (int(*)[2])edges.data(),
       verts.size(),
       edges.size());
  }
  else {
    blender::Vector<openvdb::CoordBBox> boxes = get_bounding_boxes(
        BKE_volume_grid_type(volume_grid),
        grid,
        volume->display.wireframe_detail == VOLUME_WIREFRAME_COARSE);

    blender::Vector<blender::float3> verts;
    blender::Vector<std::array<int, 2>> edges;

    if (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS) {
      verts.resize(boxes.size());
      boxes_to_center_points(boxes, grid->transform(), verts);
    }
    else {
      boxes_to_edge_mesh(boxes, grid->transform(), verts, edges);
    }

    cb(cb_userdata,
       (float(*)[3])verts.data(),
       (int(*)[2])edges.data(),
       verts.size(),
       edges.size());
  }

#else
  UNUSED_VARS(volume, volume_grid);
  cb(cb_userdata, NULL, NULL, 0, 0);
#endif
}

static void grow_triangles(blender::MutableSpan<blender::float3> verts,
                           blender::Span<std::array<int, 3>> tris,
                           const float factor)
{
  /* Compute the offset for every vertex based on the connected edges.
   * This formula simply tries increases the length of all edges. */
  blender::Array<blender::float3> offsets(verts.size(), {0, 0, 0});
  for (const std::array<int, 3> &tri : tris) {
    offsets[tri[0]] += factor * (2 * verts[tri[0]] - verts[tri[1]] - verts[tri[2]]);
    offsets[tri[1]] += factor * (2 * verts[tri[1]] - verts[tri[0]] - verts[tri[2]]);
    offsets[tri[2]] += factor * (2 * verts[tri[2]] - verts[tri[0]] - verts[tri[1]]);
  }
  /* Apply the computed offsets. */
  for (const int i : verts.index_range()) {
    verts[i] += offsets[i];
  }
}

void BKE_volume_grid_selection_surface(const Volume *volume,
                                       VolumeGrid *volume_grid,
                                       BKE_volume_selection_surface_cb cb,
                                       void *cb_userdata)
{
#ifdef WITH_OPENVDB
  openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
  blender::Vector<openvdb::CoordBBox> boxes = get_bounding_boxes(
      BKE_volume_grid_type(volume_grid), grid, true);

  blender::Vector<blender::float3> verts;
  blender::Vector<std::array<int, 3>> tris;
  boxes_to_cube_mesh(boxes, grid->transform(), verts, tris);

  /* By slightly scaling the individual boxes up, we can avoid some artifacts when drawing the
   * selection outline. */
  const float offset_factor = 0.01f;
  grow_triangles(verts, tris, offset_factor);

  cb(cb_userdata, (float(*)[3])verts.data(), (int(*)[3])tris.data(), verts.size(), tris.size());
#else
  UNUSED_VARS(volume, volume_grid);
  cb(cb_userdata, NULL, NULL, 0, 0);
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
