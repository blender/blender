/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <Eigen/Dense>

#include "slim.h"
#include "slim_matrix_transfer.h"

using namespace Eigen;

namespace slim {

struct GeometryData {
  int columns_2 = 2;
  int columns_3 = 3;
  int number_of_vertices = 0;
  int number_of_faces = 0;
  int number_of_edges_twice = 0;
  int number_of_boundary_vertices = 0;
  int number_of_pinned_vertices = 0;

  bool use_weights = false;
  double weight_influence = 0.0;

  /* All the following maps have to be declared as last members. */
  Map<MatrixXd> vertex_positions3d = Map<MatrixXd>(nullptr, 0, 0);
  Map<MatrixXd> uv_positions2d = Map<MatrixXd>(nullptr, 0, 0);
  MatrixXd positions_of_pinned_vertices2d;
  Map<Matrix<double, Dynamic, Dynamic, RowMajor>> positions_of_explicitly_pinned_vertices2d =
      Map<Matrix<double, Dynamic, Dynamic, RowMajor>>(nullptr, 0, 0);

  Map<MatrixXi> faces_by_vertexindices = Map<MatrixXi>(nullptr, 0, 0);
  Map<MatrixXi> edges_by_vertexindices = Map<MatrixXi>(nullptr, 0, 0);
  VectorXi pinned_vertex_indices;
  Map<VectorXi> explicitly_pinned_vertex_indices = Map<VectorXi>(nullptr, 0);

  Map<VectorXd> edge_lengths = Map<VectorXd>(nullptr, 0);
  Map<VectorXi> boundary_vertex_indices = Map<VectorXi>(nullptr, 0);
  Map<VectorXf> weights_per_vertex = Map<VectorXf>(nullptr, 0);

  GeometryData(const MatrixTransfer &mt, MatrixTransferChart &chart);
  GeometryData(const GeometryData &) = delete;
  GeometryData &operator=(const GeometryData &) = delete;

  void construct_slim_data(SLIMData &slim_data,
                           bool skip_initialization,
                           int reflection_mode) const;

  void retrieve_pinned_vertices(bool border_vertices_are_pinned);

 private:
  void set_geometry_data_matrices(SLIMData &slim_data) const;
  bool has_valid_preinitialized_map() const;
  bool can_initialization_be_skipped(bool skip_initialization) const;
  void combine_matrices_of_pinned_and_boundary_vertices();
  void initialize_if_needed(SLIMData &slim_data) const;
  void initialize_uvs(SLIMData &slim_data) const;
};

}  // namespace slim
