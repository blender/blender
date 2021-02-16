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

#include "BKE_geometry_set.hh"

namespace blender::bke {

/**
 * Used to keep track of a group of instances using the same geometry data.
 */
struct GeometryInstanceGroup {
  /**
   * The geometry set instanced on each of the transforms. The components are not necessarily
   * owned here. For example, they may be owned by the instanced object. This cannot be a
   * reference because not all instanced data will necessarily have a #geometry_set_eval.
   */
  GeometrySet geometry_set;

  /**
   * As an optimization to avoid copying, the same geometry set can be associated with multiple
   * instances. Each instance is stored as a transform matrix here. Again, these must be owned
   * because they may be transformed from the original data. TODO: Validate that last statement.
   */
  Vector<float4x4> transforms;
};

Vector<GeometryInstanceGroup> geometry_set_gather_instances(const GeometrySet &geometry_set);

}  // namespace blender::bke
