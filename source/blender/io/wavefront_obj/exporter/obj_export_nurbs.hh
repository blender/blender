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
  int total_spline_vertices(const int spline_index) const;
  float3 vertex_coordinates(const int spline_index,
                            const int vertex_index,
                            const float scaling_factor) const;
  int total_spline_control_points(const int spline_index) const;
  int get_nurbs_degree(const int spline_index) const;

 private:
  void set_world_axes_transform(const eTransformAxisForward forward, const eTransformAxisUp up);
};

}  // namespace blender::io::obj
