/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Stretches selected vertices to the active stroke.
 */

#include "BLI_listbase.hh"
#include "BLI_math_geom.hh"
#include "BLI_math_vector.hh"

#include "BLI_length_parameterize.hh"

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

constexpr float ray_length = 1000.0f;
constexpr float max_ray_distance = 0.01f;
constexpr float segment_epsilon = 1e-4f;
constexpr float normal_length_epsilon = 1e-3f;

struct ToAnnotationChain {
  /** Ordered vertices along the chain path. */
  Vector<BMVert *> verts;
  /** Center of the selected edge loop. */
  float3 center = float3(0.0f);
  /** Accumulated segment lengths for the edge loop. */
  Vector<float> accumulated_lengths;
};

struct StrokeData {
  /** The stroke points. */
  Span<float3> points;
  /** Center of the drawn stroke. */
  float3 center = float3(0.0f);
  /** Number of edge loops using this stroke. */
  int use_count = 0;
  /** Accumulated segment lengths for the stroke. */
  Vector<float> accumulated_lengths;
};

static void calc_annotation_input_chains_from_bm(BMesh *bm, Vector<ToAnnotationChain> &r_chains)
{
  ListBaseT<BMEdgeLoopStore> eloops = {nullptr};
  const BMEdgeLoopFind_Params params = {
      .use_vert_junction = true,
  };
  BM_mesh_edgeloops_find(
      bm, &eloops, [](BMEdge *e) { return BM_elem_flag_test(e, BM_ELEM_TAG); }, &params);

  for (BMEdgeLoopStore &el_store : eloops) {
    ToAnnotationChain chain;
    for (LinkData &node : *BM_edgeloop_verts_get(&el_store)) {
      chain.verts.append(static_cast<BMVert *>(node.data));
    }

    r_chains.append(std::move(chain));
  }

  BM_mesh_edgeloops_free(&eloops);
}

static void measure_positions(const Span<float3> positions,
                              float3 &r_center,
                              Vector<float> &r_accumulated_lengths)
{
  const int points_num = positions.size();

  r_center = float3(0.0f);
  for (const int i : IndexRange(points_num)) {
    r_center += positions[i];
  }
  r_center /= float(points_num);

  r_accumulated_lengths.resize(points_num - 1);
  length_parameterize::accumulate_lengths<float3>(
      positions, false, r_accumulated_lengths.as_mutable_span());
}

static bool is_total_length_valid(const Span<float> accumulated_lengths)
{
  return accumulated_lengths.last() > 0.0f;
}

static void calculate_chain_lengths(ToAnnotationChain &chain)
{
  const int verts_num = chain.verts.size();

  Array<float3> positions(verts_num);
  for (const int i : IndexRange(verts_num)) {
    positions[i] = float3(chain.verts[i]->co);
  }

  measure_positions(positions, chain.center, chain.accumulated_lengths);
}

static void calculate_stroke_lengths(StrokeData &stroke_data)
{
  measure_positions(stroke_data.points, stroke_data.center, stroke_data.accumulated_lengths);
}

static void sample_stroke_positions(const StrokeData &stroke_data,
                                    const ToAnnotationChain &chain,
                                    const int method,
                                    MutableSpan<float3> r_positions)
{
  const int verts_num = chain.verts.size();
  Array<int> segment_indices(verts_num);
  Array<float> factors(verts_num);

  if (method == TO_ANNOTATION_SPREAD_EVENLY) {
    length_parameterize::sample_uniform(
        stroke_data.accumulated_lengths, true, segment_indices, factors);
  }
  else {
    const float chain_length_total = chain.accumulated_lengths.last();
    const float stroke_length_total = stroke_data.accumulated_lengths.last();

    Array<float> sample_lengths(verts_num);
    for (const int i : IndexRange(verts_num)) {
      float sample_factor = (i == 0) ? 0.0f :
                                       chain.accumulated_lengths[i - 1] / chain_length_total;
      sample_lengths[i] = sample_factor * stroke_length_total;
    }

    length_parameterize::sample_at_lengths(
        stroke_data.accumulated_lengths, sample_lengths, segment_indices, factors);
  }

  length_parameterize::interpolate(
      stroke_data.points, segment_indices.as_span(), factors.as_span(), r_positions);
}

