/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Convert triangle to quads.
 *
 * TODO
 * - convert triangles to any sided faces, not just quads.
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_heap.h"
#include "BLI_math_base.h"
#include "BLI_math_geom.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.hh"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

/**
 *  Used to keep track of our math for the error values and ensure it's not getting out of control.
 */
#define ASSERT_VALID_ERROR_METRIC(val) BLI_assert(isfinite(val) && (val) >= 0 && (val) <= 2 * M_PI)

#if 0
/**
 * This define allows a developer to interrupt the operator midway through
 * to visualize and debug the actions being taken by the merge algorithm.
 *
 * This define could be enabled for all debug builds, even when the `merge_limit` or
 * `neighbor_debug` parameters might not be being passed. For example, if the API interface
 * is turned off in `editmesh_tools.cc`, or if join_triangles is called from other operators
 * such as convex_hull that don't expose the testing API, the testing code still behaves.
 * When merge_limit and `neighbor_debug` are left unset, the default values pick the
 * normal processing path.
 *
 * Usage
 * =====
 *
 * `merge_limit` selects how many merges are performed before stopping in the middle to
 * visualize intermediate results.
 * In the UI, this has a range of `-1...n`, but in the operator parameters, this is actually
 * passed as `0...n+1`. This allows the default '0' in the parameter to be a sensible default.
 *
 * `neighbor_debug` allows each step in the neighbor improvement process to be visualized.
 * When nonzero, the quad that was merged and the two triangles being considered for adjustment
 * are left selected for visualization. Additionally, the neighbor quads are adjusted to their
 * "flattened" position to be in-plane with the quad, to allow visualization of that.
 * The numerical values related to the improvements are printed to the text console.
 *
 * `merge_limit = -1` allows the algorithm to run fully.
 * `merge_limit = 0` stops before the first merge.
 * `neighbor_debug` can be stepped to diagnose every neighbor improvement
 * that occurs as a result of the pre-existing quads in the mesh (valid
 * range for `neighbor_debug = 0...8*(number of selected pre-existing quads)`
 * `merge_limit = 1, 2, 3...` stops after the specified number of merges.
 * `neighbor_debug` shows the neighbor improvements for the last quad that merged.
 * (Valid range 0...8)
 *
 * Testing
 * =======
 *
 * To turn on interactive testing, the developer needs to:
 * - enable #USE_JOIN_TRIANGLE_INTERACTIVE_TESTING here.
 * - enable #USE_JOIN_TRIANGLE_INTERACTIVE_TESTING in `bmesh_opdefines.cc`.
 * - enable #USE_JOIN_TRIANGLE_TESTING_API in `editmesh_tools.cc`.
 */
#  define USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
#endif

#define FACE_OUT (1 << 0)
#define FACE_INPUT (1 << 2)

/**
 * Improvement ranges from 0..1. Never improve fully, limit at 99% improvement.
 *
 * If you allow 100% improvement around an existing quad,
 * then all the quad's neighbors end up improved to the with the exact same value.
 * When this occurs, the relative quality of the edges is lost.
 * Keeping 1% of the original error is enough to maintain relative sorting.
 */
constexpr float maximum_improvement = 0.99f;

/* -------------------------------------------------------------------- */
/** \name Join Edges state
 * pass a struct to ensure we don't have to pass these four variables everywhere.
 * \{ */

struct JoinEdgesState {
  /** A priority queue of `BMEdge *` to be merged, in order of preference. */
  Heap *edge_queue;

  /**
   * An edge aligned array for looking up the node from the edge index.
   * Only needed when `use_topo_influence` is true, so edges can be re-prioritized.
   */
  HeapNode **edge_queue_nodes;

  /** True when topo_influnce is not equal to zero. Allows skipping expensive processing. */
  bool use_topo_influence;

  /** An operator property indicating the influence for topology. Ranges from 0-2.0. */
  float topo_influnce;

  /** An operator property indicating to select all merged quads, or just un-merged triangles. */
  bool select_tris_only;

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  /** Needed for flagging faces. */
  BMesh *debug_bm;

  /**
   * This is a count of the number of merges to allow before stopping.
   * This is a UI parameter set by the user when debugging.
   */
  int debug_merge_limit;

  /** This is a count of how many merges have been processed so far. */
  int debug_merge_count;

  /**
   * This is the index of the neighbor edge improvement to be visualized, or 0, if none.
   * This is a UI parameter set by the user when debugging.
   * valid range `0...8*n` on step 0 (processing the n existing quads in initial selection)
   * valid range `0...8` on each edge merge.
   * The visualization algorithm behaves, if you set it too high, just doesn't do anything.
   */
  int debug_neighbor;

  /**
   * This is a count of how many neighbors have been processed so far
   * This is used to count neighbor merges during step 0 (processing existing quads).
   */
  int debug_neighbor_global_count;

  /**
   * This is a flag which indicates if this is the algorithm step the user has chosen for
   * interactive debug. When set, prints values, modifies the selection logic, halts processing
   * partway through, etc.
   */
  bool debug_this_step;
#endif
};

/** \} */
/* -------------------------------------------------------------------- */

/**
 * Computes error of a proposed merge quad. Quads with the lowest error are merged first.
 *
 * A quad that is a flat plane has lower error.
 *
 * A quad with four corners that are all right angles has lower error.
 * Note parallelograms are higher error than squares or rectangles.
 *
 * A quad that is concave has higher error.
 *
 * \param v1,v2,v3,v4: The four corner coordinates of the quad.
 * \return The computed error associated with the quad.
 */
