/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Distributes vertices evenly along one or more edge loops.
 * Endpoints of edge loops are not modified unless the loop is
 * cyclic.
 *
 * Vertices along the edge loop are redistributed to uniform spacing
 * based on the cumulative length of the edge loop and are interpolated
 * either smoothly via a natural cubic spline or linearly.
 */
#include "BLI_math_vector.hh"

#include "BLI_binary_search.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_listbase.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_solvers.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

/** Used as a threshold to decide if all vertices are at the same position. */
constexpr float DUPLICATE_POSITION_THRESHOLD = 1e-6f;
/** Epsilon to prevent zero division. */
constexpr float SPACE_EPSILON = 1e-8f;

/**
 * A chain of vertices collected from a walk along connected edges.
 */
struct SpaceChainData {
  /** Ordered vertices from one end of the chain to the other. */
  Vector<BMVert *> verts;
  /** True if the path forms a closed ring. */
  bool is_closed = false;
};

/**
 * Stores measured and target distances for a vertex chain.
 */
struct SpaceMeasurements {
  /** 3D coordinates of the vertices. */
  Array<float3> positions;
  /**
   * Cumulative vertex distances along the chain.
   * For cyclic chains, this array has a length of "positions.size() + 1" to store
   * the total chain distance at the end, so values don't need to be wrapped.
   */
  Array<float> knot_distances;
};

/**
 * Coefficients for the cubic spline curve equation, calculated per coordinate axis.
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

/**
 * Build vertex chains from selected edges.
 */
static void get_space_input_chains(BMesh *bm, Vector<SpaceChainData> &r_chains)
{
  ListBaseT<BMEdgeLoopStore> eloops = {nullptr};
  const BMEdgeLoopFind_Params params = {
      .use_vert_junction = true,
  };
  BM_mesh_edgeloops_find(
      bm, &eloops, [](BMEdge *e) { return BM_elem_flag_test(e, BM_ELEM_TAG); }, &params);

  for (BMEdgeLoopStore &el_store : eloops) {
    SpaceChainData chain;
    chain.is_closed = BM_edgeloop_is_closed(&el_store);
    for (LinkData &node : *BM_edgeloop_verts_get(&el_store)) {
      chain.verts.append(static_cast<BMVert *>(node.data));
    }

    /* Skip chains where all vertices are at the same location. */
    bool all_duplicate = true;
    for (const int i : chain.verts.index_range().drop_back(1)) {
      if (math::distance_squared(float3(chain.verts[i]->co), float3(chain.verts[i + 1]->co)) >
          math::square(DUPLICATE_POSITION_THRESHOLD))
      {
        all_duplicate = false;
        break;
      }
    }

    if (!all_duplicate) {
      r_chains.append(std::move(chain));
    }
  }

  BM_mesh_edgeloops_free(&eloops);
}

/**
 * Compute cumulative distances along the chain.
 */
static SpaceMeasurements measure_chain(const SpaceChainData &chain)
{
  SpaceMeasurements measure;
  const int verts_num = chain.verts.size();

  measure.positions.reinitialize(verts_num);
  for (const int i : IndexRange(verts_num)) {
    measure.positions[i] = float3(chain.verts[i]->co);
  }
  measure.knot_distances.reinitialize(verts_num + (chain.is_closed ? 1 : 0));
  measure.knot_distances[0] = 0.0f;
  length_parameterize::accumulate_lengths<float3>(
      measure.positions, chain.is_closed, measure.knot_distances.as_mutable_span().drop_front(1));

  return measure;
}

/**
 * Compute cubic spline coefficients for one coordinate axis.
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
  const int num_segments = is_closed ? verts_num : verts_num - 1;
  Array<float> segment_length(num_segments);

  for (const int i : IndexRange(num_segments)) {
    segment_length[i] = distances[i + 1] - distances[i];
    if (!(segment_length[i] > 0.0f)) {
      segment_length[i] = SPACE_EPSILON;
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
  for (const int i : IndexRange(num_segments)) {
    const int i_next = is_closed ? math::mod_periodic(i + 1, verts_num) : i + 1;

    const float coeff_a = coords[i];
    const float coeff_b = ((coords[i_next] - coords[i]) / segment_length[i]) -
                          (segment_length[i] * (c_vals[i_next] + 2.0f * c_vals[i])) / 3.0f;
    const float coeff_c = c_vals[i];
    const float coeff_d = (c_vals[i_next] - c_vals[i]) / (3.0f * segment_length[i]);
    r_coeffs.append({coeff_a, coeff_b, coeff_c, coeff_d, distances[i]});
  }
}

/** Return the index of the spline segment that contains target_distance. */
static int calc_spline_segment(Span<float> knot_distances, const float target_distance)
{
  const int segment_index = binary_search::last_if(
      knot_distances, [&](const float value) { return value <= target_distance; });
  return std::clamp(segment_index, 0, int(knot_distances.size()) - 2);
}

