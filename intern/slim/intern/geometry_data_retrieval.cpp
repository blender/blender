/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include "geometry_data_retrieval.h"

#include <Eigen/Dense>

#include "BLI_assert.h"

#include "slim.h"
#include "slim_matrix_transfer.h"

#include "area_compensation.h"
#include "least_squares_relocator.h"
#include "uv_initializer.h"

using namespace Eigen;

namespace slim {

GeometryData::GeometryData(const MatrixTransfer &mt, MatrixTransferChart &chart)
    : number_of_vertices(chart.verts_num),
      number_of_faces(chart.faces_num),
      /* `n_edges` in transferred_data accounts for boundary edges only once. */
      number_of_edges_twice(chart.edges_num + chart.boundary_vertices_num),
      number_of_boundary_vertices(chart.boundary_vertices_num),
      number_of_pinned_vertices(chart.pinned_vertices_num),
      use_weights(mt.use_weights),
      weight_influence(mt.weight_influence),
      vertex_positions3d(chart.v_matrices.data(), number_of_vertices, columns_3),
      uv_positions2d(chart.uv_matrices.data(), number_of_vertices, columns_2),
      positions_of_pinned_vertices2d(),
      positions_of_explicitly_pinned_vertices2d(
          number_of_pinned_vertices != 0 ? chart.pp_matrices.data() : nullptr,
          number_of_pinned_vertices,
          columns_2),
      faces_by_vertexindices(chart.f_matrices.data(), number_of_faces, columns_3),
      edges_by_vertexindices(chart.e_matrices.data(), number_of_edges_twice, columns_2),
      pinned_vertex_indices(),
      explicitly_pinned_vertex_indices(number_of_pinned_vertices != 0 ? chart.p_matrices.data() :
                                                                        nullptr,
                                       number_of_pinned_vertices),
      edge_lengths(chart.el_vectors.data(), number_of_edges_twice),
      boundary_vertex_indices(chart.b_vectors.data(), number_of_boundary_vertices),
      weights_per_vertex(chart.w_vectors.data(), number_of_vertices)
{
  retrieve_pinned_vertices(mt.fixed_boundary);
}

static void create_weights_per_face(SLIMData &slim_data)
{
  if (!slim_data.valid) {
    return;
  }

  if (!slim_data.withWeightedParameterization) {
    slim_data.weightPerFaceMap = Eigen::VectorXf::Ones(slim_data.F.rows());
    return;
  }

  slim_data.weightPerFaceMap = Eigen::VectorXf(slim_data.F.rows());

  /* The actual weight is `max_factor ^ (2 * (mean - 0.5))` */
  int weight_influence_sign = (slim_data.weightInfluence >= 0) ? 1 : -1;
  double max_factor = std::abs(slim_data.weightInfluence) + 1;

  for (int fid = 0; fid < slim_data.F.rows(); fid++) {
    Eigen::RowVector3i row = slim_data.F.row(fid);
    float w1, w2, w3, mean, weight_factor, flipped_mean;
    w1 = slim_data.weightmap(row(0));
    w2 = slim_data.weightmap(row(1));
    w3 = slim_data.weightmap(row(2));
    mean = (w1 + w2 + w3) / 3;
    flipped_mean = 1 - mean;

    weight_factor = std::pow(max_factor, weight_influence_sign * 2 * (flipped_mean - 0.5));
    slim_data.weightPerFaceMap(fid) = weight_factor;
  }
}

void GeometryData::set_geometry_data_matrices(SLIMData &slim_data) const
{
  if (!slim_data.valid) {
    return;
  }

  slim_data.V = vertex_positions3d;
  slim_data.F = faces_by_vertexindices;
  slim_data.b = pinned_vertex_indices;
  slim_data.bc = positions_of_pinned_vertices2d;
  slim_data.V_o = uv_positions2d;
  slim_data.oldUVs = uv_positions2d;
  slim_data.weightmap = weights_per_vertex;
  create_weights_per_face(slim_data);
}

bool GeometryData::has_valid_preinitialized_map() const
{
  if (uv_positions2d.rows() == vertex_positions3d.rows() && uv_positions2d.cols() == columns_2) {

    int number_of_flips = count_flips(faces_by_vertexindices, uv_positions2d);
    bool no_flips_present = (number_of_flips == 0);
    return (no_flips_present);
  }
  return false;
}

/* If we use interactive parametrisation, we usually start form an existing, flip-free unwrapping.
 * Also, pinning of vertices has some issues with initialisation with convex border.
 * We therefore may want to skip initialization. however, to skip initialization we need a
 * preexisting valid starting map. */
bool GeometryData::can_initialization_be_skipped(bool skip_initialization) const
{
  return (skip_initialization && has_valid_preinitialized_map());
}

void GeometryData::construct_slim_data(SLIMData &slim_data,
                                       bool skip_initialization,
                                       int reflection_mode) const
{
  BLI_assert(slim_data.valid);

  slim_data.skipInitialization = can_initialization_be_skipped(skip_initialization);
  slim_data.weightInfluence = weight_influence;
  slim_data.reflection_mode = reflection_mode;
  slim_data.withWeightedParameterization = use_weights;
  set_geometry_data_matrices(slim_data);

  double penalty_for_violating_pinned_positions = 10.0e100;
  slim_data.soft_const_p = penalty_for_violating_pinned_positions;
  slim_data.slim_energy = SLIMData::SYMMETRIC_DIRICHLET;

  initialize_if_needed(slim_data);

  transform_initialization_if_necessary(slim_data);
  correct_mesh_surface_area_if_necessary(slim_data);

  slim_precompute(slim_data.V,
                  slim_data.F,
                  slim_data.V_o,
                  slim_data,
                  slim_data.slim_energy,
                  slim_data.b,
                  slim_data.bc,
                  slim_data.soft_const_p);
}

void GeometryData::combine_matrices_of_pinned_and_boundary_vertices()
{
  /* Over - allocate pessimistically to avoid multiple reallocation. */
  int upper_bound_on_number_of_pinned_vertices = number_of_boundary_vertices +
                                                 number_of_pinned_vertices;
  pinned_vertex_indices = VectorXi(upper_bound_on_number_of_pinned_vertices);
  positions_of_pinned_vertices2d = MatrixXd(upper_bound_on_number_of_pinned_vertices, columns_2);

  /* Since border vertices use vertex indices 0 ... #bordervertices we can do: */
  pinned_vertex_indices.segment(0, number_of_boundary_vertices) = boundary_vertex_indices;
  positions_of_pinned_vertices2d.block(0, 0, number_of_boundary_vertices, columns_2) =
      uv_positions2d.block(0, 0, number_of_boundary_vertices, columns_2);

  int index = number_of_boundary_vertices;
  int highest_vertex_index = (boundary_vertex_indices)(index - 1);

  for (Map<VectorXi>::InnerIterator it(explicitly_pinned_vertex_indices, 0); it; ++it) {
    int vertex_index = it.value();
    if (vertex_index > highest_vertex_index) {
      pinned_vertex_indices(index) = vertex_index;
      positions_of_pinned_vertices2d.row(index) = uv_positions2d.row(vertex_index);
      index++;
    }
  }

  int actual_number_of_pinned_vertices = index;
  pinned_vertex_indices.conservativeResize(actual_number_of_pinned_vertices);
  positions_of_pinned_vertices2d.conservativeResize(actual_number_of_pinned_vertices, columns_2);

  number_of_pinned_vertices = actual_number_of_pinned_vertices;
}

/* If the border is fixed, we simply pin the border vertices additionally to other pinned vertices.
 */
void GeometryData::retrieve_pinned_vertices(bool border_vertices_are_pinned)
{
  if (border_vertices_are_pinned) {
    combine_matrices_of_pinned_and_boundary_vertices();
  }
  else {
    pinned_vertex_indices = VectorXi(explicitly_pinned_vertex_indices);
    positions_of_pinned_vertices2d = MatrixXd(positions_of_explicitly_pinned_vertices2d);
  }
}

void GeometryData::initialize_if_needed(SLIMData &slim_data) const
{
  BLI_assert(slim_data.valid);

  if (!slim_data.skipInitialization) {
    initialize_uvs(slim_data);
  }
}

void GeometryData::initialize_uvs(SLIMData &slim_data) const
{
  Eigen::MatrixXd uv_positions_of_boundary(boundary_vertex_indices.rows(), 2);
  map_vertices_to_convex_border(uv_positions_of_boundary);

  bool all_vertices_on_boundary = (slim_data.V_o.rows() == uv_positions_of_boundary.rows());
  if (all_vertices_on_boundary) {
    slim_data.V_o = uv_positions_of_boundary;
    return;
  }

  mvc(faces_by_vertexindices,
      vertex_positions3d,
      edges_by_vertexindices,
      edge_lengths,
      boundary_vertex_indices,
      uv_positions_of_boundary,
      slim_data.V_o);
}

}  // namespace slim
