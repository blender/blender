/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Relaxes vertices along edge loops so they are smoother.
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"

#include "BLI_array_utils.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_listbase.hh"
#include "BLI_math_solvers.hh"
#include "BLI_vector.hh"
#include <array>

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

/**
 * Defines which vertices stay still to define the shape
 * and which vertices are actively being relaxed.
 */
struct RelaxPhase {
  /** Indices of vertices used as static anchors for the spline. */
  Vector<int> knot_indices;
  /** Indices of vertices whose positions are being updated. */
  Vector<int> point_indices;
};

/**
 * A chain of vertices collected from a walk along connected edges.
 */
struct RelaxChainData {
  /** Ordered vertices along the chain path. */
  Vector<BMVert *> verts;
  /** True if the path forms a closed chain. */
  bool is_closed = false;
  /** Cached relax phases. */
  Vector<RelaxPhase> phases;
};

/**
 * Coefficients for the cubic spline curve equation.
 */
struct SplineCoeffs {
  /** Value at the start of the segment. */
  float a;
  /** First-order coefficient. */
  float b;
  /** Second-order coefficient. */
  float c;
  /** Third-order coefficient. */
  float d;
  /** Parameter value at the start of the segment. */
  float x;
};

/** Epsilon to prevent zero division. */
constexpr float RELAX_EPSILON = 1e-8f;

/**
 * Compute cubic spline coefficients for one coordinate axis.
 * Uses `BLI_tridiagonal_solve` for open chains and
 * `BLI_tridiagonal_solve_cyclic` for closed loops.
 */
static void calculate_splines_axis(Span<float> distances,
                                   Span<float> coords,
                                   const bool is_closed,
                                   Vector<SplineCoeffs> &r_coeffs)
{
  const int verts_num = coords.size();
  if (verts_num < 2) {
    return;
  }
  const int segments_num = is_closed ? verts_num : verts_num - 1;
  Array<float> segment_length(segments_num);

  for (const int i : IndexRange(segments_num)) {
    segment_length[i] = distances[i + 1] - distances[i];
    if (!(segment_length[i] > 0.0f)) {
      segment_length[i] = RELAX_EPSILON;
    }
  }

  /* Stores second derivative coefficients. For a natural cubic spline, the boundary
   * condition defines the first and last points as zero. */
  Array<float> c_vals(verts_num, 0.0f);

  /* The Thomas algorithm used in `BLI_tridiagonal_solve` can't properly solve
   * a cyclic tridiagonal system so in this case, we use the Sherman-Morrison formula
   * via `BLI_tridiagonal_solve_cyclic`. */
  if (is_closed) {
    Array<float> lower_diag(verts_num);
    Array<float> diag(verts_num);
    Array<float> upper_diag(verts_num);
    Array<float> rhs(verts_num);
    for (const int i : IndexRange(verts_num)) {
      const int i_prev = math::mod_periodic(i - 1, verts_num);
      const int i_next = math::mod_periodic(i + 1, verts_num);
      lower_diag[i] = segment_length[i_prev];
      diag[i] = 2.0f * (segment_length[i_prev] + segment_length[i]);
      upper_diag[i] = segment_length[i];
      rhs[i] = 3.0f * (((coords[i_next] - coords[i]) / segment_length[i]) -
                       ((coords[i] - coords[i_prev]) / segment_length[i_prev]));
    }
    BLI_tridiagonal_solve_cyclic(
        lower_diag.data(), diag.data(), upper_diag.data(), rhs.data(), c_vals.data(), verts_num);
  }
  else {
    /* For a natural cubic spline the curvature at the first and last point
     * is 0, so for n given points, we only have n-2 unknown interior points. */
    const int interior = verts_num - 2;
    Array<float> lower_diag(interior);
    Array<float> diag(interior);
    Array<float> upper_diag(interior);
    Array<float> rhs(interior);

    for (const int i_curr : IndexRange(interior)) {
      const int i_next = i_curr + 1;
      lower_diag[i_curr] = segment_length[i_curr];
      diag[i_curr] = 2.0f * (segment_length[i_curr] + segment_length[i_next]);
      upper_diag[i_curr] = segment_length[i_next];
      rhs[i_curr] = 3.0f * (((coords[i_next + 1] - coords[i_next]) / segment_length[i_next]) -
                            ((coords[i_next] - coords[i_curr]) / segment_length[i_curr]));
    }
    BLI_tridiagonal_solve(lower_diag.data(),
                          diag.data(),
                          upper_diag.data(),
                          rhs.data(),
                          c_vals.data() + 1,
                          interior);
  }

  /* Build polynomial coefficients for each segment. */
  for (const int i : IndexRange(segments_num)) {
    const int i_next = is_closed ? math::mod_periodic(i + 1, verts_num) : i + 1;

    const float coeff_a = coords[i];
    const float coeff_b = ((coords[i_next] - coords[i]) / segment_length[i]) -
                          (segment_length[i] * (c_vals[i_next] + 2.0f * c_vals[i])) / 3.0f;
    const float coeff_c = c_vals[i];
    const float coeff_d = (c_vals[i_next] - c_vals[i]) / (3.0f * segment_length[i]);
    r_coeffs.append({coeff_a, coeff_b, coeff_c, coeff_d, distances[i]});
  }
}

