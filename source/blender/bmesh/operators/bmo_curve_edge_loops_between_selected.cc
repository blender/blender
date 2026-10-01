/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Fits the unselected edge loops to a curve defined by the selected vertices.
 */
#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"

#include "BLI_length_parameterize.hh"
#include "BLI_math_solvers.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

constexpr float CURVE_EPSILON = 1e-8f;

/**
 * A chain of vertices collected from a walk along connected edges.
 */
struct CurveChainData {
  /** Ordered vertices along the chain path. */
  Vector<BMVert *> verts;
  /** True if the path forms a closed chain. */
  bool is_closed = false;
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

/**
 * Compute cubic spline coefficients for one coordinate axis.
 * Uses `BLI_tridiagonal_solve` for open chains and
 * `BLI_tridiagonal_solve_cyclic` for closed loops.
 */
static void calculate_splines_axis(const Span<float> distances,
                                   const Span<float> coords,
                                   const bool is_closed,
                                   Array<SplineCoeffs> &r_coeffs)
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
      segment_length[i] = CURVE_EPSILON;
    }
  }

  /* Stores second derivative coefficients. For a natural cubic spline, the boundary
   * condition defines the first and last points as zero. */
  Array<float> c_vals(verts_num, 0.0f);

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
    const int interior = verts_num - 2;
    if (interior > 0) {
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
  }

  r_coeffs = Array<SplineCoeffs>(segments_num);
  /* Build polynomial coefficients for each segment. */
  for (const int i : IndexRange(segments_num)) {
    const int i_next = is_closed ? math::mod_periodic(i + 1, verts_num) : i + 1;
    const float coeff_a = coords[i];
    const float coeff_b = ((coords[i_next] - coords[i]) / segment_length[i]) -
                          (segment_length[i] * (c_vals[i_next] + 2.0f * c_vals[i])) / 3.0f;
    const float coeff_c = c_vals[i];
    const float coeff_d = (c_vals[i_next] - c_vals[i]) / (3.0f * segment_length[i]);
    r_coeffs[i] = {coeff_a, coeff_b, coeff_c, coeff_d, distances[i]};
  }
}

/**
 * Starts at a specific edge and walks continuously in one direction along quad or wire edges
 * until it loops back into itself in a circle or hits the end of the edge loop.
 */
static void traverse_edge_loop(BMVert *v_start,
                               BMEdge *e_start,
                               Set<BMEdge *> &visited_edges,
                               Vector<BMVert *> &r_loop,
                               bool &r_circular)
{
  BMVert *v_curr = v_start;
  BMEdge *e_curr = e_start;

  visited_edges.add(e_curr);

  while (true) {
    BMVert *v_next = BM_edge_other_vert(e_curr, v_curr);
    r_loop.append(v_next);

    if (v_next == v_start) {
      r_circular = true;
      r_loop.pop_last();
      break;
    }

    /* Never step between wire & face edges. */
    const bool is_wire = BM_edge_is_wire(e_curr);
    int edge_count = 0;
    BMIter iter;
    BMEdge *e;
    BMEdge *e_next = nullptr;

    BM_ITER_ELEM (e, &iter, v_next, BM_EDGES_OF_VERT) {
      if (BM_edge_is_wire(e) != is_wire) {
        continue;
      }
      if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        edge_count++;
      }
    }

    if (is_wire) {
      /* Stop at the end of a wire chain or where it branches. */
      if (edge_count != 2) {
        break;
      }
    }
    else {
      /* Stop at poles & corners, where the loop has no single continuation. */
      if (edge_count < 3 || edge_count > 4) {
        break;
      }
    }
    BM_ITER_ELEM (e, &iter, v_next, BM_EDGES_OF_VERT) {
      if (BM_edge_is_wire(e) != is_wire) {
        continue;
      }
      if (e == e_curr || BM_elem_flag_test(e, BM_ELEM_HIDDEN) || visited_edges.contains(e)) {
        continue;
      }

      if (!BM_edge_share_quad_check(e_curr, e)) {
        e_next = e;
        break;
      }
    }

    if (!e_next) {
      break;
    }
    e_curr = e_next;
    v_curr = v_next;
    visited_edges.add(e_curr);
  }
}

