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

#include "BLI_span.hh"

#include "DNA_modifier_types.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#endif

struct Mesh;

namespace blender::bke {

struct VolumeToMeshResolution {
  VolumeToMeshResolutionMode mode;
  union {
    float voxel_size;
    float voxel_amount;
  } settings;
};

#ifdef WITH_OPENVDB

/**
 * The result of converting a volume grid to mesh data, in the format used by the OpenVDB API.
 */
struct OpenVDBMeshData {
  std::vector<openvdb::Vec3s> verts;
  std::vector<openvdb::Vec3I> tris;
  std::vector<openvdb::Vec4I> quads;
  bool is_empty() const
  {
    return verts.empty();
  }
};

struct Mesh *volume_to_mesh(const openvdb::GridBase &grid,
                            const VolumeToMeshResolution &resolution,
                            const float threshold,
                            const float adaptivity);

struct OpenVDBMeshData volume_to_mesh_data(const openvdb::GridBase &grid,
                                           const VolumeToMeshResolution &resolution,
                                           const float threshold,
                                           const float adaptivity);

void fill_mesh_from_openvdb_data(const Span<openvdb::Vec3s> vdb_verts,
                                 const Span<openvdb::Vec3I> vdb_tris,
                                 const Span<openvdb::Vec4I> vdb_quads,
                                 const int vert_offset,
                                 const int poly_offset,
                                 const int loop_offset,
                                 MutableSpan<MVert> verts,
                                 MutableSpan<MPoly> polys,
                                 MutableSpan<MLoop> loops);

#endif

}  // namespace blender::bke