static void build_relax_phases(int verts_num, bool is_closed, Vector<RelaxPhase> &r_phases)
{
  if (!is_closed) {
    /* There are two relax phases, in the first phase, odd vertices are relaxed
     * and even ones are not(they're knots in this case), in second phase, even vertices
     * are relaxed and odd ones are not. The first and last vertices are not included to
     * be moved. */
    for (const int phase_index : IndexRange(2)) {
      RelaxPhase phase;
      for (const int i : IndexRange(verts_num)) {
        if (i % 2 == phase_index) {
          phase.knot_indices.append(i);
        }
        else if (i > 0 && i < verts_num - 1) {
          phase.point_indices.append(i);
        }
      }
      r_phases.append(std::move(phase));
    }
    return;
  }

  Vector<int> vert_indices(verts_num);
  array_utils::fill_index_range(vert_indices.as_mutable_span());

  for (const int j : IndexRange(2)) {
    const bool extend = verts_num % 2 == 1 ? j == 1 : j == 0;
    const int knot_start = !extend && j == 1 ? 1 : 0;
    const int point_start = !extend && j == 1 ? 2 : 1;

    if (extend) {
      const int last_vert = vert_indices.last();
      const int first_vert = vert_indices.first();
      vert_indices.insert(0, last_vert);
      vert_indices.append(first_vert);
    }

    RelaxPhase phase;
    for (int i = knot_start; i < vert_indices.size(); i += 2) {
      phase.knot_indices.append(vert_indices[i]);
    }
    for (int i = point_start; i < vert_indices.size(); i += 2) {
      const int val = vert_indices[i];
      if (phase.point_indices.is_empty() || val != phase.point_indices.first()) {
        phase.point_indices.append(val);
      }
    }
    if (phase.knot_indices.first() != phase.knot_indices.last()) {
      phase.knot_indices.append(phase.knot_indices.first());
    }

    if (!phase.point_indices.is_empty()) {
      r_phases.append(std::move(phase));
    }
  }
}

static bool bm_edge_relax_test_cb(BMEdge *e, void * /*user_data*/)
{
  return BM_elem_flag_test(e, BM_ELEM_TAG);
}

static void get_relax_input_chains(BMesh *bm, Vector<RelaxChainData> &r_chains)
{
  ListBaseT<BMEdgeLoopStore> eloops = {nullptr};
  const BMEdgeLoopFind_Params params = {
      .use_vert_junction = true,
  };
  BM_mesh_edgeloops_find(bm, &eloops, bm_edge_relax_test_cb, nullptr, &params);

  for (BMEdgeLoopStore &el_store : eloops) {
    RelaxChainData chain;
    chain.is_closed = BM_edgeloop_is_closed(&el_store);
    for (LinkData &node : *BM_edgeloop_verts_get(&el_store)) {
      chain.verts.append(static_cast<BMVert *>(node.data));
    }
    if (chain.verts.size() >= 3) {
      r_chains.append(std::move(chain));
    }
  }

  BM_mesh_edgeloops_free(&eloops);
}

static void calculate_phase_distances(Span<BMVert *> verts,
                                      const RelaxPhase &phase,
                                      const bool regular,
                                      Vector<float> &r_knot_distances,
                                      Vector<float> &r_point_distances)
{
  const int knots_num = phase.knot_indices.size();
  const int points_num = phase.point_indices.size();
  const int total = knots_num + points_num;

  Array<float3> positions(total);
  for (const int i : IndexRange(total)) {
    int vert_index;
    if (i % 2 == 0) {
      vert_index = phase.knot_indices[i / 2];
    }
    else if (i == total - 1) {
      vert_index = phase.knot_indices.last();
    }
    else {
      vert_index = phase.point_indices[i / 2];
    }
    positions[i] = verts[vert_index]->co;
  }

  Array<float> cumulative(total);
  cumulative[0] = 0.0f;
  length_parameterize::accumulate_lengths<float3>(
      positions, false, cumulative.as_mutable_span().drop_front(1));

  for (const int i : IndexRange(total)) {
    if (i % 2 == 0 || i == total - 1) {
      r_knot_distances.append(cumulative[i]);
    }
    else {
      r_point_distances.append(cumulative[i]);
    }
  }

  /* Place a point halfway between two knots if regular is enabled. */
  if (regular) {
    r_point_distances.clear();
    for (const int p : IndexRange(points_num)) {
      r_point_distances.append((r_knot_distances[p] + r_knot_distances[p + 1]) / 2.0f);
    }
  }
}