/**
 * Runs `traverse_edge_loop` in both forward and backward direction from a specific
 * edge and combines them into CurveChainData.
 */
static CurveChainData build_curve_chain_from_edge(BMEdge *e_start, Set<BMEdge *> &visited_edges)
{
  CurveChainData chain;
  chain.is_closed = false;

  Vector<BMVert *> forward_trace;
  bool is_forward_circular = false;
  traverse_edge_loop(e_start->v1, e_start, visited_edges, forward_trace, is_forward_circular);

  if (is_forward_circular) {
    chain.is_closed = true;
    chain.verts.append(e_start->v1);
    chain.verts.extend(forward_trace);
  }
  else {
    Vector<BMVert *> backward_trace;
    bool is_backward_circular = false;
    traverse_edge_loop(e_start->v2, e_start, visited_edges, backward_trace, is_backward_circular);

    std::ranges::reverse(backward_trace);
    chain.verts.extend(backward_trace);
    chain.verts.extend(forward_trace);
  }

  return chain;
}

static void get_perpendicular_chains(const CurveChainData &chain,
                                     Vector<CurveChainData> &r_chains,
                                     Set<BMEdge *> &visited_edges)
{
  for (BMVert *vert : chain.verts) {
    BMIter iter;
    BMEdge *e;
    BM_ITER_ELEM (e, &iter, vert, BM_EDGES_OF_VERT) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) || visited_edges.contains(e)) {
        continue;
      }
      CurveChainData perp_chain = build_curve_chain_from_edge(e, visited_edges);
      r_chains.append(std::move(perp_chain));
    }
  }
}

