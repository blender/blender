/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_volume.h"

#include "GEO_mesh_to_volume.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>

namespace blender::geometry {

/* This class follows the MeshDataAdapter interface from openvdb. */
class OpenVDBMeshAdapter {
 private:
  Span<float3> positions_;
  Span<MLoop> loops_;
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
      loops_(mesh.loops()),
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
  const float3 transformed_co = transform_ * positions_[loops_[looptri.tri[vertex_index]].v];
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

  /* Compute the voxel size based on the desired number of voxels and the approximated bounding
   * box of the volume. */
  const float diagonal = math::distance(transform * bb_max, transform * bb_min);
  const float approximate_volume_side_length = diagonal + exterior_band_width * 2.0f;
  const float voxel_size = approximate_volume_side_length / res.settings.voxel_amount /
                           volume_simplify;
  return voxel_size;
}

static openvdb::FloatGrid::Ptr mesh_to_volume_grid(const Mesh *mesh,
                                                   const float4x4 &mesh_to_volume_space_transform,
                                                   const float voxel_size,
                                                   const bool fill_volume,
                                                   const float exterior_band_width,
                                                   const float interior_band_width,
                                                   const float density)
{
  if (voxel_size == 0.0f) {
    return nullptr;
  }

  float4x4 mesh_to_index_space_transform;
  scale_m4_fl(mesh_to_index_space_transform.values, 1.0f / voxel_size);
  mul_m4_m4_post(mesh_to_index_space_transform.values, mesh_to_volume_space_transform.values);
  /* Better align generated grid with the source mesh. */
  add_v3_fl(mesh_to_index_space_transform.values[3], -0.5f);

  OpenVDBMeshAdapter mesh_adapter{*mesh, mesh_to_index_space_transform};

  /* Convert the bandwidths from object in index space. */
  const float exterior = MAX2(0.001f, exterior_band_width / voxel_size);
  const float interior = MAX2(0.001f, interior_band_width / voxel_size);

  openvdb::FloatGrid::Ptr new_grid;
  if (fill_volume) {
    /* Setting the interior bandwidth to FLT_MAX, will make it fill the entire volume. */
    new_grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
        mesh_adapter, {}, exterior, FLT_MAX);
  }
  else {
    new_grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
        mesh_adapter, {}, exterior, interior);
  }

  /* Give each grid cell a fixed density for now. */
  openvdb::tools::foreach (
      new_grid->beginValueOn(),
      [density](const openvdb::FloatGrid::ValueOnIter &iter) { iter.setValue(density); });

  return new_grid;
}

VolumeGrid *volume_grid_add_from_mesh(Volume *volume,
                                      const StringRefNull name,
                                      const Mesh *mesh,
                                      const float4x4 &mesh_to_volume_space_transform,
                                      const float voxel_size,
                                      const bool fill_volume,
                                      const float exterior_band_width,
                                      const float interior_band_width,
                                      const float density)
{
  VolumeGrid *c_grid = BKE_volume_grid_add(volume, name.c_str(), VOLUME_GRID_FLOAT);
  openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(
      BKE_volume_grid_openvdb_for_write(volume, c_grid, false));

  /* Generate grid from mesh */
  openvdb::FloatGrid::Ptr mesh_grid = mesh_to_volume_grid(mesh,
                                                          mesh_to_volume_space_transform,
                                                          voxel_size,
                                                          fill_volume,
                                                          exterior_band_width,
                                                          interior_band_width,
                                                          density);

  /* Merge the generated grid. Should be cheap because grid has just been created. */
  grid->merge(*mesh_grid);
  /* Set class to "Fog Volume". */
  grid->setGridClass(openvdb::GRID_FOG_VOLUME);
  /* Change transform so that the index space is correctly transformed to object space. */
  grid->transform().postScale(voxel_size);
  return c_grid;
}
}  // namespace blender::geometry
#endif
