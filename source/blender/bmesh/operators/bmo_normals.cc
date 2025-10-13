/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Functionality for flipping faces to make normals consistent.
 */

#include "MEM_guardedalloc.h"

#include "BLI_linklist_stack.h"
#include "BLI_math_vector.h"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

/********* Right-hand faces implementation ****** */

#define FACE_FLAG (1 << 0)
#define FACE_FLIP (1 << 1)
#define FACE_TEMP (1 << 2)

static bool bmo_recalc_normal_loop_filter_cb(const BMLoop *l, void * /*user_data*/)
{
  return BM_edge_is_manifold(l->e);
}

/**
 * This uses a more comprehensive test to see if the furthest face from the center
 * is pointing towards the center or not.
 *
 * A simple test could just check the dot product
 * of the faces-normal and the direction from the center,
 * however this can fail for faces which make a sharp spike. eg:
 *
 * <pre>
 * +
 * |\ <- face
 * + +
 * \ \
 *   \ \
 *    \ +--------------+
 *     \               |
 *      \ center -> +  |
 *       \             |
 *        +------------+
 * </pre>
 *
 * In the example above, the \a face can point towards the \a center
 * which would end up flipping the normals inwards.
 *
 * To take these spikes into account, find the furthest face-loop-vertex.
 */

/**
 * \return a face index in \a faces and set \a r_is_flip
 * if the face is flipped away from the center.
 */