static void calc_curve_input_chains_from_bm(BMesh *bm, Vector<CurveChainData> &r_chains)
{
  Vector<BMVert *> tag_verts;
  BMIter iter;
  BMVert *v;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      tag_verts.append(v);
    }
  }

  Set<BMEdge *> visited_edges;

  for (BMVert *v_start : tag_verts) {
    BMIter v_edge_iter;
    BMEdge *e_start;
    BM_ITER_ELEM (e_start, &v_edge_iter, v_start, BM_EDGES_OF_VERT) {
      if (BM_elem_flag_test(e_start, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (visited_edges.contains(e_start)) {
        continue;
      }

      CurveChainData chain = build_curve_chain_from_edge(e_start, visited_edges);

      if (chain.verts.size() < 3) {
        continue;
      }

      int tag_count = 0;
      for (BMVert *v_chain : chain.verts) {
        if (BM_elem_flag_test(v_chain, BM_ELEM_TAG)) {
          tag_count++;
        }
      }

      /* If a whole edge loop is tagged, all edge loops perpendicular to it are operated on,
       * with the tagged edge loop being used as control points.  */
      if (tag_count == chain.verts.size()) {
        get_perpendicular_chains(chain, r_chains, visited_edges);
      }
      else if (tag_count >= 1) {
        r_chains.append(std::move(chain));
      }
    }
  }
}

/**
 * Measures the gap size between the current vertex and the next vertex
 * to find the longest gap size and the starting index of that gap.
 */
static void find_longest_unselected_gap(const Span<BMVert *> verts,
                                        int &r_longest_gap,
                                        int &r_best_gap_start)
{
  const int verts_num = verts.size();
  Vector<int> selected_indices;

  for (const int i : verts.index_range()) {
    if (BM_elem_flag_test(verts[i], BM_ELEM_TAG)) {
      selected_indices.append(i);
    }
  }

  if (selected_indices.is_empty()) {
    r_longest_gap = verts_num;
    r_best_gap_start = 0;
    return;
  }

  int longest_gap = -1;
  int best_gap_start = -1;

  for (const int i : selected_indices.index_range()) {
    int i_curr = selected_indices[i];
    int i_next = selected_indices[math::mod_periodic(i + 1, int(selected_indices.size()))];
    int gap_size = math::mod_periodic(i_next - i_curr - 1, verts_num);

    if (gap_size > longest_gap) {
      longest_gap = gap_size;
      best_gap_start = math::mod_periodic(i_curr + 1, verts_num);
    }
  }

  r_longest_gap = longest_gap;
  r_best_gap_start = best_gap_start;
}

static void trim_chains_to_boundaries(Vector<CurveChainData> &chains)
{
  for (CurveChainData &chain : chains) {
    if (chain.is_closed) {
      int longest_gap, best_gap_start;
      find_longest_unselected_gap(chain.verts, longest_gap, best_gap_start);

      if (longest_gap > 0) {
        int new_start_index = math::mod_periodic(best_gap_start + longest_gap,
                                                 int(chain.verts.size()));
        /* Pushes the unselected gap to the end of the vector so it's easy to trim it off. */
        std::rotate(chain.verts.begin(), chain.verts.begin() + new_start_index, chain.verts.end());
      }
      chain.is_closed = false;
    }

    int first_selected = -1;
    int last_selected = -1;
    for (const int i : chain.verts.index_range()) {
      if (BM_elem_flag_test(chain.verts[i], BM_ELEM_TAG)) {
        if (first_selected == -1) {
          first_selected = i;
        }
        last_selected = i;
      }
    }

    if (first_selected > 0 || last_selected < chain.verts.size() - 1) {
      Vector<BMVert *> trimmed;
      for (int i = first_selected; i <= last_selected; i++) {
        trimmed.append(chain.verts[i]);
      }
      chain.verts = std::move(trimmed);
    }
    BLI_assert(!chain.verts.is_empty());
  }
}

static void calculate_curve_distances(const Span<BMVert *> verts,
                                      const Span<int> knot_indices,
                                      const Span<int> point_indices,
                                      const bool is_closed,
                                      const bool regular,
                                      Array<float> &r_knot_distances,
                                      Array<float> &r_point_distances)
{
  const int verts_num = verts.size();

  Array<float3> positions(verts_num);
  for (const int i : IndexRange(verts_num)) {
    positions[i] = float3(verts[i]->co);
  }
  const int cumulative_array_size = verts_num + (is_closed ? 1 : 0);
  Array<float> cumulative(cumulative_array_size);
  cumulative[0] = 0.0f;
  length_parameterize::accumulate_lengths<float3>(
      positions, is_closed, cumulative.as_mutable_span().drop_front(1));

  r_knot_distances = Array<float>(knot_indices.size() + (is_closed ? 1 : 0));
  r_point_distances = Array<float>(point_indices.size());

  int knot_index = 0;
  int point_index = 0;
  for (const int i : IndexRange(verts_num)) {
    if (knot_index < knot_indices.size() && knot_indices[knot_index] == i) {
      r_knot_distances[knot_index] = cumulative[i];
      knot_index++;
    }
    else {
      r_point_distances[point_index] = cumulative[i];
      point_index++;
    }
  }

  if (is_closed) {
    const float total_length = cumulative.last();
    r_knot_distances[knot_index++] = total_length;
  }

  if (regular) {
    const float total = cumulative.last();
    if (total > CURVE_EPSILON) {
      const float spacing = total / float(is_closed ? verts_num : verts_num - 1);
      for (const int i : point_indices.index_range()) {
        const int v_index = point_indices[i];
        r_point_distances[i] = float(v_index) * spacing;
      }
      for (const int ki : knot_indices.index_range()) {
        r_knot_distances[ki] = float(knot_indices[ki]) * spacing;
      }
      if (is_closed) {
        r_knot_distances[knot_indices.size()] = total;
      }
    }
  }
}

static void calculate_curve_splines(const Span<float3> knot_positions,
                                    const Span<float> knot_distances,
                                    const bool is_closed,
                                    std::array<Array<SplineCoeffs>, 3> &r_coeffs)
{
  const int knots_num = knot_positions.size();
  Array<float> coords_x(knots_num);
  Array<float> coords_y(knots_num);
  Array<float> coords_z(knots_num);

  for (const int i : IndexRange(knots_num)) {
    coords_x[i] = knot_positions[i].x;
    coords_y[i] = knot_positions[i].y;
    coords_z[i] = knot_positions[i].z;
  }

  calculate_splines_axis(knot_distances, coords_x, is_closed, r_coeffs[0]);
  calculate_splines_axis(knot_distances, coords_y, is_closed, r_coeffs[1]);
  calculate_splines_axis(knot_distances, coords_z, is_closed, r_coeffs[2]);
}

static void execute_curve_chain(const Span<BMVert *> input_chain,
                                bool is_closed,
                                const CurveInterpolation interpolation,
                                const CurveClampElevation clamp_elevation,
                                const bool regular,
                                const float factor,
                                const bool lock_x,
                                const bool lock_y,
                                const bool lock_z)
{
  int verts_num = input_chain.size();
  int new_start_index = 0;

  if (is_closed) {
    int longest_gap, best_gap_start;
    find_longest_unselected_gap(input_chain, longest_gap, best_gap_start);

    int offset = verts_num / 4;

    /* For a closed circle, if more than 50 percent of the vertices in the circle are unselected,
     * in order to prevent a total collapse from happening, the tool's influence is restricted to
     * 25 percent of the total vertices on either side of the selection. */
    if (longest_gap > 2 * offset) {
      is_closed = false;

      int trim_start = math::mod_periodic(best_gap_start + longest_gap - offset, verts_num);
      int trim_end = math::mod_periodic(best_gap_start - 1 + offset, verts_num);

      new_start_index = trim_start;
      verts_num = math::mod_periodic(trim_end - trim_start, verts_num) + 1;
    }
    else {
      for (const int i : input_chain.index_range()) {
        if (BM_elem_flag_test(input_chain[i], BM_ELEM_TAG)) {
          new_start_index = i;
          break;
        }
      }
    }
  }

  Array<BMVert *> verts(verts_num);
  for (const int i : IndexRange(verts_num)) {
    verts[i] = input_chain[math::mod_periodic(i + new_start_index, int(input_chain.size()))];
  }

  Vector<int> knot_indices;
  for (const int i : IndexRange(verts_num)) {
    if (BM_elem_flag_test(verts[i], BM_ELEM_TAG)) {
      knot_indices.append(i);
    }
  }

  if (!is_closed) {
    if (knot_indices.is_empty() || knot_indices.first() != 0) {
      knot_indices.insert(0, 0);
    }
    if (knot_indices.last() != verts_num - 1) {
      knot_indices.append(verts_num - 1);
    }
  }

  Array<float3> knot_positions(knot_indices.size());
  for (const int ki : knot_indices.index_range()) {
    knot_positions[ki] = float3(verts[knot_indices[ki]]->co);
  }

  Array<float> knot_distances;
  Array<float> point_distances;
  Array<int> point_indices(verts_num - knot_indices.size());

  std::set_difference(IndexRange(verts_num).begin(),
                      IndexRange(verts_num).end(),
                      knot_indices.data(),
                      knot_indices.data() + knot_indices.size(),
                      point_indices.data());

  calculate_curve_distances(
      verts, knot_indices, point_indices, is_closed, regular, knot_distances, point_distances);

  if (point_indices.is_empty()) {
    return;
  }

  const int points_num = point_indices.size();
  Array<float3> new_positions(points_num);
  Array<int> segment_indices(points_num);
  Array<float> factors(points_num);

  length_parameterize::sample_at_lengths(
      knot_distances.as_span().drop_front(1), point_distances, segment_indices, factors);

  if (interpolation == CURVE_INTERP_CUBIC) {
    std::array<Array<SplineCoeffs>, 3> axis_coeffs;
    calculate_curve_splines(knot_positions, knot_distances, is_closed, axis_coeffs);

    for (const int i : IndexRange(points_num)) {
      const int seg = segment_indices[i];
      const float dt = point_distances[i] - axis_coeffs[0][seg].x;

      const SplineCoeffs &cx = axis_coeffs[0][seg];
      const SplineCoeffs &cy = axis_coeffs[1][seg];
      const SplineCoeffs &cz = axis_coeffs[2][seg];

      new_positions[i] = float3(cx.a + dt * (cx.b + dt * (cx.c + dt * cx.d)),
                                cy.a + dt * (cy.b + dt * (cy.c + dt * cy.d)),
                                cz.a + dt * (cz.b + dt * (cz.c + dt * cz.d)));
    }
  }
  else if (interpolation == CURVE_INTERP_LINEAR) {
    length_parameterize::interpolate<float3>(
        knot_positions, segment_indices, factors, new_positions);
  }

  for (const int i : point_indices.index_range()) {
    const int v_index = point_indices[i];
    BMVert *vert = verts[v_index];
    float3 new_pos = new_positions[i];

    const float3 old_pos(vert->co);
    const float3 delta = new_pos - old_pos;

    /* Wire vertices have no face normal to clamp against. */
    if ((clamp_elevation != CURVE_CLAMP_ELEVATION_NONE) && !BM_vert_is_wire(vert)) {
      if (math::length_squared(delta) > math::square(CURVE_EPSILON)) {
        const float3 normal(vert->no);
        const float dot_val = math::dot(delta, normal);

        if (clamp_elevation == CURVE_CLAMP_ELEVATION_RAISE) {
          if (dot_val < -CURVE_EPSILON) {
            continue;
          }
        }
        else if (clamp_elevation == CURVE_CLAMP_ELEVATION_LOWER) {
          if (dot_val > CURVE_EPSILON) {
            continue;
          }
        }
      }
    }

    if (lock_x) {
      new_pos.x = old_pos.x;
    }
    if (lock_y) {
      new_pos.y = old_pos.y;
    }
    if (lock_z) {
      new_pos.z = old_pos.z;
    }

    const float3 final_pos = math::interpolate(old_pos, new_pos, factor);
    copy_v3_v3(vert->co, final_pos);
  }
}

void bmo_curve_edge_loops_between_selected_exec(BMesh *bm, BMOperator *op)
{
  const float factor = BMO_slot_float_get(op->slots_in, "factor");
  const CurveClampElevation clamp_elevation = static_cast<CurveClampElevation>(
      BMO_slot_int_get(op->slots_in, "clamp_elevation"));
  const bool extend_loop = BMO_slot_bool_get(op->slots_in, "extend_loop");
  const bool regular = BMO_slot_bool_get(op->slots_in, "regular");
  const bool lock_x = BMO_slot_bool_get(op->slots_in, "lock_x");
  const bool lock_y = BMO_slot_bool_get(op->slots_in, "lock_y");
  const bool lock_z = BMO_slot_bool_get(op->slots_in, "lock_z");
  const CurveInterpolation interpolation = static_cast<CurveInterpolation>(
      BMO_slot_int_get(op->slots_in, "interpolation"));

  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_VERT, BM_ELEM_TAG, false);

  Vector<CurveChainData> chains;
  calc_curve_input_chains_from_bm(bm, chains);

  if (!extend_loop) {
    trim_chains_to_boundaries(chains);
  }

  for (const CurveChainData &chain : chains) {
    execute_curve_chain(chain.verts,
                        chain.is_closed,
                        interpolation,
                        clamp_elevation,
                        regular,
                        factor,
                        lock_x,
                        lock_y,
                        lock_z);
  }
}

}  // namespace blender