/**
 * Calculates the total distance between the vertices on the selected edge loop
 * and their equivalent annotation stroke positions.
 * It's possible for the direction in which a stroke is drawn to not match the direction
 * of the edge loop. In this case, running the operator without checking
 * for the best alignment distance would result in the vertices from the edge
 * loop being twisted onto the annotation stroke.
 */
static float calculate_alignment_distance(const ToAnnotationChain &chain,
                                          const StrokeData &stroke_data,
                                          const int method)
{
  const int verts_num = chain.verts.size();
  Array<float3> stroke_positions(verts_num);
  sample_stroke_positions(stroke_data, chain, method, stroke_positions);

  float total_distance = 0.0f;
  for (const int i : IndexRange(verts_num)) {
    total_distance += math::distance(float3(chain.verts[i]->co), stroke_positions[i]);
  }
  return total_distance;
}

static bool edge_shares_face_with_tagged_edge(BMEdge *e, BMVert *v)
{
  BMLoop *l;
  BMIter liter;

  BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
    BMLoop *l_other = BM_loop_other_edge_loop(l, v);
    BMEdge *e_other = l_other->e;
    if (BM_elem_flag_test(e_other, BM_ELEM_TAG)) {
      return true;
    }
  }
  return false;
}

static void calculate_verts(const StrokeData &stroke_data,
                            const int method,
                            const float factor,
                            const bool lock_x,
                            const bool lock_y,
                            const bool lock_z,
                            ToAnnotationChain &r_chain)
{
  const int verts_num = r_chain.verts.size();

  if (method == TO_ANNOTATION_PROJECT) {
    for (const int i : IndexRange(verts_num)) {
      BMVert *v = r_chain.verts[i];
      const float3 v_co = float3(v->co);
      float3 v_co_best = v_co;
      float dist_sq_best = FLT_MAX;
      bool has_best = false;

      auto check_intersection_fn = [&](const float3 &ray_dir) {
        float3 ray_end = v_co + ray_dir * ray_length;
        float3 ray_start = v_co - ray_dir * ray_length;

        for (const int j : IndexRange(stroke_data.points.size() - 1)) {
          float3 stroke_seg_start = stroke_data.points[j];
          float3 stroke_seg_end = stroke_data.points[j + 1];

          float3 closest_on_ray;
          float3 closest_on_stroke;
          if (math::isect_line_line(ray_start,
                                    ray_end,
                                    stroke_seg_start,
                                    stroke_seg_end,
                                    closest_on_ray,
                                    closest_on_stroke) != 0)
          {
            if (math::distance_squared(closest_on_ray, closest_on_stroke) <
                math::square(max_ray_distance))
            {
              float dist_seg = math::distance(stroke_seg_start, stroke_seg_end);
              float dist_to_start = math::distance(stroke_seg_start, closest_on_stroke);
              float dist_to_end = math::distance(stroke_seg_end, closest_on_stroke);

              if (dist_to_start <= dist_seg + segment_epsilon &&
                  dist_to_end <= dist_seg + segment_epsilon)
              {
                float dist_sq_test = math::distance_squared(v_co, closest_on_stroke);
                if (dist_sq_test < dist_sq_best) {
                  dist_sq_best = dist_sq_test;
                  v_co_best = closest_on_stroke;
                  has_best = true;
                }
              }
            }
          }
        }
      };

      /* In order to project the selected vertices unto the annotation stroke, we need
       * to shoot rays forwards and backwards from those vertices along the direction of
       * their connected unselected vertices to find where those rays interesect with the
       * stroke, with the exception of unselected vertices connected to selected vertices
       * on the same edge loop of the selection which can cause the selected vertices to
       * project oddly unto the annotation stroke if the stroke curves around the selection.
       *
       * In the case that doesn't yield any results, we fallback to shooting rays from the
       * vertex normals.
       */
      BMIter iter;
      BMEdge *e;
      BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
          if (edge_shares_face_with_tagged_edge(e, v)) {
            BMVert *v_other = BM_edge_other_vert(e, v);
            float3 ray_dir = math::normalize(float3(v_other->co) - float3(v->co));
            check_intersection_fn(ray_dir);
          }
        }
      }

      if (!has_best) {
        float3 ray_dir = float3(v->no);
        if (math::length_squared(ray_dir) > math::square(normal_length_epsilon)) {
          ray_dir = math::normalize(ray_dir);
          check_intersection_fn(ray_dir);
        }
      }

      if (has_best) {
        float3 new_co = math::interpolate(v_co, v_co_best, factor);
        if (lock_x) {
          new_co.x = v_co.x;
        }
        if (lock_y) {
          new_co.y = v_co.y;
        }
        if (lock_z) {
          new_co.z = v_co.z;
        }
        copy_v3_v3(v->co, new_co);
      }
    }
  }
  else {
    Array<float3> stroke_positions(verts_num);
    sample_stroke_positions(stroke_data, r_chain, method, stroke_positions);

    for (const int i : IndexRange(verts_num)) {
      BMVert *v = r_chain.verts[i];
      const float3 v_co = float3(v->co);
      float3 new_co = math::interpolate(v_co, stroke_positions[i], factor);
      if (lock_x) {
        new_co.x = v_co.x;
      }
      if (lock_y) {
        new_co.y = v_co.y;
      }
      if (lock_z) {
        new_co.z = v_co.z;
      }
      copy_v3_v3(v->co, new_co);
    }
  }
}

