/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include <cstdlib>

#include "slim.h"
#include "slim_matrix_transfer.h"

#include "area_compensation.h"

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

namespace slim {

using namespace Eigen;

static void transfer_uvs_back_to_native_part(MatrixTransferChart &chart, Eigen::MatrixXd &uv)
{
  if (!chart.succeeded) {
    return;
  }

  auto &uv_coordinate_array = chart.uv_matrices;
  int number_of_vertices = chart.verts_num;

  for (int i = 0; i < number_of_vertices; i++) {
    uv_coordinate_array[i] = uv(i, 0);
    uv_coordinate_array[number_of_vertices + i] = uv(i, 1);
  }
}

static Eigen::MatrixXd get_interactive_result_blended_with_original(float blend,
                                                                    const SLIMData &slim_data)
{
  Eigen::MatrixXd original_map_weighted = blend * slim_data.oldUVs;
  Eigen::MatrixXd interactive_result_map = (1.0 - blend) * slim_data.V_o;
  return original_map_weighted + interactive_result_map;
}

static void adjust_pins(SLIMData &slim_data, const PinnedVertexData &pinned_vertex_data)
{
  if (!slim_data.valid) {
    return;
  }

  const auto &pinned_vertex_indices = pinned_vertex_data.pinned_vertex_indices;
  const auto &pinned_vertex_positions_2D = pinned_vertex_data.pinned_vertex_positions_2D;
  const auto &selected_pins = pinned_vertex_data.selected_pins;

  int n_pins = pinned_vertex_indices.size();
  int n_selected_pins = selected_pins.size();

  Eigen::VectorXi old_pin_indices = slim_data.b;
  Eigen::MatrixXd old_pin_positions = slim_data.bc;

  slim_data.b.resize(n_pins);
  slim_data.bc.resize(n_pins, 2);

  int old_pin_pointer = 0;
  int selected_pin_pointer = 0;

  for (int new_pin_pointer = 0; new_pin_pointer < n_pins; new_pin_pointer++) {

    int pinned_vertex_index = pinned_vertex_indices[new_pin_pointer];
    slim_data.b(new_pin_pointer) = pinned_vertex_index;

    while ((old_pin_pointer < old_pin_indices.size()) &&
           (old_pin_indices(old_pin_pointer) < pinned_vertex_index))
    {
      ++old_pin_pointer;
    }
    bool old_pointer_valid = (old_pin_pointer < old_pin_indices.size()) &&
                             (old_pin_indices(old_pin_pointer) == pinned_vertex_index);

    while ((selected_pin_pointer < n_selected_pins) &&
           (selected_pins[selected_pin_pointer] < pinned_vertex_index))
    {
      ++selected_pin_pointer;
    }
    bool pin_selected = (selected_pin_pointer < n_selected_pins) &&
                        (selected_pins[selected_pin_pointer] == pinned_vertex_index);

    if (!pin_selected && old_pointer_valid) {
      slim_data.bc.row(new_pin_pointer) = old_pin_positions.row(old_pin_pointer);
    }
    else {
      slim_data.bc(new_pin_pointer, 0) = pinned_vertex_positions_2D[2 * new_pin_pointer];
      slim_data.bc(new_pin_pointer, 1) = pinned_vertex_positions_2D[2 * new_pin_pointer + 1];
    }
  }
}

void MatrixTransferChart::transfer_uvs_blended(float blend)
{
  if (!succeeded) {
    return;
  }

  Eigen::MatrixXd blended_uvs = get_interactive_result_blended_with_original(blend, *data);
  correct_map_surface_area_if_necessary(*data);
  transfer_uvs_back_to_native_part(*this, blended_uvs);
}

void MatrixTransferChart::try_slim_solve(int iter_num)
{
  if (!succeeded) {
    return;
  }

  try {
    slim_solve(*data, iter_num);
  }
  catch (SlimFailedException &) {
    succeeded = false;
  }
}

void MatrixTransferChart::parametrize_single_iteration()
{
  int number_of_iterations = 1;
  try_slim_solve(number_of_iterations);
}

void MatrixTransfer::parametrize_live(MatrixTransferChart &chart,
                                      const PinnedVertexData &pinned_vertex_data)
{
  int number_of_iterations = 3;
  adjust_pins(*chart.data, pinned_vertex_data);

  chart.try_slim_solve(number_of_iterations);

  correct_map_surface_area_if_necessary(*chart.data);
  transfer_uvs_back_to_native_part(chart, chart.data->V_o);
}

void MatrixTransfer::parametrize()
{
  for (MatrixTransferChart &chart : charts) {
    setup_slim_data(chart);

    chart.try_slim_solve(n_iterations);

    correct_map_surface_area_if_necessary(*chart.data);
    transfer_uvs_back_to_native_part(chart, chart.data->V_o);

    chart.free_slim_data();
  }
}

}  // namespace slim
