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
struct Mesh *volume_to_mesh(const openvdb::GridBase &grid,
                            const VolumeToMeshResolution &resolution,
                            const float threshold,
                            const float adaptivity);
#endif

}  // namespace blender::bke
