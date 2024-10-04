/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include "least_squares_relocator.h"

#include "slim.h"

#include <Eigen/Dense>

#include "BLI_assert.h"

namespace slim {

using namespace Eigen;

static void apply_transformation(SLIMData &slim_data, Matrix2d &transformation_matrix)
{
  BLI_assert(slim_data.valid);

  for (int i = 0; i < slim_data.V_o.rows(); i++) {
    slim_data.V_o.row(i) = transformation_matrix * slim_data.V_o.row(i).transpose();
  }
}

static void apply_translation(SLIMData &slim_data, Vector2d &translation_vector)
{
  BLI_assert(slim_data.valid);

  for (int i = 0; i < slim_data.V_o.rows(); i++) {
    slim_data.V_o.row(i) = translation_vector.transpose() + slim_data.V_o.row(i);
  }
}

static void retrieve_positions_of_pinned_vertices_in_initialization(
    const MatrixXd &all_uv_positions_in_initialization,
    const VectorXi &indices_of_pinned_vertices,
    MatrixXd &position_of_pinned_vertices_in_initialization)
{
  int i = 0;
  for (VectorXi::InnerIterator it(indices_of_pinned_vertices, 0); it; ++it, i++) {
    int vertex_index = it.value();
    position_of_pinned_vertices_in_initialization.row(i) = all_uv_positions_in_initialization.row(
        vertex_index);
  }
}

static void flip_input_geometry(SLIMData &slim_data)
{
  BLI_assert(slim_data.valid);

  VectorXi temp = slim_data.F.col(0);
  slim_data.F.col(0) = slim_data.F.col(2);
  slim_data.F.col(2) = temp;
}

static void compute_centroid(const MatrixXd &point_cloud, Vector2d &centroid)
{
  centroid << point_cloud.col(0).sum(), point_cloud.col(1).sum();
  centroid /= point_cloud.rows();
}

/* Finds scaling matrix:
 *
 * T = |a 0|
 *     |0 a|
 *
 * s.t. if to each point p in the inizialized map the following is applied
 *
 *  T*p
 *
 * We get the closest scaling of the positions of the vertices in the initialized map to the pinned
 * vertices in a least squares sense. We find them by solving
 *
 * argmin_{t}	At = p
 *
 * i.e.:
 *
 * | x_1 |           |u_1|
 * |  .  |           | . |
 * |  .  |           | . |
 * | x_n |           |u_n|
 * | y_1 | * | a | = |v_1|
 * |  .  |           | . |
 * |  .  |           | . |
 * | y_n |           |v_n|
 *
 * `t` is of dimension `1 x 1` and `p` of dimension `2*numberOfPinnedVertices x 1`
 * is the vector holding the uv positions of the pinned vertices. */
static void compute_least_squares_scaling(MatrixXd centered_pins,
                                          MatrixXd centered_initialized_pins,
                                          Matrix2d &transformation_matrix)
{
  int number_of_pinned_vertices = centered_pins.rows();

  MatrixXd a = MatrixXd::Zero(number_of_pinned_vertices * 2, 1);
  a << centered_initialized_pins.col(0), centered_initialized_pins.col(1);

  VectorXd p(2 * number_of_pinned_vertices);
  p << centered_pins.col(0), centered_pins.col(1);

  VectorXd t = a.colPivHouseholderQr().solve(p);
  t(0) = abs(t(0));
  transformation_matrix << t(0), 0, 0, t(0);
}

static void comput_least_squares_rotation_scale_only(SLIMData &slim_data,
                                                     Vector2d &translation_vector,
                                                     Matrix2d &transformation_matrix,
                                                     bool is_flip_allowed)
{
  BLI_assert(slim_data.valid);

  MatrixXd position_of_initialized_pins(slim_data.b.rows(), 2);
  retrieve_positions_of_pinned_vertices_in_initialization(
      slim_data.V_o, slim_data.b, position_of_initialized_pins);

  Vector2d centroid_of_initialized;
  compute_centroid(position_of_initialized_pins, centroid_of_initialized);

  Vector2d centroid_of_pins;
  compute_centroid(slim_data.bc, centroid_of_pins);

  MatrixXd centered_initialized_pins = position_of_initialized_pins.rowwise().operator-(
      centroid_of_initialized.transpose());
  MatrixXd centeredpins = slim_data.bc.rowwise().operator-(centroid_of_pins.transpose());

  MatrixXd s = centered_initialized_pins.transpose() * centeredpins;

  JacobiSVD<MatrixXd> svd(s, ComputeFullU | ComputeFullV);

  Matrix2d vu_t = svd.matrixV() * svd.matrixU().transpose();

  Matrix2d singular_values = Matrix2d::Identity();

  bool contains_reflection = vu_t.determinant() < 0;
  if (contains_reflection) {
    if (!is_flip_allowed) {
      singular_values(1, 1) = vu_t.determinant();
    }
    else {
      flip_input_geometry(slim_data);
    }
  }

  compute_least_squares_scaling(centeredpins, centered_initialized_pins, transformation_matrix);

  transformation_matrix = transformation_matrix * svd.matrixV() * singular_values *
                          svd.matrixU().transpose();

  translation_vector = centroid_of_pins - transformation_matrix * centroid_of_initialized;
}

static void compute_transformation_matrix2_pins(const SLIMData &slim_data,
                                                Matrix2d &transformation_matrix)
{
  BLI_assert(slim_data.valid);

  Vector2d pinned_position_difference_vector = slim_data.bc.row(0) - slim_data.bc.row(1);
  Vector2d initialized_position_difference_vector = slim_data.V_o.row(slim_data.b(0)) -
                                                    slim_data.V_o.row(slim_data.b(1));

  double scale = pinned_position_difference_vector.norm() /
                 initialized_position_difference_vector.norm();

  pinned_position_difference_vector.normalize();
  initialized_position_difference_vector.normalize();

  /* TODO: sometimes rotates in wrong direction. */
  double cos_angle = pinned_position_difference_vector.dot(initialized_position_difference_vector);
  double sin_angle = sqrt(1 - pow(cos_angle, 2));

  transformation_matrix << cos_angle, -sin_angle, sin_angle, cos_angle;
  transformation_matrix = (Matrix2d::Identity() * scale) * transformation_matrix;
}

static void compute_translation1_pin(const SLIMData &slim_data, Vector2d &translation_vector)
{
  BLI_assert(slim_data.valid);
  translation_vector = slim_data.bc.row(0) - slim_data.V_o.row(slim_data.b(0));
}

static void transform_initialized_map(SLIMData &slim_data)
{
  BLI_assert(slim_data.valid);
  Matrix2d transformation_matrix;
  Vector2d translation_vector;

  int number_of_pinned_vertices = slim_data.b.rows();

  switch (number_of_pinned_vertices) {
    case 0:
      return;
    case 1: /* Only translation is needed with one pin. */
      compute_translation1_pin(slim_data, translation_vector);
      apply_translation(slim_data, translation_vector);
      break;
    case 2:
      compute_transformation_matrix2_pins(slim_data, transformation_matrix);
      apply_transformation(slim_data, transformation_matrix);
      compute_translation1_pin(slim_data, translation_vector);
      apply_translation(slim_data, translation_vector);
      break;
    default:

      bool flip_allowed = slim_data.reflection_mode == 0;

      comput_least_squares_rotation_scale_only(
          slim_data, translation_vector, transformation_matrix, flip_allowed);

      apply_transformation(slim_data, transformation_matrix);
      apply_translation(slim_data, translation_vector);

      break;
  }
}

static bool is_translation_needed(const SLIMData &slim_data)
{
  BLI_assert(slim_data.valid);
  bool pinned_vertices_exist = (slim_data.b.rows() > 0);
  bool was_initialized = !slim_data.skipInitialization;
  return was_initialized && pinned_vertices_exist;
}

void transform_initialization_if_necessary(SLIMData &slim_data)
{
  BLI_assert(slim_data.valid);

  if (!is_translation_needed(slim_data)) {
    return;
  }

  transform_initialized_map(slim_data);
}
}  // namespace slim
