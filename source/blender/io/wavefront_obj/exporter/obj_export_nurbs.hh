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
 * \ingroup obj
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "DNA_curve_types.h"

namespace blender::io::obj {

/**
 * Provides access to the a Curve Object's properties.
 * Only #CU_NURBS type is supported.
 *
 * \note Used for Curves to be exported in parameter form, and not converted to meshes.
 */
class OBJCurve : NonCopyable {
 private:
  const Object *export_object_eval_;
  const Curve *export_curve_;
  float world_axes_transform_[4][4];

 public:
  OBJCurve(const Depsgraph *depsgraph, const OBJExportParams &export_params, Object *curve_object);

  const char *get_curve_name() const;
  int total_splines() const;
  /**
   * \param spline_index: Zero-based index of spline of interest.
   * \return: Total vertices in a spline.
   */
  int total_spline_vertices(int spline_index) const;
  /**
   * Get coordinates of the vertex at the given index on the given spline.
   */
  float3 vertex_coordinates(int spline_index, int vertex_index, float scaling_factor) const;
  /**
   * Get total control points of the NURBS spline at the given index. This is different than total
   * vertices of a spline.
   */
  int total_spline_control_points(int spline_index) const;
  /**
   * Get the degree of the NURBS spline at the given index.
   */
  int get_nurbs_degree(int spline_index) const;

 private:
  /**
   * Set the final transform after applying axes settings and an Object's world transform.
   */
  void set_world_axes_transform(eTransformAxisForward forward, eTransformAxisUp up);
};

}  // namespace blender::io::obj