/** Evaluates the cubic spline at target_distance. */
static float3 evaluate_cubic(Span<float> tknots,
                             Span<SplineCoeffs> coeffs_x,
                             Span<SplineCoeffs> coeffs_y,
                             Span<SplineCoeffs> coeffs_z,
                             const float target_distance)
{
  const int segment = calc_spline_segment(tknots, target_distance);
  const float dt = target_distance - coeffs_x[segment].x;

  const SplineCoeffs &cx = coeffs_x[segment];
  const SplineCoeffs &cy = coeffs_y[segment];
  const SplineCoeffs &cz = coeffs_z[segment];

  return float3(cx.a + dt * (cx.b + dt * (cx.c + dt * cx.d)),
                cy.a + dt * (cy.b + dt * (cy.c + dt * cy.d)),
                cz.a + dt * (cz.b + dt * (cz.c + dt * cz.d)));
}

void bmo_space_edge_loops_evenly_exec(BMesh *bm, BMOperator *op)
{
  const float factor = BMO_slot_float_get(op->slots_in, "factor");
  const SpaceInterpolationMethod interpolation = static_cast<SpaceInterpolationMethod>(
      BMO_slot_int_get(op->slots_in, "interpolation"));
  const bool lock_x = BMO_slot_bool_get(op->slots_in, "lock_x");
  const bool lock_y = BMO_slot_bool_get(op->slots_in, "lock_y");
  const bool lock_z = BMO_slot_bool_get(op->slots_in, "lock_z");

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_EDGE, BM_ELEM_TAG, false);

  Vector<SpaceChainData> chains;
  get_space_input_chains(bm, chains);

  for (SpaceChainData &chain : chains) {
    const int verts_num = chain.verts.size();
    SpaceMeasurements measure = measure_chain(chain);

    Array<int> sample_indices(verts_num);
    Array<float> sample_factors(verts_num);
    length_parameterize::sample_uniform(measure.knot_distances.as_span().drop_front(1),
                                        !chain.is_closed,
                                        sample_indices,
                                        sample_factors);

    Array<float3> new_positions(verts_num);

    if (interpolation == SPACE_EDGE_LOOPS_EVENLY_INTERP_LINEAR) {
      length_parameterize::interpolate<float3>(
          measure.positions, sample_indices, sample_factors, new_positions);
    }
    else {
      Array<float> coords_x(verts_num);
      Array<float> coords_y(verts_num);
      Array<float> coords_z(verts_num);
      for (const int i : IndexRange(verts_num)) {
        coords_x[i] = measure.positions[i].x;
        coords_y[i] = measure.positions[i].y;
        coords_z[i] = measure.positions[i].z;
      }

      Vector<SplineCoeffs> coeffs_x;
      Vector<SplineCoeffs> coeffs_y;
      Vector<SplineCoeffs> coeffs_z;
      calculate_splines_axis(measure.knot_distances, coords_x, chain.is_closed, coeffs_x);
      calculate_splines_axis(measure.knot_distances, coords_y, chain.is_closed, coeffs_y);
      calculate_splines_axis(measure.knot_distances, coords_z, chain.is_closed, coeffs_z);

      for (const int i : IndexRange(verts_num)) {
        const int seg = sample_indices[i];
        const float target_dist = math::interpolate(
            measure.knot_distances[seg], measure.knot_distances[seg + 1], sample_factors[i]);

        new_positions[i] = evaluate_cubic(
            measure.knot_distances, coeffs_x, coeffs_y, coeffs_z, target_dist);
      }
    }

    for (const int i : IndexRange(verts_num)) {
      /* The first and last vertices of an open chain are anchor points so they are skipped. */
      if (!chain.is_closed && (i == 0 || i == verts_num - 1)) {
        continue;
      }

      float3 new_pos = new_positions[i];

      if (lock_x) {
        new_pos.x = measure.positions[i].x;
      }
      if (lock_y) {
        new_pos.y = measure.positions[i].y;
      }
      if (lock_z) {
        new_pos.z = measure.positions[i].z;
      }

      float3 final_pos = math::interpolate(measure.positions[i], new_pos, factor);
      copy_v3_v3(chain.verts[i]->co, final_pos);
    }
  }
}

}  // namespace blender