static float quad_calc_error(const float v1[3],
                             const float v2[3],
                             const float v3[3],
                             const float v4[3])
{
  float error = 0.0f;

  /* Normal difference: a perfectly flat planar face adds a difference of zero. */
  {
    float n1[3], n2[3];
    float angle_a, angle_b;
    float diff;

    normal_tri_v3(n1, v1, v2, v3);
    normal_tri_v3(n2, v1, v3, v4);
    angle_a = compare_v3v3(n1, n2, FLT_EPSILON) ? 0.0f : angle_normalized_v3v3(n1, n2);

    normal_tri_v3(n1, v2, v3, v4);
    normal_tri_v3(n2, v4, v1, v2);
    angle_b = compare_v3v3(n1, n2, FLT_EPSILON) ? 0.0f : angle_normalized_v3v3(n1, n2);

    diff = (angle_a + angle_b) / float(M_PI * 2);

    ASSERT_VALID_ERROR_METRIC(diff);

    error += diff;
  }

  /* Co-linearity: a face with four right angle corners adds a difference of zero. */
  {
    float edge_vecs[4][3];
    float diff;

    sub_v3_v3v3(edge_vecs[0], v1, v2);
    sub_v3_v3v3(edge_vecs[1], v2, v3);
    sub_v3_v3v3(edge_vecs[2], v3, v4);
    sub_v3_v3v3(edge_vecs[3], v4, v1);

    normalize_v3(edge_vecs[0]);
    normalize_v3(edge_vecs[1]);
    normalize_v3(edge_vecs[2]);
    normalize_v3(edge_vecs[3]);

    /* A completely skinny face is 'pi' after halving. */
    diff = (fabsf(angle_normalized_v3v3(edge_vecs[0], edge_vecs[1]) - float(M_PI_2)) +
            fabsf(angle_normalized_v3v3(edge_vecs[1], edge_vecs[2]) - float(M_PI_2)) +
            fabsf(angle_normalized_v3v3(edge_vecs[2], edge_vecs[3]) - float(M_PI_2)) +
            fabsf(angle_normalized_v3v3(edge_vecs[3], edge_vecs[0]) - float(M_PI_2))) /
           float(M_PI * 2);

    ASSERT_VALID_ERROR_METRIC(diff);

    error += diff;
  }

  /* Concavity: a face with no concavity adds an error of 0. */
  {
    float area_min, area_max, area_a, area_b;
    float diff;

    area_a = area_tri_v3(v1, v2, v3) + area_tri_v3(v1, v3, v4);
    area_b = area_tri_v3(v2, v3, v4) + area_tri_v3(v4, v1, v2);

    area_min = min_ff(area_a, area_b);
    area_max = max_ff(area_a, area_b);

    /* Note use of ternary operator to guard against divide by zero. */
    diff = area_max ? (1.0f - (area_min / area_max)) : 1.0f;

    ASSERT_VALID_ERROR_METRIC(diff);

    error += diff;
  }

  ASSERT_VALID_ERROR_METRIC(error);

  return error;
}

/**
 * Get the corners of the quad that would result after an edge merge.
 *
 * \param e: An edge to be merged. It must be manifold and have triangles on either side.
 * \param r_v_quad: An array of vertices to return the corners.
 */
static void bm_edge_to_quad_verts(const BMEdge *e, const BMVert *r_v_quad[4])
{
  BLI_assert(BM_edge_is_manifold(e));
  BLI_assert(e->l->f->len == 3 && e->l->radial_next->f->len == 3);
  r_v_quad[0] = e->l->v;
  r_v_quad[1] = e->l->radial_next->prev->v;
  r_v_quad[2] = e->l->next->v;
  r_v_quad[3] = e->l->prev->v;
}

/* -------------------------------------------------------------------- */
/** \name Delimit processing
 * \{ */

/** Cache custom-data delimiters. */

namespace {

struct DelimitData_CD {
  int cd_type;
  int cd_size;
  int cd_offset;
  int cd_offset_end;
};

struct DelimitData {
  uint do_seam : 1;
  uint do_sharp : 1;
  uint do_mat : 1;
  uint do_angle_face : 1;
  uint do_angle_shape : 1;

  float angle_face;
  float angle_face__cos;

  float angle_shape;

  DelimitData_CD cdata[4];
  int cdata_len;
};

}  // namespace

/** Determines if the loop custom-data is contiguous. */
static bool bm_edge_is_contiguous_loop_cd_all(const BMEdge *e, const DelimitData_CD *delimit_data)
{
  int cd_offset;
  for (cd_offset = delimit_data->cd_offset; cd_offset < delimit_data->cd_offset_end;
       cd_offset += delimit_data->cd_size)
  {
    if (BM_edge_is_contiguous_loop_cd(e, delimit_data->cd_type, cd_offset) == false) {
      return false;
    }
  }

  return true;
}

/** Looks up delimit data from custom data. Used to delimit by color or UV. */
static bool bm_edge_delimit_cdata(CustomData *ldata,
                                  eCustomDataType type,
                                  DelimitData_CD *r_delim_cd)
{
  const int layer_len = CustomData_number_of_layers(ldata, type);
  r_delim_cd->cd_type = type;
  r_delim_cd->cd_size = CustomData_sizeof(eCustomDataType(r_delim_cd->cd_type));
  r_delim_cd->cd_offset = CustomData_get_n_offset(ldata, type, 0);
  r_delim_cd->cd_offset_end = r_delim_cd->cd_offset + (r_delim_cd->cd_size * layer_len);
  return (r_delim_cd->cd_offset != -1);
}

