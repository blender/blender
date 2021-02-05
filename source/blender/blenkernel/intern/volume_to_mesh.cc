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

#include <vector>

#include "BLI_float3.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_volume_types.h"

#include "BKE_mesh.h"
#include "BKE_volume.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

#include "BKE_volume_to_mesh.hh"

namespace blender::bke {

#ifdef WITH_OPENVDB

struct VolumeToMeshOp {
  const openvdb::GridBase &base_grid;
  const VolumeToMeshResolution resolution;
  const float threshold;
  const float adaptivity;
  std::vector<openvdb::Vec3s> verts;
  std::vector<openvdb::Vec3I> tris;
  std::vector<openvdb::Vec4I> quads;

  template<typename GridType> bool operator()()
  {
    if constexpr (std::is_scalar_v<typename GridType::ValueType>) {
      this->generate_mesh_data<GridType>();
      return true;
    }
    return false;
  }

  template<typename GridType> void generate_mesh_data()
  {
    const GridType &grid = static_cast<const GridType &>(base_grid);

    if (this->resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_GRID) {
      this->grid_to_mesh(grid);
      return;
    }

    const float resolution_factor = this->compute_resolution_factor(base_grid);
    typename GridType::Ptr temp_grid = this->create_grid_with_changed_resolution(
        grid, resolution_factor);
    this->grid_to_mesh(*temp_grid);
  }

  template<typename GridType>
  typename GridType::Ptr create_grid_with_changed_resolution(const GridType &old_grid,
                                                             const float resolution_factor)
  {
    BLI_assert(resolution_factor > 0.0f);

    openvdb::Mat4R xform;
    xform.setToScale(openvdb::Vec3d(resolution_factor));
    openvdb::tools::GridTransformer transformer{xform};

    typename GridType::Ptr new_grid = GridType::create();
    transformer.transformGrid<openvdb::tools::BoxSampler>(old_grid, *new_grid);
    new_grid->transform() = old_grid.transform();
    new_grid->transform().preScale(1.0f / resolution_factor);
    return new_grid;
  }

  float compute_resolution_factor(const openvdb::GridBase &grid) const
  {
    const openvdb::Vec3s voxel_size{grid.voxelSize()};
    const float current_voxel_size = std::max({voxel_size[0], voxel_size[1], voxel_size[2]});
    const float desired_voxel_size = this->compute_desired_voxel_size(grid);
    return current_voxel_size / desired_voxel_size;
  }

  float compute_desired_voxel_size(const openvdb::GridBase &grid) const
  {
    if (this->resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
      return this->resolution.settings.voxel_size;
    }
    const openvdb::CoordBBox coord_bbox = base_grid.evalActiveVoxelBoundingBox();
    const openvdb::BBoxd bbox = grid.transform().indexToWorld(coord_bbox);
    const float max_extent = bbox.extents()[bbox.maxExtent()];
    const float voxel_size = max_extent / this->resolution.settings.voxel_amount;
    return voxel_size;
  }

  template<typename GridType> void grid_to_mesh(const GridType &grid)
  {
    openvdb::tools::volumeToMesh(
        grid, this->verts, this->tris, this->quads, this->threshold, this->adaptivity);

    /* Better align generated mesh with volume (see T85312). */
    openvdb::Vec3s offset = grid.voxelSize() / 2.0f;
    for (openvdb::Vec3s &position : this->verts) {
      position += offset;
    }
  }
};

static Mesh *new_mesh_from_openvdb_data(Span<openvdb::Vec3s> verts,
                                        Span<openvdb::Vec3I> tris,
                                        Span<openvdb::Vec4I> quads)
{
  const int tot_loops = 3 * tris.size() + 4 * quads.size();
  const int tot_polys = tris.size() + quads.size();

  Mesh *mesh = BKE_mesh_new_nomain(verts.size(), 0, 0, tot_loops, tot_polys);

  /* Write vertices. */
  for (const int i : verts.index_range()) {
    const blender::float3 co = blender::float3(verts[i].asV());
    copy_v3_v3(mesh->mvert[i].co, co);
  }

  /* Write triangles. */
  for (const int i : tris.index_range()) {
    mesh->mpoly[i].loopstart = 3 * i;
    mesh->mpoly[i].totloop = 3;
    for (int j = 0; j < 3; j++) {
      /* Reverse vertex order to get correct normals. */
      mesh->mloop[3 * i + j].v = tris[i][2 - j];
    }
  }

  /* Write quads. */
  const int poly_offset = tris.size();
  const int loop_offset = tris.size() * 3;
  for (const int i : quads.index_range()) {
    mesh->mpoly[poly_offset + i].loopstart = loop_offset + 4 * i;
    mesh->mpoly[poly_offset + i].totloop = 4;
    for (int j = 0; j < 4; j++) {
      /* Reverse vertex order to get correct normals. */
      mesh->mloop[loop_offset + 4 * i + j].v = quads[i][3 - j];
    }
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);
  return mesh;
}

Mesh *volume_to_mesh(const openvdb::GridBase &grid,
                     const VolumeToMeshResolution &resolution,
                     const float threshold,
                     const float adaptivity)
{
  const VolumeGridType grid_type = BKE_volume_grid_type_openvdb(grid);

  VolumeToMeshOp to_mesh_op{grid, resolution, threshold, adaptivity};
  if (!BKE_volume_grid_type_operation(grid_type, to_mesh_op)) {
    return nullptr;
  }

  return new_mesh_from_openvdb_data(to_mesh_op.verts, to_mesh_op.tris, to_mesh_op.quads);
}

#endif /* WITH_OPENVDB */

}  // namespace blender::bke
