/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Circularize selected boundary chains.
 */
#include "BLI_kdopbvh.hh"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include <numbers>
#include <optional>

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

/** Maximum iterations for the non linear least squares solver. */
constexpr int NON_LINEAR_LEAST_SQUARES_MAX_ITERATIONS = 500;
/** Threshold for considering a vertex to be on the mirror plane. */
constexpr float MIRROR_LIMIT = 0.001f;
/** Used for convergence checks and precision comparisons. */
constexpr float CIRCULARIZE_EPSILON = 1e-6f;

/** Method used for fitting the circle. */
enum FitMethod { FIT_METHOD_LEAST_SQUARE = 0, FIT_METHOD_CONTRACT = 1 };

/** Holds data for a vertex projected onto the local plane. */
struct CircleVert {
  BMVert *v;
  /** Current position on the plane. */
  float2 co_2d;
  /** Where it should move to on the circle. */
  float2 target_2d;
};

/** Stores the boundary geometry that defines the circle. */
struct VertChain {
  /** The ordered vertices that defines the circle's boundary. */
  Vector<BMVert *> verts;
  /** This is true if the path forms a closed chain, for open chains it's false. */
  bool is_closed;
};

/**
 * Detects whether an edge should be considered a valid boundary
 * edge for circularization.
 * Valid boundary edges are edges that are selected, not hidden
 * and are not interior. They lie on the boundary between a selected
 * face and an unselected face and do not lie on the mirror plane.
 */
static bool is_valid_boundary_edge(BMEdge *e, const char hflag, const bool check_axis[3])
{
  if (!BM_elem_flag_test(e, hflag) || BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
    return false;
  }

  /* Wire edges are not valid boundary edges. */
  if (!e->l) {
    return false;
  }

  int selected_face_count = 0;
  BMIter fiter;
  BMFace *f;
  BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN) && BM_elem_flag_test(f, hflag)) {
      selected_face_count++;
      if (selected_face_count > 1) {
        break;
      }
    }
  }

  if (selected_face_count > 1) {
    return false;
  }

  /* If both vertices of an edge lie close to the same coordinate plane
   * (X = 0, Y = 0, or Z = 0), the edge lies on a mirror plane and is not
   * considered a valid boundary edge. */
  for (int i = 0; i < 3; i++) {
    if (check_axis[i] && std::abs(e->v1->co[i]) < MIRROR_LIMIT &&
        std::abs(e->v2->co[i]) < MIRROR_LIMIT)
    {
      return false;
    }
  }

  return true;
}

/**
 * Traverses a connected path of boundary edges to form a continuous sequence of vertices.
 * This function handles two cases:
 * Closed chains: walks until the traversal returns to the start vertex.
 * Open chains: walks in one direction until a dead end, then walks in the
 * opposite direction from the start edge and merges the results.
 */
static std::optional<VertChain> walk_boundary_chain(BMEdge *start_edge,
                                                    Set<BMEdge *> &visited,
                                                    const char hflag,
                                                    const bool check_axis[3])
{
  VertChain chain_data;
  /* Finds the next valid boundary edge that isn't visited. */
  auto get_next_edge_fn = [&](BMVert *v, BMEdge *exclude_e) -> BMEdge * {
    BMIter eiter;
    BMEdge *e_next;
    BM_ITER_ELEM (e_next, &eiter, v, BM_EDGES_OF_VERT) {
      if (e_next != exclude_e && !visited.contains(e_next)) {
        if (is_valid_boundary_edge(e_next, hflag, check_axis)) {
          return e_next;
        }
      }
    }
    return nullptr;
  };

  /* Walks in one direction until a dead end. */
  auto walk_fn = [&](BMVert *curr_v, BMEdge *curr_e, Vector<BMVert *> &list) {
    while (true) {
      BMEdge *next_e = get_next_edge_fn(curr_v, curr_e);
      if (!next_e) {
        break;
      }

      /* Move to next vertex. */
      curr_v = BM_edge_other_vert(next_e, curr_v);
      curr_e = next_e;

      list.append(curr_v);
      visited.add(curr_e);
    }
  };

  chain_data.verts.append(start_edge->v1);
  chain_data.verts.append(start_edge->v2);
  visited.add(start_edge);

  /* The initial edge direction (v1 -> v2) is arbitrary.
   * We walk from v2 to extend this sequence. */
  walk_fn(start_edge->v2, start_edge, chain_data.verts);

  /* If the traversal forms a closed chain, the last vertex will match the first.
   * Remove the duplicate end vertex. */
  if (chain_data.verts.size() > 2 && chain_data.verts.first() == chain_data.verts.last()) {
    if (chain_data.verts.size() < 4) {
      return std::nullopt;
    }
    chain_data.verts.remove_last();
    chain_data.is_closed = true;
    return chain_data;
  }

  /* If we are here, the chain is open.
   * We need to check the other direction from the start vertex. */
  Vector<BMVert *> pre_chain;
  walk_fn(start_edge->v1, start_edge, pre_chain);

  if (!pre_chain.is_empty()) {
    std::reverse(pre_chain.begin(), pre_chain.end());

    pre_chain.extend(chain_data.verts);
    chain_data.verts = std::move(pre_chain);
  }

  chain_data.is_closed = false;

  if (chain_data.verts.size() < 3) {
    return std::nullopt;
  }

  return chain_data;
}