/**
 * Setup the delimit data from the parameters provided to the operator.
 *
 * \param bm: The mesh to provide UV or color data.
 * \param op: The operator to provide the parameters.
 */
static DelimitData bm_edge_delmimit_data_from_op(BMesh *bm, BMOperator *op)
{
  DelimitData delimit_data = {0};
  delimit_data.do_seam = BMO_slot_bool_get(op->slots_in, "cmp_seam");
  delimit_data.do_sharp = BMO_slot_bool_get(op->slots_in, "cmp_sharp");
  delimit_data.do_mat = BMO_slot_bool_get(op->slots_in, "cmp_materials");

  /* Determine if angle face processing occurs and its parameters. */
  float angle_face = BMO_slot_float_get(op->slots_in, "angle_face_threshold");
  if (angle_face < DEG2RADF(180.0f)) {
    delimit_data.angle_face = angle_face;
    delimit_data.angle_face__cos = cosf(angle_face);
    delimit_data.do_angle_face = true;
  }
  else {
    delimit_data.do_angle_face = false;
  }

  /* Determine if angle shape processing occurs and its parameters. */
  float angle_shape = BMO_slot_float_get(op->slots_in, "angle_shape_threshold");
  if (angle_shape < DEG2RADF(180.0f)) {
    delimit_data.angle_shape = angle_shape;
    delimit_data.do_angle_shape = true;
  }
  else {
    delimit_data.do_angle_shape = false;
  }

  if (BMO_slot_bool_get(op->slots_in, "cmp_uvs") &&
      bm_edge_delimit_cdata(
          &bm->ldata, CD_PROP_FLOAT2, &delimit_data.cdata[delimit_data.cdata_len]))
  {
    delimit_data.cdata_len += 1;
  }

  delimit_data.cdata[delimit_data.cdata_len].cd_offset = -1;
  if (BMO_slot_bool_get(op->slots_in, "cmp_vcols") &&
      bm_edge_delimit_cdata(
          &bm->ldata, CD_PROP_BYTE_COLOR, &delimit_data.cdata[delimit_data.cdata_len]))
  {
    delimit_data.cdata_len += 1;
  }
  return delimit_data;
}

/**
 * Computes if an edge is a delimit edge, therefore should not be considered for merging.
 *
 * \param e: the edge to check
 * \param delimit_data: the delimit configuration
 * \return true, if the edge is a delimit edge.
 */
static bool bm_edge_is_delimit(const BMEdge *e, const DelimitData *delimit_data)
{
  BMFace *f_a = e->l->f, *f_b = e->l->radial_next->f;
#if 0
  const bool is_contig = BM_edge_is_contiguous(e);
  float angle;
#endif

  if (delimit_data->do_seam && BM_elem_flag_test(e, BM_ELEM_SEAM)) {
    return true;
  }

  if (delimit_data->do_sharp && (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == 0)) {
    return true;
  }

  if (delimit_data->do_mat && (f_a->mat_nr != f_b->mat_nr)) {
    return true;
  }

  if (delimit_data->do_angle_face) {
    if (dot_v3v3(f_a->no, f_b->no) < delimit_data->angle_face__cos) {
      return true;
    }
  }

  if (delimit_data->do_angle_shape) {
    const BMVert *verts[4];
    bm_edge_to_quad_verts(e, verts);

    /* if we're checking the shape at all, a flipped face is out of the question */
    if (is_quad_flip_v3(verts[0]->co, verts[1]->co, verts[2]->co, verts[3]->co)) {
      return true;
    }

    float edge_vecs[4][3];

    sub_v3_v3v3(edge_vecs[0], verts[0]->co, verts[1]->co);
    sub_v3_v3v3(edge_vecs[1], verts[1]->co, verts[2]->co);
    sub_v3_v3v3(edge_vecs[2], verts[2]->co, verts[3]->co);
    sub_v3_v3v3(edge_vecs[3], verts[3]->co, verts[0]->co);

    normalize_v3(edge_vecs[0]);
    normalize_v3(edge_vecs[1]);
    normalize_v3(edge_vecs[2]);
    normalize_v3(edge_vecs[3]);

    if ((fabsf(angle_normalized_v3v3(edge_vecs[0], edge_vecs[1]) - float(M_PI_2)) >
         delimit_data->angle_shape) ||
        (fabsf(angle_normalized_v3v3(edge_vecs[1], edge_vecs[2]) - float(M_PI_2)) >
         delimit_data->angle_shape) ||
        (fabsf(angle_normalized_v3v3(edge_vecs[2], edge_vecs[3]) - float(M_PI_2)) >
         delimit_data->angle_shape) ||
        (fabsf(angle_normalized_v3v3(edge_vecs[3], edge_vecs[0]) - float(M_PI_2)) >
         delimit_data->angle_shape))
    {
      return true;
    }
  }

  if (delimit_data->cdata_len) {
    int i;
    for (i = 0; i < delimit_data->cdata_len; i++) {
      if (!bm_edge_is_contiguous_loop_cd_all(e, &delimit_data->cdata[i])) {
        return true;
      }
    }
  }

  return false;
}

/** \} */

struct JoinEdgesNeighborItem {
  BMEdge *e;
  BMLoop *l;
};

struct JoinEdgesNeighborInfo {
  /** Logically there can only ever be 8 items in this array.
   *
   * Since a quad has no more than 4 neighbor triangles, and each neighbor triangle has no more
   * than two edges to consider, #reprioritize_face_neighbors can't possibly call this function
   * more than 8 times so this can't happen. Still, it's good to safeguard against running off
   * the end of the array.
   */
  JoinEdgesNeighborItem items[8];
  int items_num;
};