static void calculate_relax_splines(Span<BMVert *> verts,
                                    Span<int> knot_indices,
                                    Span<float> t_params,
                                    bool is_closed,
                                    std::array<Vector<SplineCoeffs>, 3> &r_coeffs)
{
  const int knots_num = knot_indices.size();
  const int coords_num = is_closed ? knots_num - 1 : knots_num;
  Array<float> coords_x(coords_num);
  Array<float> coords_y(coords_num);
  Array<float> coords_z(coords_num);

  for (const int i : IndexRange(coords_num)) {
    const float *co = verts[knot_indices[i]]->co;
    coords_x[i] = co[0];
    coords_y[i] = co[1];
    coords_z[i] = co[2];
  }

  calculate_splines_axis(t_params, coords_x, is_closed, r_coeffs[0]);
  calculate_splines_axis(t_params, coords_y, is_closed, r_coeffs[1]);
  calculate_splines_axis(t_params, coords_z, is_closed, r_coeffs[2]);
}

static void execute_relax_phase(
    Span<BMVert *> verts, const RelaxPhase &phase, bool is_closed, int interpolation, bool regular)
{
  if (phase.point_indices.is_empty()) {
    return;
  }

  Vector<float> knot_distances;
  Vector<float> point_distances;
  calculate_phase_distances(verts, phase, regular, knot_distances, point_distances);

  const Span<float> accumulated_lengths = Span<float>(knot_distances).drop_front(1);

  const int points_num = phase.point_indices.size();
  Array<int> segment_indices(points_num);
  Array<float> factors(points_num);
  length_parameterize::sample_at_lengths(
      accumulated_lengths, point_distances, segment_indices, factors);

  Array<float3> sampled_positions(points_num);

  if (interpolation == RELAX_EDGE_LOOPS_INTERP_LINEAR) {
    Array<float3> knot_positions(phase.knot_indices.size());
    for (const int i : phase.knot_indices.index_range()) {
      knot_positions[i] = verts[phase.knot_indices[i]]->co;
    }

    length_parameterize::interpolate<float3>(
        knot_positions, segment_indices, factors, sampled_positions);
  }
  else {
    std::array<Vector<SplineCoeffs>, 3> axis_coeffs;
    calculate_relax_splines(verts, phase.knot_indices, knot_distances, is_closed, axis_coeffs);

    for (const int i : IndexRange(points_num)) {
      const int seg = segment_indices[i];
      const float dt = point_distances[i] - axis_coeffs[0][seg].x;

      const SplineCoeffs &cx = axis_coeffs[0][seg];
      const SplineCoeffs &cy = axis_coeffs[1][seg];
      const SplineCoeffs &cz = axis_coeffs[2][seg];

      sampled_positions[i] = float3(cx.a + dt * (cx.b + dt * (cx.c + dt * cx.d)),
                                    cy.a + dt * (cy.b + dt * (cy.c + dt * cy.d)),
                                    cz.a + dt * (cz.b + dt * (cz.c + dt * cz.d)));
    }
  }

  for (const int i : IndexRange(points_num)) {
    const int v_index = phase.point_indices[i];
    const float3 current_pos(verts[v_index]->co);
    const float3 final_pos = (current_pos + sampled_positions[i]) / 2.0f;
    copy_v3_v3(verts[v_index]->co, final_pos);
  }
}

void bmo_relax_edge_loops_exec(BMesh *bm, BMOperator *op)
{
  const int iterations = BMO_slot_int_get(op->slots_in, "iterations");
  const int interpolation = BMO_slot_int_get(op->slots_in, "interpolation");
  const bool even_spacing = BMO_slot_bool_get(op->slots_in, "even_spacing");

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_EDGE, BM_ELEM_TAG, false);

  Vector<RelaxChainData> chains;
  get_relax_input_chains(bm, chains);

  for (RelaxChainData &chain : chains) {
    build_relax_phases(chain.verts.size(), chain.is_closed, chain.phases);
  }

  for (const int it : IndexRange(iterations)) {
    UNUSED_VARS(it);

    for (RelaxChainData &chain : chains) {
      for (const RelaxPhase &phase : chain.phases) {
        execute_relax_phase(chain.verts, phase, chain.is_closed, interpolation, even_spacing);
      }
    }
  }
}

}  // namespace blender