/** Collects all valid boundary edge chains from the current selection. */
static void bm_vert_chain_extract_from_boundary_edges(BMesh *bm,
                                                      Vector<VertChain> &r_chains,
                                                      const char hflag,
                                                      const bool check_axis[3])
{
  Set<BMEdge *> visited;
  BMIter iter;
  BMEdge *edge;

  BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
    if (visited.contains(edge)) {
      continue;
    }
    if (!is_valid_boundary_edge(edge, hflag, check_axis)) {
      continue;
    }

    std::optional<VertChain> ld = walk_boundary_chain(edge, visited, hflag, check_axis);
    if (ld.has_value()) {
      r_chains.append(*ld);
    }
  }
}

/** Computes the local coordinate system defining the 2D plane of the vertex chain. */
static float3x3 bm_vert_chain_orientation_matrix_calc(Span<BMVert *> chain, float3 &r_center)
{
  r_center = float3(0.0f);
  for (BMVert *v : chain) {
    r_center += float3(v->co);
  }
  r_center /= float(chain.size());
  BMVert *prev = chain.last();

  float3 normal = float3(0.0f);
  /* Compute a best fit plane normal for the chain using Newell's method. */
  for (BMVert *curr : chain) {
    add_newell_cross_v3_v3v3(normal, prev->co, curr->co);
    prev = curr;
  }
  normal = math::normalize(normal);
  float3 guess = float3(1.0f, 0.0f, 0.0f);

  /* If normal is parallel to (1,0,0),the cross product would be zero.
   * In that case, we switch the guess to the y axis to allow a valid
   * perpendicular vector to be found. */
  if (std::abs(math::dot(normal, guess)) > 0.99f) {
    guess = float3(0.0f, 1.0f, 0.0f);
  }

  float3 p = math::normalize(math::cross(normal, guess));
  float3 q = math::cross(normal, p);
  float3x3 mat;
  mat.x_axis() = p;
  mat.y_axis() = q;
  mat.z_axis() = normal;
  return mat;
}

/** Projects 3D vertex coordinates onto a local 2D plane defined by the P and Q basis vectors. */
static void project_chain_to_2d(Span<BMVert *> chain,
                                const float3 &center,
                                const float3x3 &mat,
                                Vector<CircleVert> &r_2d_verts)
{
  r_2d_verts.reserve(chain.size());
  for (BMVert *v : chain) {
    float3 vec = float3(v->co) - center;
    CircleVert cv{.v = v, .co_2d = {math::dot(vec, mat.x_axis()), math::dot(vec, mat.y_axis())}};
    r_2d_verts.append(cv);
  }
}

