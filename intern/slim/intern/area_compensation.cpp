/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include "BLI_assert.h"

#include <Eigen/Dense>

#include "area_compensation.h"
#include "doublearea.h"
#include "slim.h"

using namespace Eigen;

namespace slim {

static void correct_geometry_size(double surface_area_to_map_area_ratio,
                                  MatrixXd &vertex_positions,
                                  double desired_surface_area_to_map_ration)
{
  BLI_assert(surface_area_to_map_area_ratio > 0);
  double sqrt_of_ratio = sqrt(surface_area_to_map_area_ratio / desired_surface_area_to_map_ration);
  vertex_positions = vertex_positions / sqrt_of_ratio;
}

template<typename VertexPositionType, typename FaceIndicesType>
static double compute_surface_area(const VertexPositionType v, const FaceIndicesType f)
{
  Eigen::VectorXd doubled_area_of_triangles;
  doublearea(v, f, doubled_area_of_triangles);
  double area_of_map = doubled_area_of_triangles.sum() / 2;
  return area_of_map;
}

void correct_map_surface_area_if_necessary(SLIMData &slim_data)
{
  if (!slim_data.valid) {
    return;
  }

  bool mesh_surface_area_was_corrected = (slim_data.expectedSurfaceAreaOfResultingMap != 0);
  int number_of_pinned_vertices = slim_data.b.rows();
  bool no_pinned_vertices_exist = number_of_pinned_vertices == 0;

  bool needs_area_correction = mesh_surface_area_was_corrected && no_pinned_vertices_exist;
  if (!needs_area_correction) {
    return;
  }

  double area_ofresulting_map = compute_surface_area(slim_data.V_o, slim_data.F);
  if (!area_ofresulting_map) {
    return;
  }

  double resulting_area_to_expected_area_ratio = area_ofresulting_map /
                                                 slim_data.expectedSurfaceAreaOfResultingMap;
  double desired_ratio = 1.0;
  correct_geometry_size(resulting_area_to_expected_area_ratio, slim_data.V_o, desired_ratio);
}

void correct_mesh_surface_area_if_necessary(SLIMData &slim_data)
{
  BLI_assert(slim_data.valid);

  int number_of_pinned_vertices = slim_data.b.rows();
  bool pinned_vertices_exist = number_of_pinned_vertices > 0;
  bool needs_area_correction = slim_data.skipInitialization || pinned_vertices_exist;

  if (!needs_area_correction) {
    return;
  }

  double area_of_preinitialized_map = compute_surface_area(slim_data.V_o, slim_data.F);
  if (!area_of_preinitialized_map) {
    return;
  }

  if (area_of_preinitialized_map < 0) {
    area_of_preinitialized_map *= -1;
  }

  slim_data.expectedSurfaceAreaOfResultingMap = area_of_preinitialized_map;
  double surface_area_of3d_mesh = compute_surface_area(slim_data.V, slim_data.F);
  double surface_area_to_map_area_ratio = surface_area_of3d_mesh / area_of_preinitialized_map;

  double desired_ratio = 1.0;
  correct_geometry_size(surface_area_to_map_area_ratio, slim_data.V, desired_ratio);
}

}  // namespace slim