/**
 * Adds edges and loops to an array of neighbors, but won't add duplicates a second time.
 *
 * This function is necessary because otherwise the 3rd edge attached to a 3-pole at the corner
 * of a freshly merged quad might be seen as a neighbor of _both_ the quad edges it touches,
 * (depending on the triangulation), and might get double the improvement it deserves.
 *
 * \param merge_edges: the array to add the merge edges to
 * \param shared_loops: the array to add the shared loops to
 * \param count: the number of items currently in each array.
 * \param e: The new merge edge to add to the array, if it's not a duplicate.
 * \param l: The new shared loop to add to the array, if the edge isn't a duplicate
 */
static void add_without_duplicates(JoinEdgesNeighborInfo &neighbor_info, BMEdge *e, BMLoop *l)
{
  BLI_assert(neighbor_info.items_num < ARRAY_SIZE(neighbor_info.items));

  /* Don't add null pointers. Another safeguard which cannot happen. */
  BLI_assert(e != nullptr);

  /* Don't add duplicates. */
  for (uint index = 0; index < neighbor_info.items_num; index++) {
    if (neighbor_info.items[neighbor_info.items_num].e == e) {
      return;
    }
  }

  /* Add the edge and increase the count by 1. */
  JoinEdgesNeighborItem *item = &neighbor_info.items[neighbor_info.items_num++];
  item->e = e;
  item->l = l;
}

/**
 * Add the neighboring edges of a given loop to the `merge_edges` and `shared_loops` arrays.
 *
 * \param merge_edges: the array of mergeable edges to add to.
 * \param shared_loops: the array to shared loops to add to.
 * \param count: the number of items currently in each array.
 * \param l_in_quad: The loop to add the neighboring edges of, if they check out.
 */
static void add_neighbors(JoinEdgesNeighborInfo &neighbor_info, BMLoop *l_in_quad)
{
  /* If the edge is not manifold, there is no neighboring face to process. */
  if (!BM_edge_is_manifold(l_in_quad->e)) {
    /* No new edges added. */
    return;
  }

  BMLoop *l_in_neighbor = l_in_quad->radial_next;

  /* If the neighboring face is not a triangle, don't process it. */
  if (l_in_neighbor->f->len != 3) {
    /* No new edges added. */
    return;
  }

#ifndef NDEBUG
  const int items_num_prev = neighbor_info.items_num;
#endif

  /* Get the other two loops of the neighboring triangle. */
  BMLoop *l_other_arr[2] = {l_in_neighbor->prev, l_in_neighbor->next};
  for (int i = 0; i < ARRAY_SIZE(l_other_arr); i++) {
    BMLoop *l_other = l_other_arr[i];

    /* If `l_other` is manifold, and the adjacent face is also a triangle,
     * mark it for potential improvement. */
    if (BM_edge_is_manifold(l_other->e) && l_other->radial_next->f->len == 3) {
      add_without_duplicates(neighbor_info, l_other->e, l_in_neighbor);
    }
  }

  /* Added either 0, 1, or 2 edges. */
#ifndef NDEBUG
  BLI_assert(neighbor_info.items_num - items_num_prev < 3);
#endif
}

/**
 * Compute the coordinates of a quad that would result from an edge join, if that quad was
 * rotated into the same plane as the existing quad next to it.
 *
 * \param s: State information about the join_triangles process
 * \param quad_verts: Four vertices of a quad, which has l_shared as one of its edges
 * \param l_shared: the 'hinge' loop, shared with the neighbor, that lies in the plane.
 * \param plane_normal: The normal vector of the plane to rotate the quad to lie aligned with
 * \param r_quad_coordinates: An array of coordinates to return the four corrected vertex locations
 */
static void rotate_to_plane(const JoinEdgesState &s,
                            const BMVert *quad_verts[4],
                            const BMLoop *l_shared,
                            const float plane_normal[3],
                            float r_quad_coordinates[4][3])
{
#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  if (s.debug_this_step) {
    printf("angle");
  }
#else
  UNUSED_VARS(s);
#endif

  float rotation_axis[3];
  sub_v3_v3v3(rotation_axis, l_shared->v->co, l_shared->next->v->co);
  normalize_v3(rotation_axis);

  float quad_normal[3] = {0};
  normal_quad_v3(
      quad_normal, quad_verts[0]->co, quad_verts[1]->co, quad_verts[2]->co, quad_verts[3]->co);

  float angle = angle_signed_on_axis_v3v3_v3(plane_normal, quad_normal, rotation_axis);

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  if (s.debug_this_step) {
    printf(" %f, ", RAD2DEGF(angle));
  }
#endif

  for (int i = 0; i < 4; i++) {
    if (ELEM(quad_verts[i], l_shared->v, l_shared->next->v)) {
      /* Two coordinates of the quad match the vector that defines the axis of rotation, so they
       * don't change. */
      copy_v3_v3(r_quad_coordinates[i], quad_verts[i]->co);
    }
    else {
      /* The other two coordinates get rotated around the axis, and so they change. */
      float local_coordinate[3];
      sub_v3_v3v3(local_coordinate, quad_verts[i]->co, l_shared->v->co);
      rotate_normalized_v3_v3v3fl(r_quad_coordinates[i], local_coordinate, rotation_axis, angle);
      add_v3_v3(r_quad_coordinates[i], l_shared->v->co);
    }
  }
}

