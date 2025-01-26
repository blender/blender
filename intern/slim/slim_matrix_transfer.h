/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <memory>
#include <vector>

namespace slim {

struct SLIMData;

using SLIMDataPtr = std::unique_ptr<SLIMData>;

/**
 * MatrixTransferChart holds all information and data matrices to be
 * transferred from Blender to SLIM.
 */
struct MatrixTransferChart {
  int verts_num = 0;
  int faces_num = 0;
  int pinned_vertices_num = 0;
  int boundary_vertices_num = 0;
  int edges_num = 0;

  /** Field indicating whether a given SLIM operation succeeded or not. */
  bool succeeded = false;

  /** Vertex positions (matrix [verts_num x 3] in a linearized form). */
  std::vector<double> v_matrices;
  /** UV positions of vertices (matrix [verts_num x 2] in a linearized form). */
  std::vector<double> uv_matrices;
  /** Positions of pinned vertices (matrix [pinned_vertices_num x 2] in a linearized form). */
  std::vector<double> pp_matrices;
  /** Edge lengths. */
  std::vector<double> el_vectors;
  /** Weights per vertex. */
  std::vector<float> w_vectors;

  /** Vertex index triplets making up faces (matrix [faces_num x 3] in a linearized form). */
  std::vector<int> f_matrices;
  /** Indices of pinned vertices. */
  std::vector<int> p_matrices;
  /** Vertex index tuples making up edges (matrix [edges_num x 2] in a linearized form). */
  std::vector<int> e_matrices;
  /** Vertex indices of boundary vertices. */
  std::vector<int> b_vectors;

  SLIMDataPtr data;

  MatrixTransferChart();
  MatrixTransferChart(MatrixTransferChart &&);

  MatrixTransferChart(const MatrixTransferChart &) = delete;
  MatrixTransferChart &operator=(const MatrixTransferChart &) = delete;

  ~MatrixTransferChart();

  void try_slim_solve(int iter_num);
  /** Executes a single iteration of SLIM, must follow a proper setup & initialization. */
  void parametrize_single_iteration();
  /**
   * Called from the native part during each iteration of interactive parametrization.
   * The blend parameter decides the linear blending between the original UV map and the one
   * obtained from the accumulated SLIM iterations so far.
   */
  void transfer_uvs_blended(float blend);
  void free_slim_data();
};

struct PinnedVertexData {
  std::vector<int> pinned_vertex_indices;
  std::vector<double> pinned_vertex_positions_2D;
  std::vector<int> selected_pins;
};

struct MatrixTransfer {
  bool fixed_boundary = false;
  bool use_weights = false;
  double weight_influence = 0.0;
  int reflection_mode = 0;
  int n_iterations = 0;
  bool skip_initialization = false;
  bool is_minimize_stretch = false;

  std::vector<MatrixTransferChart> charts;

  /** Used for pins update in live unwrap. */
  PinnedVertexData pinned_vertex_data;

  MatrixTransfer();
  MatrixTransfer(const MatrixTransfer &) = delete;
  MatrixTransfer &operator=(const MatrixTransfer &) = delete;
  ~MatrixTransfer();

  void parametrize();

  /** Executes slim iterations during live unwrap. needs to provide new selected-pin positions. */
  void parametrize_live(MatrixTransferChart &chart, const PinnedVertexData &pinned_vertex_data);

  /** Transfers all the matrices from the native part and initializes SLIM. */
  void setup_slim_data(MatrixTransferChart &chart) const;
};

}  // namespace slim
