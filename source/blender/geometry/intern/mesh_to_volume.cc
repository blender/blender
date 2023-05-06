/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_task.hh"

#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_volume.h"

#include "GEO_mesh_to_volume.hh"

#ifdef WITH_OPENVDB
#  include <algorithm>
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/VolumeToMesh.h>

namespace blender::geometry {

/* This class follows the MeshDataAdapter interface from openvdb. */
class OpenVDBMeshAdapter {
 private:
  Span<float3> positions_;
  Span<int> corner_verts_;
  Span<MLoopTri> looptris_;
  float4x4 transform_;

 public:
  OpenVDBMeshAdapter(const Mesh &mesh, float4x4 transform);
  size_t polygonCount() const;
  size_t pointCount() const;
  size_t vertexCount(size_t /*polygon_index*/) const;
  void getIndexSpacePoint(size_t polygon_index, size_t vertex_index, openvdb::Vec3d &pos) const;
};

OpenVDBMeshAdapter::OpenVDBMeshAdapter(const Mesh &mesh, float4x4 transform)
    : positions_(mesh.vert_positions()),
      corner_verts_(mesh.corner_verts()),
      looptris_(mesh.looptris()),
      transform_(transform)
{
}

size_t OpenVDBMeshAdapter::polygonCount() const
{
  return size_t(looptris_.size());
}

size_t OpenVDBMeshAdapter::pointCount() const
{
  return size_t(positions_.size());
}

size_t OpenVDBMeshAdapter::vertexCount(size_t /*polygon_index*/) const
{
  /* All polygons are triangles. */
  return 3;
}

void OpenVDBMeshAdapter::getIndexSpacePoint(size_t polygon_index,
                                            size_t vertex_index,
                                            openvdb::Vec3d &pos) const
{
  const MLoopTri &looptri = looptris_[polygon_index];
  const float3 transformed_co = math::transform_point(
      transform_, positions_[corner_verts_[looptri.tri[vertex_index]]]);
  pos = &transformed_co.x;
}

float volume_compute_voxel_size(const Depsgraph *depsgraph,
                                FunctionRef<void(float3 &r_min, float3 &r_max)> bounds_fn,
                                const MeshToVolumeResolution res,
                                const float exterior_band_width,
                                const float4x4 &transform)
{
  const float volume_simplify = BKE_volume_simplify_factor(depsgraph);
  if (volume_simplify == 0.0f) {
    return 0.0f;
  }

  if (res.mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE) {
    return res.settings.voxel_size / volume_simplify;
  }
  if (res.settings.voxel_amount <= 0) {
    return 0;
  }

  float3 bb_min;
  float3 bb_max;
  bounds_fn(bb_min, bb_max);

  /* Compute the diagonal of the bounding box. This is used because
   * it will always be bigger than the widest side of the mesh. */
  const float diagonal = math::distance(math::transform_point(transform, bb_max),
                                        math::transform_point(transform, bb_min));

  /* To get the approximate size per voxel, first subtract the exterior band from the requested
   * voxel amount, then divide the diagonal with this value if it's bigger than 1. */
  const float voxel_size =
      (diagonal / std::max(1.0f, float(res.settings.voxel_amount) - 2.0f * exterior_band_width));

  /* Return the simplified voxel size. */
  return voxel_size / volume_simplify;
}

static openvdb::FloatGrid::Ptr mesh_to_fog_volume_grid(
    const Mesh *mesh,
    const float4x4 &mesh_to_volume_space_transform,
    const float voxel_size,
    const float interior_band_width,
    const float density)
{
  if (voxel_size < 1e-5f || interior_band_width <= 0.0f) {
    return nullptr;
  }

  float4x4 mesh_to_index_space_transform = math::from_scale<float4x4>(float3(1.0f / voxel_size));
  mesh_to_index_space_transform *= mesh_to_volume_space_transform;
  /* Better align generated grid with the source mesh. */
  mesh_to_index_space_transform.location() -= 0.5f;

  OpenVDBMeshAdapter mesh_adapter{*mesh, mesh_to_index_space_transform};
  const float interior = std::max(1.0f, interior_band_width / voxel_size);

  openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(
      voxel_size);
  openvdb::FloatGrid::Ptr new_grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
      mesh_adapter, *transform, 1.0f, interior);

  openvdb::tools::sdfToFogVolume(*new_grid);

  if (density != 1.0f) {
    openvdb::tools::foreach (new_grid->beginValueOn(),
                             [&](const openvdb::FloatGrid::ValueOnIter &iter) {
                               iter.modifyValue([&](float &value) { value *= density; });
                             });
  }
  return new_grid;
}

static openvdb::FloatGrid::Ptr mesh_to_sdf_volume_grid(const Mesh &mesh,
                                                       const float voxel_size,
                                                       const float half_band_width)
{
  if (voxel_size <= 0.0f || half_band_width <= 0.0f) {
    return nullptr;
  }

  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<MLoopTri> looptris = mesh.looptris();

  std::vector<openvdb::Vec3s> points(positions.size());
  std::vector<openvdb::Vec3I> triangles(looptris.size());

  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const float3 &co = positions[i];
      points[i] = openvdb::Vec3s(co.x, co.y, co.z) - 0.5f * voxel_size;
    }
  });

  threading::parallel_for(looptris.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const MLoopTri &loop_tri = looptris[i];
      triangles[i] = openvdb::Vec3I(corner_verts[loop_tri.tri[0]],
                                    corner_verts[loop_tri.tri[1]],
                                    corner_verts[loop_tri.tri[2]]);
    }
  });

  openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(
      voxel_size);
  openvdb::FloatGrid::Ptr new_grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
      *transform, points, triangles, half_band_width);

  return new_grid;
}

VolumeGrid *fog_volume_grid_add_from_mesh(Volume *volume,
                                          const StringRefNull name,
                                          const Mesh *mesh,
                                          const float4x4 &mesh_to_volume_space_transform,
                                          const float voxel_size,
                                          const float interior_band_width,
                                          const float density)
{
  openvdb::FloatGrid::Ptr mesh_grid = mesh_to_fog_volume_grid(
      mesh, mesh_to_volume_space_transform, voxel_size, interior_band_width, density);
  return mesh_grid ? BKE_volume_grid_add_vdb(*volume, name, std::move(mesh_grid)) : nullptr;
}

VolumeGrid *sdf_volume_grid_add_from_mesh(Volume *volume,
                                          const StringRefNull name,
                                          const Mesh &mesh,
                                          const float voxel_size,
                                          const float half_band_width)
{
  openvdb::FloatGrid::Ptr mesh_grid = mesh_to_sdf_volume_grid(mesh, voxel_size, half_band_width);
  return mesh_grid ? BKE_volume_grid_add_vdb(*volume, name, std::move(mesh_grid)) : nullptr;
}
}  // namespace blender::geometry
#endif