static void calculate_circle_best_fit(Span<CircleVert> verts,
                                      const std::optional<float2> &fixed_center,
                                      float2 &r_center,
                                      float *r_radius)
{
  /* If the center is locked, we skip the solver. The best fit for the fixed center
   * is simply the average radius. */
  if (fixed_center.has_value()) {
    r_center = *fixed_center;
    float radius = 0.0f;
    for (const CircleVert &cv : verts) {
      radius += math::length(cv.co_2d);
    }
    radius /= verts.size();
    *r_radius = radius;
    return;
  }

  /* Initial guesses. */
  float2 initial_center = float2(0.0f);
  float initial_radius = 1.0f;

  for (int iter = 0; iter < NON_LINEAR_LEAST_SQUARES_MAX_ITERATIONS; iter++) {
    float3x3 normal_matrix = float3x3::zero();
    float3 jacobian_transpose_residual = float3(0.0f);

    for (const CircleVert &cv : verts) {
      const float2 d_vec = initial_center - cv.co_2d;
      const float distance = math::length(d_vec);
      if (distance < CIRCULARIZE_EPSILON) {
        continue;
      }
      const float3 j_row = {d_vec / distance, -1.0f};
      const float residual = initial_radius - distance;

      for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
          normal_matrix[col][row] += j_row[row] * j_row[col];
        }
        jacobian_transpose_residual[row] += j_row[row] * residual;
      }
    }
    bool success;
    float3x3 inverse_normal_matrix = math::invert(normal_matrix, success);
    if (!success) {
      break;
    }

    float3 delta = inverse_normal_matrix * jacobian_transpose_residual;

    initial_center.x += delta.x;
    initial_center.y += delta.y;
    initial_radius += delta.z;

    /* Check for convergence to stop iterating if we're close enough to the optimal
     * solution. */
    if (std::abs(delta.x) < CIRCULARIZE_EPSILON && std::abs(delta.y) < CIRCULARIZE_EPSILON &&
        std::abs(delta.z) < CIRCULARIZE_EPSILON)
    {
      break;
    }
  }

  r_center = initial_center;
  *r_radius = initial_radius;
}

static void calculate_circle_inside_fit(Span<CircleVert> verts,
                                        const std::optional<float2> &fixed_center,
                                        float2 &r_center,
                                        float *r_radius)
{
  float2 center;
  if (fixed_center.has_value()) {
    center = *fixed_center;
  }
  else {
    float total_edge_length = 0.0f;
    center = float2(0.0f);
    float2 prev_co = verts.last().co_2d;

    for (const CircleVert &cv : verts) {
      const float2 &curr_co = cv.co_2d;
      const float edge_length = math::distance(prev_co, curr_co);
      center += (prev_co + curr_co) * edge_length;
      total_edge_length += edge_length;
      prev_co = curr_co;
    }
    if (total_edge_length != 0.0f) {
      center *= (0.5f / total_edge_length);
    }
  }

  float radius_sq = FLT_MAX;
  for (const CircleVert &cv : verts) {
    const float dist_sq = math::distance_squared(center, cv.co_2d);
    radius_sq = std::min(radius_sq, dist_sq);
  }

  r_center = center;
  *r_radius = math::sqrt(radius_sq);
}

static void calculate_target_locations(MutableSpan<CircleVert> verts,
                                       const float2 &center,
                                       const float radius,
                                       const bool is_regular,
                                       const bool is_closed,
                                       const float rotation_angle)
{
  float step = 0.0f;
  float start_angle = 0.0f;

  if (is_regular) {
    float total_angle = 2.0f * std::numbers::pi_v<float>;
    int divisions = verts.size();

    /* For open chains, we calculate the total angle obtained by traversing
     * the vertices. Unlike closed chains whose total angle is 2*Pi,
     * we cannot assume Pi for an open chain because it might span any amount
     * of the circle. */
    if (!is_closed && divisions > 1) {
      total_angle = 0.0f;
      divisions = verts.size() - 1;

      float2 vec_prev = verts[0].co_2d - center;
      vec_prev = math::normalize(vec_prev);
      /* Skip the first vertex because it was used to initialize vec_prev otherwise
       * we'll end up with a self comparison in the first iteration. */
      for (const int i : verts.index_range().drop_front(1)) {
        float2 vec_curr = verts[i].co_2d - center;
        vec_curr = math::normalize(vec_curr);

        total_angle -= angle_signed_v2v2(vec_prev, vec_curr);
        vec_prev = vec_curr;
      }
      /* In case the angle exceeds a full revolution, clamp it to the max angle. */
      const float max_angle = 2.0f * std::numbers::pi_v<float>;
      total_angle = std::clamp(total_angle, -max_angle, max_angle);
    }

    step = total_angle / divisions;
    float sum_sin = 0.0f;
    float sum_cos = 0.0f;

    /* Using only one vertex as the basis for the start angle can skew
     * the resulting rotation of the circle in an undesirable way.
     * So instead, we calculate the circular mean of the rotation by measuring
     * the angular deviation for every vertex and averaging them to find the best
     * fit alignment.
     * Note: We accumulate the sine and cosine of the angular deviations to calculate
     * the circular mean because angles wrap around 360 degrees, and averaging them directly
     * would give incorrect results. */
    for (const int i : verts.index_range()) {
      float2 vec = verts[i].co_2d - center;
      const float angle_diff = atan2f(vec.y, vec.x) - (step * i);
      sum_sin += sinf(angle_diff);
      sum_cos += cosf(angle_diff);
    }
    start_angle = atan2f(sum_sin, sum_cos);
  }

  for (const int i : verts.index_range()) {
    float angle;

    if (is_regular) {
      angle = start_angle + step * i - rotation_angle;
    }
    else {
      float2 vec = verts[i].co_2d - center;
      angle = atan2f(vec.y, vec.x) - rotation_angle;
    }

    verts[i].target_2d.x = center.x + cosf(angle) * radius;
    verts[i].target_2d.y = center.y + sinf(angle) * radius;
  }
}