void bmo_fit_edge_loops_to_annotation_exec(BMesh *bm, BMOperator *op)
{
  const float factor = BMO_slot_float_get(op->slots_in, "factor");
  const int method = BMO_slot_int_get(op->slots_in, "method");
  const bool lock_x = BMO_slot_bool_get(op->slots_in, "lock_x");
  const bool lock_y = BMO_slot_bool_get(op->slots_in, "lock_y");
  const bool lock_z = BMO_slot_bool_get(op->slots_in, "lock_z");
  Vector<Span<float3>> *stroke_data_in = static_cast<Vector<Span<float3>> *>(
      BMO_slot_ptr_get(op->slots_in, "strokes"));

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_EDGE, BM_ELEM_TAG, false);

  Vector<ToAnnotationChain> chains;
  calc_annotation_input_chains_from_bm(bm, chains);
  Vector<StrokeData> strokes;

  if (stroke_data_in) {
    for (const Span<float3> &points : *stroke_data_in) {
      StrokeData stroke_data;
      stroke_data.points = points;
      calculate_stroke_lengths(stroke_data);
      strokes.append(stroke_data);
    }
  }

  for (ToAnnotationChain &chain : chains) {
    calculate_chain_lengths(chain);
    if (!is_total_length_valid(chain.accumulated_lengths)) {
      continue;
    }

    StrokeData *stroke_best = nullptr;
    float3 points_fallback[2];
    StrokeData stroke_fallback;

    /* If there are no annotation strokes drawn, the vertices are moved
     * onto a line between the first and last vertex. */
    if (strokes.is_empty()) {
      float3 chain_start_co = float3(chain.verts.first()->co);
      float3 chain_end_co = float3(chain.verts.last()->co);

      points_fallback[0] = chain_start_co;
      points_fallback[1] = chain_end_co;
      stroke_fallback.points = Span<float3>(points_fallback, 2);
      stroke_fallback.accumulated_lengths = {math::distance(chain_start_co, chain_end_co)};

      stroke_best = &stroke_fallback;
    }
    else {
      int index_best = -1;
      int use_count_best = INT_MAX;
      float dist_sq_best = FLT_MAX;

      for (const int i : strokes.index_range()) {
        const int use_count = strokes[i].use_count;
        const float dist_sq_test = math::distance_squared(chain.center, strokes[i].center);

        if (use_count < use_count_best ||
            (use_count == use_count_best && dist_sq_test < dist_sq_best))
        {
          use_count_best = use_count;
          dist_sq_best = dist_sq_test;
          index_best = i;
        }
      }

      strokes[index_best].use_count++;
      stroke_best = &strokes[index_best];
    }

    float forward_dist = calculate_alignment_distance(chain, *stroke_best, method);

    ToAnnotationChain reversed_chain = chain;
    std::reverse(reversed_chain.verts.begin(), reversed_chain.verts.end());
    reversed_chain.center = float3(0.0f);
    reversed_chain.accumulated_lengths.clear();
    calculate_chain_lengths(reversed_chain);

    float reversed_dist = calculate_alignment_distance(reversed_chain, *stroke_best, method);
    if (reversed_dist < forward_dist) {
      chain = std::move(reversed_chain);
    }
    calculate_verts(*stroke_best, method, factor, lock_x, lock_y, lock_z, chain);
  }
}

}  // namespace blender