/**
 * Given a pair of quads, compute how well aligned they are.
 *
 * Computes a float, indicating alignment.
 * - regular grids of squares have pairs with alignments near 1.
 * - regular grids of parallelograms also have pairs with alignments near 1.
 * - mismatched combinations of squares, diamonds, parallelograms, trapezoids, etc
 *   have alignments near 0.
 * - however, pairs of quads which lie in perpendicular or opposite-facing planes can
 *   still have good alignments. In other words, pairs of quads which share an edge that
 *   defines a sharp corner on a mesh can still have good alignment, if the quads flow
 *   over the corner in a natural way. The sharp corner *alone* is *not* a penalty.
 *
 * \param s: State information about the join_triangles process.
 * \param quad_a_vecs: an array of four unit vectors.
 * These are *not* the coordinates of
 * the four vertices of quad_a. Instead, They are four unit vectors, aligned
 * parallel to the respective edge loop of quad_a.
 * \param quad_b_verts: an array of four vertices, giving the four corners of `quad_b`.
 * \param l_shared: a loop known to be one of the common manifold loops that is
 * shared between the two quads. This is used as a 'hinge' to flatten the two
 * quads into the same plane as much as possible.
 * \param plane_normal: The normal vector of quad_a.
 *
 * \return the computed alignment
 *
 * \note Since we test quad A against up to eight other quads, we precompute and pass in the
 * quad_a_vecs, instead of starting with verts, and having to recompute the same numbers
 * eight different times.
 * That is why the quad_a_vecs and quad_b_verts have different type definitions.
 */
static float compute_alignment(const JoinEdgesState &s,
                               const float quad_a_vecs[4][3],
                               const BMVert *quad_b_verts[4],
                               const BMLoop *l_shared,
                               const float plane_normal[3])
{
  /* Many meshes have lots of curvature or sharp edges. Pairs of quads shouldn't be penalized
   * *worse* because they represent a curved surface or define an edge. So we rotate quad_b around
   * its common edge with quad_a until both are, as much as possible, in the same plane.
   * This ensures the best possible chance to align. */
  float quad_b_coordinates[4][3];
  rotate_to_plane(s, quad_b_verts, l_shared, plane_normal, quad_b_coordinates);

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  if (s.debug_this_step) {
    /* For visualization purposes *only*, rotate the face being considered.
     * The const_cast here is purposeful. We want to specify `const BMVert` deliberately
     * to show that we're not *supposed* to be moving verts around.
     * But only for debug visualization, we do. This alters the mesh to visualize the
     * effect of rotating the face into the plane for alignment testing. */
    copy_v3_v3(const_cast<BMVert *>(quad_b_verts[0])->co, quad_b_coordinates[0]);
    copy_v3_v3(const_cast<BMVert *>(quad_b_verts[1])->co, quad_b_coordinates[1]);
    copy_v3_v3(const_cast<BMVert *>(quad_b_verts[2])->co, quad_b_coordinates[2]);
    copy_v3_v3(const_cast<BMVert *>(quad_b_verts[3])->co, quad_b_coordinates[3]);
  }
#endif

  /* compute the four unit vectors of the quad b edges. */
  float quad_b_vecs[4][3];
  sub_v3_v3v3(quad_b_vecs[0], quad_b_coordinates[0], quad_b_coordinates[1]);
  sub_v3_v3v3(quad_b_vecs[1], quad_b_coordinates[1], quad_b_coordinates[2]);
  sub_v3_v3v3(quad_b_vecs[2], quad_b_coordinates[2], quad_b_coordinates[3]);
  sub_v3_v3v3(quad_b_vecs[3], quad_b_coordinates[3], quad_b_coordinates[0]);
  normalize_v3(quad_b_vecs[0]);
  normalize_v3(quad_b_vecs[1]);
  normalize_v3(quad_b_vecs[2]);
  normalize_v3(quad_b_vecs[3]);

  /* Given that we're not certain of how the first loop of the quad and the first loop
   * of the proposed merge quad relate to each other, there are four possible combinations
   * to check, to test that the neighbor face and the merged face have good alignment.
   *
   * In theory, a very nuanced analysis involving l_shared, loop pointers, vertex pointers,
   * etc, would allow determining which sets of vectors are the right matches sets to compare.
   *
   * Do not meddle in the affairs of algorithms, for they are subtle and quick to anger.
   *
   * Instead, this code does the math twice, then it just flips each component by 180 degrees to
   * pick up the other two cases. Four extra angle tests aren't that much worse than optimal.
   * Brute forcing the math and ending up with clear and understandable code is better. */

  float error[4] = {0.0f};
  for (int i = 0; i < ARRAY_SIZE(error); i++) {
    const float angle_a = fabsf(angle_normalized_v3v3(quad_a_vecs[i], quad_b_vecs[i]));
    const float angle_b = fabsf(angle_normalized_v3v3(quad_a_vecs[i], quad_b_vecs[(i + 1) % 4]));

    /* Compute the case if the quads are aligned. */
    error[0] += angle_a;
    /* Compute the case if the quads are 90 degrees rotated. */
    error[1] += angle_b;

    /* Compute the case if the quads are 180 degrees rotated.
     * This is `error[0]` except each error component is individually rotated 180 degrees. */
    error[2] += M_PI - angle_a;

    /* Compute the case if the quads are 270 degrees rotated.
     * This is `error[1]` except each error component is individually rotated 180 degrees. */
    error[3] += M_PI - angle_b;
  }

  /* Pick the best option and average the four components. */
  const float best_error = std::min({error[0], error[1], error[2], error[3]}) / 4.0f;

  ASSERT_VALID_ERROR_METRIC(best_error);

  /* Based on the best error, we scale how aligned we are to the range 0...1
   * `M_PI / 4` is used here because the worst case is a quad with all four edges
   * at 45 degree angles. */
  float alignment = 1.0f - (best_error / (M_PI / 4.0f));

  /* if alignment is *truly* awful, then do nothing. Don't make a join worse. */
  alignment = std::max(alignment, 0.0f);

  ASSERT_VALID_ERROR_METRIC(alignment);

  return alignment;
}

