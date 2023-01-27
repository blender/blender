/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
                            float threshold,
                            float adaptivity);

/**
 * Convert an OpenVDB volume grid to corresponding mesh data: vertex positions and quad and
 * triangle indices.
 */
struct OpenVDBMeshData volume_to_mesh_data(const openvdb::GridBase &grid,
                                           const VolumeToMeshResolution &resolution,
                                           float threshold,
                                           float adaptivity);

/**
 * Convert mesh data from the format provided by OpenVDB into Blender's #Mesh data structure.
 * This can be used to add mesh data from a grid into an existing mesh rather than merging multiple
 * meshes later on.
 */
void fill_mesh_from_openvdb_data(const Span<openvdb::Vec3s> vdb_verts,
                                 const Span<openvdb::Vec3I> vdb_tris,
                                 const Span<openvdb::Vec4I> vdb_quads,
                                 int vert_offset,
                                 int poly_offset,
                                 int loop_offset,
                                 MutableSpan<float3> vert_positions,
                                 MutableSpan<MPoly> polys,
                                 MutableSpan<MLoop> loops);

#endif

}  // namespace blender::bke