struct NearestTriUserData {
  Span<std::array<BMLoop *, 3>> looptris;
};

/** Callback for BLI_bvhtree_find_nearest. Finds the closest point on the given triangle. */
static void nearest_tri_cb(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
  const NearestTriUserData *data = static_cast<const NearestTriUserData *>(userdata);
  const std::array<BMLoop *, 3> &ltri = data->looptris[index];

  float3 closest;
  closest_on_tri_to_point_v3(closest, co, ltri[0]->v->co, ltri[1]->v->co, ltri[2]->v->co);
  const float dist_sq = math::distance_squared(float3(co), closest);
  if (dist_sq < nearest->dist_sq) {
    nearest->dist_sq = dist_sq;
    nearest->index = index;
    copy_v3_v3(nearest->co, closest);
  }
}

using FaceTessellationCache = Map<BMFace *, Array<std::array<BMVert *, 3>>>;

static void project_on_mesh(BVHTree *bvh_tree,
                            NearestTriUserData *bvh_data,
                            BMVert *v,
                            const float3 &center_pos,
                            const float3 &normal,
                            float3 &r_pos,
                            FaceTessellationCache &tess_cache)
{
  float3 vec = center_pos - float3(v->co);
  float length;
  vec = math::normalize_and_get_length(vec, length);
  /* If vertices are too close, normalization can fail. */
  if (length == 0.0f) {
    r_pos = center_pos;
    return;
  }
  const float angle = angle_normalized_v3v3(vec, normal);
  if (std::abs(angle) < CIRCULARIZE_EPSILON ||
      std::abs(std::numbers::pi_v<float> - angle) < CIRCULARIZE_EPSILON)
  {
    r_pos = float3(v->co);
    return;
  }

  float3 p2 = center_pos + normal;
  float best_dist_sq = FLT_MAX;
  bool found = false;

  auto test_tri_fn = [&](BMVert *v1, BMVert *v2, BMVert *v3) {
    float lambda;
    float2 uv;
    if (isect_line_tri_v3(center_pos, p2, v1->co, v2->co, v3->co, &lambda, uv)) {
      float3 hit_pos = center_pos + normal * lambda;
      const float dist_sq = math::distance_squared(center_pos, hit_pos);
      if (dist_sq < best_dist_sq) {
        best_dist_sq = dist_sq;
        r_pos = hit_pos;
        found = true;
      }
    }
  };

  BMIter fiter;
  BMFace *f;
  BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
    if (f->len < 3 || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (f->len == 3 || f->len == 4) {
      BMLoop *l_start = f->l_first;
      BMVert *v1 = l_start->prev->v;
      BMVert *v2 = l_start->v;
      BMVert *v3 = l_start->next->v;
      test_tri_fn(v1, v2, v3);

      if (f->len == 4) {
        BMVert *v4 = l_start->next->next->v;
        test_tri_fn(v1, v3, v4);
      }
    }
    else {
      const Array<std::array<BMVert *, 3>> &cached = tess_cache.lookup_or_add_cb(f, [&]() {
        const int tottri = f->len - 2;
        Array<BMLoop *, BM_DEFAULT_NGON_STACK_SIZE> loops(f->len);
        Array<std::array<uint, 3>, BM_DEFAULT_NGON_STACK_SIZE> index(tottri);
        BM_face_calc_tessellation(
            f, false, loops.data(), reinterpret_cast<uint(*)[3]>(index.data()));
        Array<std::array<BMVert *, 3>> tris(tottri);
        for (int i = 0; i < tottri; i++) {
          tris[i] = {loops[index[i][0]]->v, loops[index[i][1]]->v, loops[index[i][2]]->v};
        }
        return tris;
      });
      for (const std::array<BMVert *, 3> &tri : cached) {
        test_tri_fn(tri[0], tri[1], tri[2]);
      }
    }
  }

  if (found) {
    return;
  }

  BMIter eiter;
  BMEdge *e;
  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    float3 closest;
    closest_to_line_v3(closest, center_pos, e->v1->co, e->v2->co);
    const float fac = line_point_factor_v3(closest, e->v1->co, e->v2->co);
    if (fac > CIRCULARIZE_EPSILON && fac < 1.0f - CIRCULARIZE_EPSILON) {
      const float dist_sq = math::distance_squared(center_pos, closest);
      if (dist_sq < best_dist_sq) {
        best_dist_sq = dist_sq;
        r_pos = closest;
        found = true;
      }
    }
  }

  if (found) {
    return;
  }

  if (bvh_tree) {
    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    nearest.index = -1;
    BLI_bvhtree_find_nearest(bvh_tree, center_pos, &nearest, nearest_tri_cb, bvh_data);
    if (nearest.index != -1) {
      r_pos = float3(nearest.co);
      return;
    }
  }

  r_pos = center_pos;
}