/**
 * Lowers the error of an edge because of its proximity to a known good quad.
 *
 * This function is the core of the entire topology_influence algorithm.
 *
 * This function allows an existing, good quad to influence the topology around it.
 * This means a quad with a higher error can end up preferred - when it creates better topology -
 * even though there might be an alternate quad with lower numerical error.
 *
 * This algorithm reduces the error of a given edge based on three factors:
 * - The error of the neighboring quad. The better the neighbor quad, the more the impact.
 * - The alignment of the proposed new quad the existing quad.
 *   Grids of rectangles or trapezoids improve well. Trapezoids and diamonds are left alone.
 * - topology_influence. The higher the operator parameter is set, the more the impact.
 *   To help counteract the alignment penalty, topology_influence is permitted to exceed 100%.
 *
 * Because of the reduction due to misalignment, this will reduce the error of an edge, to be
 * closer to the error of the known good quad, and increase its changes of being merged sooner.
 * However, some of the edge's error always remains - it never is made *equal* to the lower error
 * from the good face. This means the influence of an exceptionally good quad will fade away with
 * each successive, neighbor, instead of affecting the *entire* mesh. This is desirable.
 *
 * \param s: State information about the join_triangles process
 * \param e_merge: the edge to improve
 * \param l_shared: the edge that is common between the two faces
 * \param neighbor_quad_vecs: four unit vectors, aligned to the four loops around the good quad
 * \param neighbor_quad_error: the error of the neighbor quad
 * \param neighbor_quad_normal: the normal vector of the good quad
 */
static void reprioritize_join(JoinEdgesState &s,
                              BMEdge *e_merge,
                              BMLoop *l_shared,
                              float neighbor_quad_vecs[4][3],
                              const float neighbor_quad_error,
                              const float neighbor_quad_normal[3])
{
  ASSERT_VALID_ERROR_METRIC(neighbor_quad_error);

  /* If the edge wasn't found, (delimit, non-manifold, etc) then return.
   * Nothing to do here. */
  BLI_assert(BM_elem_index_get(e_merge) >= 0);
  HeapNode *node = s.edge_queue_nodes[BM_elem_index_get(e_merge)];
  if (node == nullptr) {
    return;
  }

  float join_error_curr = BLI_heap_node_value(node);

  ASSERT_VALID_ERROR_METRIC(join_error_curr);

  /* Never make a join *worse* because of topology around it.
   * Because we are sorted during the join phase of the algorithm, this should *only* happen when
   * processing any pre-existing quads in the input mesh during setup. They might have high error.
   * If they do, ignore them. */
  if (neighbor_quad_error > join_error_curr) {

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
    /* This should only happen during setup.
     * Indicates an error, if it happens once we've started merging. */
    BLI_assert(s.debug_merge_count == 0);
#endif

    return;
  }

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  if (s.debug_this_step) {
    printf("Edge improved from ");
  }
#endif

  /* Get the four corners of the quad that would result if we merged. */
  const BMVert *quad_verts_merge[4];
  bm_edge_to_quad_verts(e_merge, quad_verts_merge);

  /* Now compute the alignment.
   * Regular grids of rectangles or trapezoids have high alignment
   * Mismatched combinations of rectangles diamonds and trapezoids have low alignment. */
  const float alignment = compute_alignment(
      s, neighbor_quad_vecs, quad_verts_merge, l_shared, neighbor_quad_normal);

  /* Compute how much the neighbor is better than the candidate.
   * Since the neighbor quad error is smaller, Improvement is always represented as negative. */
  const float improvement = (neighbor_quad_error - join_error_curr);

  ASSERT_VALID_ERROR_METRIC(-improvement);

  /* Compute the scale factor for how much of that possible improvement we should apply to this
   * edge. This combines... topology_influence, which is an operator setting... and alignment,
   * which is computed. Faces which are diagonal have an alignment of 0% - perfect rectangular
   * grids have an alignment of 100% Neither topology_influence nor alignment can be negative;
   * therefore the multiplier *never* makes error worse. once combined, 0 means no improvement, 1
   * means improve all the way to exactly match the quality of the contributing neighbor.
   * topology_influece is allowed to exceed 1.0, which lets it cancel out some of the alignment
   * penalty. */
  float multiplier = s.topo_influnce * alignment;

  /* However, the combined multiplier shouldn't ever be allowed to exceed 1.0 because permitting
   * that would cause exponential growth when alignment is very good, and when that happens, the
   * algorithm becomes crazy.
   *
   * Further, if we allow a multiplier of exactly 1.0, then all eight edges around the neighbor
   * quad would end up with a quality that is *exactly* equal to the neighbor - and each other;
   * losing valuable information about their relative sorting.
   * In order to preserve that, the multiplier is capped at 99%.
   * The last 1% that is left uncorrected is enough to preserve relative ordering.
   *
   * This especially helps in quads that touch 3-poles and 5-poles. Since those quads naturally
   * have diamond shapes, their initial error values tend to be higher and they sort to the end of
   * the priority queue. Limiting improvement at 99% ensures those quads tend to retain their bad
   * sort, meaning they end up surrounded by quads that define a good grid,
   * then they merge last, which tends to produce better results. */
  multiplier = std::min(multiplier, maximum_improvement);

  ASSERT_VALID_ERROR_METRIC(multiplier);

  /* improvement is always represented as a negative number (that will reduce error)
   * Based on that convention, `+` is correct here. */
  float join_error_next = join_error_curr + (improvement * multiplier);

  ASSERT_VALID_ERROR_METRIC(join_error_next);

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  if (s.debug_this_step) {
    printf("%f to %f with alignment of %f.\n", join_error_curr, join_error_next, alignment);
    BMO_face_flag_enable(s.debug_bm, e_merge->l->f, FACE_OUT);
    BMO_face_flag_enable(s.debug_bm, e_merge->l->radial_next->f, FACE_OUT);
  }
#endif

  /* Now, update the node value in the heap, which may cause the node
   * to be moved toward the head of the priority queue. */
  BLI_heap_node_value_update(s.edge_queue, node, join_error_next);
}

