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
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

#ifndef __OPENVDB_LEVEL_SET_H__
#define __OPENVDB_LEVEL_SET_H__

#include "openvdb_capi.h"
#include <openvdb/math/FiniteDifference.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/LevelSetFilter.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

struct OpenVDBLevelSet {
 private:
  openvdb::FloatGrid::Ptr grid;

 public:
  OpenVDBLevelSet();
  ~OpenVDBLevelSet();
  const openvdb::FloatGrid::Ptr &get_grid();
  void set_grid(const openvdb::FloatGrid::Ptr &grid);

  void mesh_to_level_set(const float *vertices,
                         const unsigned int *faces,
                         const unsigned int totvertices,
                         const unsigned int totfaces,
                         const openvdb::math::Transform::Ptr &transform);

  void volume_to_mesh(struct OpenVDBVolumeToMeshData *mesh,
                      const double isovalue,
                      const double adaptivity,
                      const bool relax_disoriented_triangles);
  void filter(OpenVDBLevelSet_FilterType filter_type,
              int width,
              float distance,
              OpenVDBLevelSet_FilterBias filter_bias);
  openvdb::FloatGrid::Ptr CSG_operation_apply(const openvdb::FloatGrid::Ptr &gridA,
                                              const openvdb::FloatGrid::Ptr &gridB,
                                              OpenVDBLevelSet_CSGOperation operation);
};

#endif /* __OPENVDB_LEVEL_SET_H__ */