void bmo_circularize_exec(BMesh *bm, BMOperator *op)
{
  const float factor = BMO_slot_float_get(op->slots_in, "factor");
  const float custom_radius = BMO_slot_float_get(op->slots_in, "custom_radius");
  const float angle = BMO_slot_float_get(op->slots_in, "angle");
  const int fit_method = BMO_slot_int_get(op->slots_in, "fit_method");
  const float flatten = BMO_slot_float_get(op->slots_in, "flatten");
  const bool regular = BMO_slot_bool_get(op->slots_in, "regular");

  const bool check_axis[3] = {
      BMO_slot_bool_get(op->slots_in, "mirror_x"),
      BMO_slot_bool_get(op->slots_in, "mirror_y"),
      BMO_slot_bool_get(op->slots_in, "mirror_z"),
  };

  const bool lock_x = BMO_slot_bool_get(op->slots_in, "lock_x");
  const bool lock_y = BMO_slot_bool_get(op->slots_in, "lock_y");
  const bool lock_z = BMO_slot_bool_get(op->slots_in, "lock_z");

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(
      bm, op->slots_in, "geom", BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  Vector<VertChain> chains;
  bm_vert_chain_extract_from_boundary_edges(bm, chains, BM_ELEM_TAG, check_axis);

  /* Builds a BVH tree when flatten is disabled. Without this we would have to iterate
   * over every face in the mesh for every vertex which is too slow.
   *
   * Note: There is the possibility of a feedback loop here, with the geometry
   * being manipulated which is used in the BVH tree. However, in practice this
   * is an acceptable limitation that is unlikely to cause problems. */
  Vector<std::array<BMLoop *, 3>> looptris;
  BVHTree *bvh_tree = nullptr;
  NearestTriUserData bvh_data = {};
  FaceTessellationCache tess_cache;

  if (flatten < 1.0f) {
    const int tot_tri = poly_to_tri_count(bm->totface, bm->totloop);
    looptris.reinitialize(tot_tri);
    BM_mesh_calc_tessellation(bm, looptris);

    bvh_tree = BLI_bvhtree_new(tot_tri, 0.0f, 8, 8);
    for (const int i : looptris.index_range()) {
      const std::array<BMLoop *, 3> &ltri = looptris[i];
      float3 tri_coords[3] = {
          float3(ltri[0]->v->co), float3(ltri[1]->v->co), float3(ltri[2]->v->co)};
      BLI_bvhtree_insert(bvh_tree, i, reinterpret_cast<float *>(tri_coords), 3);
    }
    BLI_bvhtree_balance(bvh_tree);
    bvh_data.looptris = looptris;
  }

  for (VertChain &chain_data : chains) {
    Vector<BMVert *> &chain = chain_data.verts;
    float3 normal_accum = float3(0.0f);
    for (BMVert *v : chain) {
      normal_accum += float3(v->no);
    }
    float3 center_3d;
    float3x3 mat = bm_vert_chain_orientation_matrix_calc(chain, center_3d);

    /* Reverse the chain winding if the Newell normal opposes the cumulative vertex normal. */
    if (math::dot(mat.z_axis(), normal_accum) < 0.0f) {
      std::reverse(chain.begin(), chain.end());
      mat = bm_vert_chain_orientation_matrix_calc(chain, center_3d);
    }

    bool is_mirrored = false;
    int mirror_axis = -1;

    if (!chain_data.is_closed) {
      BMVert *v_start = chain.first();
      BMVert *v_end = chain.last();

      for (int i = 0; i < 3; i++) {
        if (check_axis[i] && std::abs(v_start->co[i]) < MIRROR_LIMIT &&
            std::abs(v_end->co[i]) < MIRROR_LIMIT)
        {
          is_mirrored = true;
          mirror_axis = i;
        }
      }
    }

    /* For open chains on a symmetry plane, force the center to the midpoint of the endpoints
     * to keep the circle aligned with the mirror plane. */
    if (is_mirrored) {
      BMVert *v_start = chain.first();
      BMVert *v_end = chain.last();

      center_3d = math::midpoint(float3(v_start->co), float3(v_end->co));
      float3 p = math::normalize(float3(v_start->co) - center_3d);
      float3 q = math::normalize(math::cross(mat.z_axis(), p));
      mat.x_axis() = p;
      mat.y_axis() = q;
    }

    Vector<CircleVert> circle_verts;
    project_chain_to_2d(chain, center_3d, mat, circle_verts);

    float2 circle_center_2d;
    float radius;

    std::optional<float2> fixed_center = std::nullopt;
    if (is_mirrored) {
      fixed_center = float2(0.0f);
    }

    if (fit_method == FIT_METHOD_CONTRACT) {
      calculate_circle_inside_fit(circle_verts, fixed_center, circle_center_2d, &radius);
    }
    else {
      calculate_circle_best_fit(circle_verts, fixed_center, circle_center_2d, &radius);
    }

    if (custom_radius > 0.0f) {
      radius = custom_radius;
    }

    calculate_target_locations(
        circle_verts, circle_center_2d, radius, regular, chain_data.is_closed, angle);

    for (const CircleVert &cv : circle_verts) {
      const float3 target_local(cv.target_2d.x, cv.target_2d.y, 0.0f);
      float3 final_pos = center_3d + mat * target_local;

      if (flatten < 1.0f) {
        float3 projected_pos;
        project_on_mesh(
            bvh_tree, &bvh_data, cv.v, final_pos, mat.z_axis(), projected_pos, tess_cache);
        interp_v3_v3v3(final_pos, projected_pos, final_pos, flatten);
      }

      /* If this vertex is an endpoint of a mirrored chain, force it
       * exactly to 0.0 on the mirror axis.
       * There are some cases where a slight floating point drift ends up being
       * produced which prevents the mirror modifier from merging vertices. */
      if (is_mirrored) {
        BLI_assert(mirror_axis != -1);
        if (cv.v == chain.first() || cv.v == chain.last()) {
          final_pos[mirror_axis] = 0.0f;
        }
      }

      /* If an axis is locked, restore the original coordinate. */
      if (lock_x || lock_y || lock_z) {
        const float *orig = cv.v->co;
        if (lock_x) {
          final_pos.x = orig[0];
        }
        if (lock_y) {
          final_pos.y = orig[1];
        }
        if (lock_z) {
          final_pos.z = orig[2];
        }
      }

      interp_v3_v3v3(cv.v->co, cv.v->co, final_pos, factor);
    }
  }

  /* There would be a memory leak if this isn't freed. */
  if (bvh_tree) {
    BLI_bvhtree_free(bvh_tree);
  }
}

}  // namespace blender