/**
 * Given a face, find merge_edges which are being considered for merge and improve them
 *
 * \param s: State information about the join_triangles process.
 * \param f: A quad.
 * \param f_error: The current error of the face.
 */
static void reprioritize_face_neighbors(JoinEdgesState &s, BMFace *f, float f_error)
{
  BLI_assert(f->len == 4);

  /* Identify any mergeable edges of any neighbor triangles that face us.
   * - Some of our four edges... might not be manifold.
   * - Some of our neighbor faces... might not be triangles.
   * - Some of our neighbor triangles... might have other non-manifold (unmergeable) edges.
   * - Some of our neighbor triangles' manifold edges... might have non-triangle neighbors.
   * Therefore, there can be have up to eight mergeable edges, although there are often fewer. */
  JoinEdgesNeighborInfo neighbor_info = {};

  /* Get the four loops around the face. */
  BMLoop *l_quad[4];
  BM_face_as_array_loop_quad(f, l_quad);

  /* Add the mergeable neighbors for each of those loops. */
  for (int i = 0; i < ARRAY_SIZE(l_quad); i++) {
    add_neighbors(neighbor_info, l_quad[i]);
  }

  /* Return if there is nothing to do. */
  if (neighbor_info.items_num == 0) {
    return;
  }

  /* Compute the four unit vectors around this quad. */
  float quad_vecs[4][3];
  for (int i_next = 0, i = ARRAY_SIZE(l_quad) - 1; i_next < ARRAY_SIZE(l_quad); i = i_next++) {
    sub_v3_v3v3(quad_vecs[i], l_quad[i]->v->co, l_quad[i_next]->v->co);
    normalize_v3(quad_vecs[i]);
  }

  /* Re-prioritize each neighbor. */
  for (int i = 0; i < neighbor_info.items_num; i++) {
#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
    s.debug_this_step = (s.debug_merge_limit > 0 && s.debug_merge_count == s.debug_merge_limit &&
                         i + 1 == s.debug_neighbor) ||
                        (++s.debug_neighbor_global_count == s.debug_neighbor &&
                         s.debug_merge_limit == 0);
#endif
    const JoinEdgesNeighborItem *item = &neighbor_info.items[i];
    reprioritize_join(s, item->e, item->l, quad_vecs, f_error, f->no);
  }
}
/**
 * Given a manifold edge, join the triangles on either side to form a quad.
 *
 * \param s: State information about the join_triangles process
 * \param e: the edge to merge. It must be manifold.
 * \return the face that resulted, or nullptr if the merge was rejected.
 */
static BMFace *bm_faces_join_pair_by_edge(BMesh *bm,
                                          BMEdge *e
#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
                                          ,
                                          JoinEdgesState &s
#endif
)
{
  /* Non-manifold edges can't be merged. */
  BLI_assert(BM_edge_is_manifold(e));

  /* Identify the loops on either side of the edge which may be joined. */
  BMLoop *l_a = e->l;
  BMLoop *l_b = e->l->radial_next;

  /* If previous face merges have created quads, which now make this edge unmergeable,
   * then skip it and move on. This happens frequently and that's ok.
   * It's much easier and more efficient to just skip these edges when we encounter them,
   * than it is to try to search the heap for them and remove them preemptively. */
  if ((l_a->f->len != 3) || (l_b->f->len != 3)) {
    return nullptr;
  }

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  /* Stop doing merges in the middle of processing if we reached a user limit.
   * This is allowed so a developer can check steps in the process of the algorithm. */
  if (++s.debug_merge_count > s.debug_merge_limit && s.debug_merge_limit != -1) {
    return nullptr;
  }
#endif

  BMFace *f_double;

  /* Join the edge and identify the face. */
  BMFace *f = BM_faces_join_pair(bm, l_a, l_b, true, &f_double);
  /* See #BM_faces_join note on callers asserting when `r_double` is non-null. */
  BLI_assert_msg(f_double == nullptr,
                 "Doubled face detected at " AT ". Resulting mesh may be corrupt.");

  return f;
}

