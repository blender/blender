/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_utility_mixins.hh"

#include "DNA_curve_types.h"

#include "IO_orientation.hh"

struct OBJExportParams;

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
  const Nurb *get_spline(int spline_index) const;
  /**
   * \param spline_index: Zero-based index of spline of interest.
   * \return Total vertices in a spline.
   */
  int total_spline_vertices(int spline_index) const;
  /**
   * Get coordinates of the vertex at the given index on the given spline.
   */
  float3 vertex_coordinates(int spline_index, int vertex_index, float global_scale) const;
  /**
   * Get the number of control points on the U-dimension.
   */
  int num_control_points_u(int spline_index) const;
  /**
   * Get the number of control points on the V-dimension.
   */
  int num_control_points_v(int spline_index) const;
  /**
   * Get the degree of the NURBS spline for the U-dimension.
   */
  int get_nurbs_degree_u(int spline_index) const;
  /**
   * Get the degree of the NURBS spline for the V-dimension.
   */
  int get_nurbs_degree_v(int spline_index) const;
  /**
   * Get the U flags (CU_NURB_*) of the NURBS spline at the given index.
   */
  short get_nurbs_flagu(int spline_index) const;
  /**
   * Get the knot vector for the U-dimension. Computes knots using the buffer if necessary.
   */
  Span<float> get_knots_u(int spline_index, Vector<float> &buffer) const;

 private:
  /**
   * Set the final transform after applying axes settings and an Object's world transform.
   */
  void set_world_axes_transform(eIOAxis forward, eIOAxis up);
};

}  // namespace blender::io::obj