static int recalc_face_normals_find_index(BMesh *bm,
                                          BMFace **faces,
                                          const int faces_len,
                                          bool *r_is_flip)
{
  const float eps = FLT_EPSILON;
  float cent_area_accum = 0.0f;
  float cent[3];
  const float cent_fac = 1.0f / float(faces_len);

  bool is_flip = false;
  int f_start_index;
  int i;

  /** Search for the best loop. Members are compared in-order defined here. */
  struct {
    /**
     * Squared distance from the center to the loops vertex 'l->v'.
     * The normalized direction between the center and this vertex
     * is also used for the dot-products below.
     */
    float dist_sq;
    /**
     * Signed dot product using the normalized edge vector,
     * (best of 'l->prev->v' or 'l->next->v').
     */
    float edge_dot;
    /**
     * Unsigned dot product using the loop-normal
     * (sign is used to check if we need to flip).
     */
    float loop_dot;
  } best, test;

  UNUSED_VARS_NDEBUG(bm);

  zero_v3(cent);

  /* first calculate the center */
  for (i = 0; i < faces_len; i++) {
    float f_cent[3];
    const float f_area = BM_face_calc_area(faces[i]);
    BM_face_calc_center_median_weighted(faces[i], f_cent);
    madd_v3_v3fl(cent, f_cent, cent_fac * f_area);
    cent_area_accum += f_area;

    BLI_assert(BMO_face_flag_test(bm, faces[i], FACE_TEMP) == 0);
    BLI_assert(BM_face_is_normal_valid(faces[i]));
  }

  if (cent_area_accum != 0.0f) {
    mul_v3_fl(cent, 1.0f / cent_area_accum);
  }

  /* Distances must start above zero,
   * or we can't do meaningful calculations based on the direction to the center */
  best.dist_sq = eps;
  best.edge_dot = best.loop_dot = -FLT_MAX;

  /* used in degenerate cases only */
  f_start_index = 0;

  /**
   * Find the outer-most vertex, comparing distance to the center,
   * then the outer-most loop attached to that vertex.
   *
   * Important this is correctly detected,
   * where casting a ray from the center won't hit any loops past this one.
   * Otherwise the result may be incorrect.
   */
  for (i = 0; i < faces_len; i++) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(faces[i]);
    do {
      bool is_best_dist_sq;
      float dir[3];
      sub_v3_v3v3(dir, l_iter->v->co, cent);
      test.dist_sq = len_squared_v3(dir);
      is_best_dist_sq = (test.dist_sq > best.dist_sq);
      if (is_best_dist_sq || (test.dist_sq == best.dist_sq)) {
        float edge_dir_pair[2][3];
        mul_v3_fl(dir, 1.0f / sqrtf(test.dist_sq));

        sub_v3_v3v3(edge_dir_pair[0], l_iter->next->v->co, l_iter->v->co);
        sub_v3_v3v3(edge_dir_pair[1], l_iter->prev->v->co, l_iter->v->co);

        if ((normalize_v3(edge_dir_pair[0]) > eps) && (normalize_v3(edge_dir_pair[1]) > eps)) {
          bool is_best_edge_dot;
          test.edge_dot = max_ff(dot_v3v3(dir, edge_dir_pair[0]), dot_v3v3(dir, edge_dir_pair[1]));
          is_best_edge_dot = (test.edge_dot > best.edge_dot);
          if (is_best_dist_sq || is_best_edge_dot || (test.edge_dot == best.edge_dot)) {
            float loop_dir[3];
            cross_v3_v3v3(loop_dir, edge_dir_pair[0], edge_dir_pair[1]);
            if (normalize_v3(loop_dir) > eps) {
              float loop_dir_dot;
              /* Highly unlikely the furthest loop is also the concave part of an ngon,
               * but it can be contrived with _very_ non-planar faces - so better check. */
              if (UNLIKELY(dot_v3v3(loop_dir, l_iter->f->no) < 0.0f)) {
                negate_v3(loop_dir);
              }
              loop_dir_dot = dot_v3v3(dir, loop_dir);
              test.loop_dot = fabsf(loop_dir_dot);
              if (is_best_dist_sq || is_best_edge_dot || (test.loop_dot > best.loop_dot)) {
                best = test;
                f_start_index = i;
                is_flip = (loop_dir_dot < 0.0f);
              }
            }
          }
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  *r_is_flip = is_flip;
  return f_start_index;
}

/**
 * Given an array of faces, recalculate their normals.
 * this functions assumes all faces in the array are connected by edges.
 *
 * \param bm:
 * \param faces: Array of connected faces.
 * \param faces_len: Length of \a faces
 * \param oflag: Flag to check before doing the actual face flipping.
 */
static void bmo_recalc_face_normals_array(BMesh *bm,
                                          BMFace **faces,
                                          const int faces_len,
                                          const short oflag)
{
  int i, f_start_index;
  const short oflag_flip = oflag | FACE_FLIP;
  bool is_flip;

  BMFace *f;

  BLI_LINKSTACK_DECLARE(fstack, BMFace *);

  f_start_index = recalc_face_normals_find_index(bm, faces, faces_len, &is_flip);

  if (is_flip) {
    BMO_face_flag_enable(bm, faces[f_start_index], FACE_FLIP);
  }

  /* now that we've found our starting face, make all connected faces
   * have the same winding.  this is done recursively, using a manual
   * stack (if we use simple function recursion, we'd end up overloading
   * the stack on large meshes). */
  BLI_LINKSTACK_INIT(fstack);

  BLI_LINKSTACK_PUSH(fstack, faces[f_start_index]);
  BMO_face_flag_enable(bm, faces[f_start_index], FACE_TEMP);

  while ((f = BLI_LINKSTACK_POP(fstack))) {
    const bool flip_state = BMO_face_flag_test_bool(bm, f, FACE_FLIP);
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BMLoop *l_other = l_iter->radial_next;

      if ((l_other != l_iter) && bmo_recalc_normal_loop_filter_cb(l_iter, nullptr)) {
        if (!BMO_face_flag_test(bm, l_other->f, FACE_TEMP)) {
          BMO_face_flag_enable(bm, l_other->f, FACE_TEMP);
          BMO_face_flag_set(bm, l_other->f, FACE_FLIP, (l_other->v == l_iter->v) != flip_state);
          BLI_LINKSTACK_PUSH(fstack, l_other->f);
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  BLI_LINKSTACK_FREE(fstack);

  /* apply flipping to oflag'd faces */
  for (i = 0; i < faces_len; i++) {
    if (BMO_face_flag_test(bm, faces[i], oflag_flip) == oflag_flip) {
      BM_face_normal_flip(bm, faces[i]);
    }
    BMO_face_flag_disable(bm, faces[i], FACE_TEMP);
  }
}

/**
 * Put normal to the outside, and set the first direction flags in edges
 *
 * then check the object, and set directions / direction-flags:
 * but only for edges with 1 or 2 faces this is in fact the 'select connected'
 *
 * in case all faces were not done: start over with 'find the ultimate ...'.
 */

void bmo_recalc_face_normals_exec(BMesh *bm, BMOperator *op)
{
  int *groups_array = MEM_malloc_arrayN<int>(bm->totface, __func__);
  BMFace **faces_grp = static_cast<BMFace **>(
      MEM_mallocN(sizeof(*faces_grp) * bm->totface, __func__));

  int (*group_index)[2];
  const int group_tot = BM_mesh_calc_face_groups(bm,
                                                 groups_array,
                                                 &group_index,
                                                 bmo_recalc_normal_loop_filter_cb,
                                                 nullptr,
                                                 nullptr,
                                                 0,
                                                 BM_EDGE);
  int i;

  BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_FLAG);

  BM_mesh_elem_table_ensure(bm, BM_FACE);

  for (i = 0; i < group_tot; i++) {
    const int fg_sta = group_index[i][0];
    const int fg_len = group_index[i][1];
    int j;
    bool is_calc = false;

    for (j = 0; j < fg_len; j++) {
      faces_grp[j] = BM_face_at_index(bm, groups_array[fg_sta + j]);

      if (is_calc == false) {
        is_calc = BMO_face_flag_test_bool(bm, faces_grp[j], FACE_FLAG);
      }
    }

    if (is_calc) {
      bmo_recalc_face_normals_array(bm, faces_grp, fg_len, FACE_FLAG);
    }
  }

  MEM_freeN(faces_grp);

  MEM_freeN(groups_array);
  MEM_freeN(group_index);
}