/** Given a mesh, convert triangles to quads. */
void bmo_join_triangles_exec(BMesh *bm, BMOperator *op)
{
  BMIter iter;
  BMOIter siter;
  BMFace *f;
  BMEdge *e;

  DelimitData delimit_data = bm_edge_delmimit_data_from_op(bm, op);

  /* Initial setup of state. */
  JoinEdgesState s = {nullptr};
  s.topo_influnce = BMO_slot_float_get(op->slots_in, "topology_influence");
  s.use_topo_influence = (s.topo_influnce != 0.0f);
  s.edge_queue = BLI_heap_new();
  s.select_tris_only = BMO_slot_bool_get(op->slots_in, "deselect_joined");
  if (s.use_topo_influence) {
    s.edge_queue_nodes = MEM_malloc_arrayN<HeapNode *>(bm->totedge, __func__);
  }

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  s.debug_bm = bm;
  s.debug_merge_limit = BMO_slot_int_get(op->slots_in, "merge_limit") - 1;
  s.debug_neighbor = BMO_slot_int_get(op->slots_in, "neighbor_debug");
  s.debug_merge_count = 0;
  s.debug_neighbor_global_count = 0;
#endif

  /* Go through every face in the input slot. Mark triangles for processing. */
  BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
    if (f->len == 3) {
      BMO_face_flag_enable(bm, f, FACE_INPUT);

      /* And setup the initial selection. */
      if (s.select_tris_only) {
        BMO_face_flag_enable(bm, f, FACE_OUT);
      }
    }
  }

  /* Go through every edge in the mesh, mark edges that can be merged. */
  int i = 0;
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    BM_elem_index_set(e, i); /* set_inline */

    /* If the edge is manifold, has a tagged input triangle on both sides,
     * and is *not* delimited, then it's a candidate to merge. */
    BMFace *f_a, *f_b;
    if (BM_edge_face_pair(e, &f_a, &f_b) && BMO_face_flag_test(bm, f_a, FACE_INPUT) &&
        BMO_face_flag_test(bm, f_b, FACE_INPUT) && !bm_edge_is_delimit(e, &delimit_data))
    {
      /* Compute the error that would result from a merge. */
      const BMVert *e_verts[4];
      bm_edge_to_quad_verts(e, e_verts);
      const float merge_error = quad_calc_error(
          e_verts[0]->co, e_verts[1]->co, e_verts[2]->co, e_verts[3]->co);

      /* Record the candidate merge in both the heap, and the heap index. */
      HeapNode *node = BLI_heap_insert(s.edge_queue, merge_error, e);
      if (s.use_topo_influence) {
        s.edge_queue_nodes[i] = node;
      }
    }
    else {
      if (s.use_topo_influence) {
        s.edge_queue_nodes[i] = nullptr;
      }
    }
  }

  /* Go through all the faces of the input slot, this time to find quads.
   * Improve the candidates around any preexisting quads in the mesh.
   *
   * NOTE: This unfortunately misses any quads which are not selected, but
   * which neighbor the selection. The only alternate would be to iterate the
   * whole mesh, which might be expensive for very large meshes with small selections. */
  if (s.use_topo_influence && (BLI_heap_is_empty(s.edge_queue) == false)) {
    BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
      if (f->len == 4) {
        BMVert *f_verts[4];
        BM_face_as_array_vert_quad(f, f_verts);

        /* Flat quads with right angle corners and no concavity have lower error. */
        float f_error = quad_calc_error(
            f_verts[0]->co, f_verts[1]->co, f_verts[2]->co, f_verts[3]->co);

        /* Apply the compensated error.
         * Since we're early in the process we over-prioritize any already existing quads to
         * allow them to have an especially strong influence on the resulting mesh.
         * At a topology influence of 200%, they're considered to be *almost perfect* quads
         * regardless of their actual error. Either way, the multiplier is never completely
         * allowed to reach zero. Instead, 1% of the original error is preserved...
         * which is enough to maintain the relative priority sorting between existing quads. */
        f_error *= (2.0f - (s.topo_influnce * maximum_improvement));

        reprioritize_face_neighbors(s, f, f_error);
      }
    }
  }

  /* Process all possible merges. */
  while (!BLI_heap_is_empty(s.edge_queue)) {

    /* Get the best merge from the priority queue.
     * Remove it from the priority queue. */
    const float f_error = BLI_heap_top_value(s.edge_queue);
    BMEdge *e = reinterpret_cast<BMEdge *>(BLI_heap_pop_min(s.edge_queue));

    /* Attempt the merge. */
    BMFace *f_new = bm_faces_join_pair_by_edge(bm,
                                               e
#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
                                               ,
                                               s
#endif
    );

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
    /* If stopping partway through, clear the selection entirely, and instead
     * highlight the faces being considered in the step the user is checking. */
    if (s.debug_merge_limit != -1 && s.debug_merge_count == s.debug_merge_limit) {
      BMEdge *f;
      BMIter iter;
      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        BMO_face_flag_disable(bm, f, FACE_OUT);
      }
    }
#endif

    if (f_new) {

      /* Tag the face so the selection can be extended to include the new face. */
      if (s.select_tris_only == false) {
        BMO_face_flag_enable(bm, f_new, FACE_OUT);
      }

      /* Improve the neighbors on success. */
      if (s.use_topo_influence) {
        reprioritize_face_neighbors(s, f_new, f_error);
      }
    }

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
    /* If we're supposed to stop partway though, do that.
     * This allows a developer to inspect the mesh at intermediate stages of processing. */
    if (s.debug_merge_limit != -1 && s.debug_merge_count >= s.debug_merge_limit) {
      break;
    }
#endif
  }

#ifdef USE_JOIN_TRIANGLE_INTERACTIVE_TESTING
  /* Expect a full processing to have occurred, *only* if we didn't stop partway through. */
  if (!(s.debug_merge_limit != -1 && s.debug_merge_count >= s.debug_merge_limit))
#endif
  {
    /* Expect a full processing to have occurred. */
    BLI_assert(BLI_heap_is_empty(s.edge_queue));
  }

  /* Clean up. */
  BLI_heap_free(s.edge_queue, nullptr);
  if (s.use_topo_influence) {
    MEM_freeN(s.edge_queue_nodes);
  }

  /* Return the selection results. */
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
}
