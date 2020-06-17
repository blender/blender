/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bmesh
 *
 * Connect verts across faces (splits faces).
 */

#include "BLI_alloca.h"
#include "BLI_linklist_stack.h"
#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define VERT_INPUT 1

#define EDGE_OUT 1
/* Edge spans 2 VERT_INPUT's, its a nop,
 * but include in "edges.out" */
#define EDGE_OUT_ADJ 2

#define FACE_TAG 2
#define FACE_EXCLUDE 4

static int bm_face_connect_verts(BMesh *bm, BMFace *f, const bool check_degenerate)
{
  const uint pair_split_max = f->len / 2;
  BMLoop *(*loops_split)[2] = BLI_array_alloca(loops_split, pair_split_max);
  STACK_DECLARE(loops_split);
  BMVert *(*verts_pair)[2] = BLI_array_alloca(verts_pair, pair_split_max);
  STACK_DECLARE(verts_pair);

  BMLoop *l_tag_prev = NULL, *l_tag_first = NULL;
  BMLoop *l_iter, *l_first;
  uint i;
  int result = 1;

  STACK_INIT(loops_split, pair_split_max);
  STACK_INIT(verts_pair, pair_split_max);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (BMO_vert_flag_test(bm, l_iter->v, VERT_INPUT) &&
        /* ensure this vertex isnt part of a contiguous group */
        ((BMO_vert_flag_test(bm, l_iter->prev->v, VERT_INPUT) == 0) ||
         (BMO_vert_flag_test(bm, l_iter->next->v, VERT_INPUT) == 0))) {
      if (!l_tag_prev) {
        l_tag_prev = l_tag_first = l_iter;
        continue;
      }

      if (!BM_loop_is_adjacent(l_tag_prev, l_iter)) {
        BMEdge *e;
        e = BM_edge_exists(l_tag_prev->v, l_iter->v);
        if (e == NULL || !BMO_edge_flag_test(bm, e, EDGE_OUT)) {
          BMLoop **l_pair = STACK_PUSH_RET(loops_split);
          l_pair[0] = l_tag_prev;
          l_pair[1] = l_iter;
        }
      }

      l_tag_prev = l_iter;
    }
  } while ((l_iter = l_iter->next) != l_first);

  if (STACK_SIZE(loops_split) == 0) {
    return 0;
  }

  if (!BM_loop_is_adjacent(l_tag_first, l_tag_prev) &&
      /* ensure we don't add the same pair twice */
      (((loops_split[0][0] == l_tag_first) && (loops_split[0][1] == l_tag_prev)) == 0)) {
    BMLoop **l_pair = STACK_PUSH_RET(loops_split);
    l_pair[0] = l_tag_first;
    l_pair[1] = l_tag_prev;
  }

  if (check_degenerate) {
    BM_face_splits_check_legal(bm, f, loops_split, STACK_SIZE(loops_split));
  }
  else {
    BM_face_splits_check_optimal(f, loops_split, STACK_SIZE(loops_split));
  }

  for (i = 0; i < STACK_SIZE(loops_split); i++) {
    BMVert **v_pair;
    if (loops_split[i][0] == NULL) {
      continue;
    }

    v_pair = STACK_PUSH_RET(verts_pair);
    v_pair[0] = loops_split[i][0]->v;
    v_pair[1] = loops_split[i][1]->v;
  }

  /* Clear and re-use to store duplicate faces, to remove after splitting is finished. */
  STACK_CLEAR(loops_split);

  for (i = 0; i < STACK_SIZE(verts_pair); i++) {
    BMFace *f_new;
    BMLoop *l_new;
    BMLoop *l_pair[2];

    /* Note that duplicate edges in this case is very unlikely but it can happen, see T70287. */
    bool edge_exists = (BM_edge_exists(verts_pair[i][0], verts_pair[i][1]) != NULL);
    if ((l_pair[0] = BM_face_vert_share_loop(f, verts_pair[i][0])) &&
        (l_pair[1] = BM_face_vert_share_loop(f, verts_pair[i][1]))) {
      f_new = BM_face_split(bm, f, l_pair[0], l_pair[1], &l_new, NULL, edge_exists);

      /* Check if duplicate faces have been created, store the loops for removal in this case.
       * Note that this matches how triangulate works (newly created duplicates get removed). */
      if (UNLIKELY(edge_exists)) {
        BMLoop **l_pair_deferred_remove = NULL;
        for (int j = 0; j < 2; j++) {
          if (BM_face_find_double(l_pair[j]->f)) {
            if (l_pair_deferred_remove == NULL) {
              l_pair_deferred_remove = STACK_PUSH_RET(loops_split);
              l_pair_deferred_remove[0] = NULL;
              l_pair_deferred_remove[1] = NULL;
            }
            l_pair_deferred_remove[j] = l_pair[j];
          }
        }
      }
    }
    else {
      f_new = NULL;
      l_new = NULL;
    }

    if (!l_new || !f_new) {
      result = -1;
      break;
    }

    f = f_new;
    // BMO_face_flag_enable(bm, f_new, FACE_NEW);
    BMO_edge_flag_enable(bm, l_new->e, EDGE_OUT);
  }

  for (i = 0; i < STACK_SIZE(loops_split); i++) {
    for (int j = 0; j < 2; j++) {
      if (loops_split[i][j] != NULL) {
        BM_face_kill(bm, loops_split[i][j]->f);
      }
    }
  }

  return result;
}

void bmo_connect_verts_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMVert *v;
  BMFace *f;
  const bool check_degenerate = BMO_slot_bool_get(op->slots_in, "check_degenerate");
  BLI_LINKSTACK_DECLARE(faces, BMFace *);

  BLI_LINKSTACK_INIT(faces);

  /* tag so we won't touch ever (typically hidden faces) */
  BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces_exclude", BM_FACE, FACE_EXCLUDE);

  /* add all faces connected to verts */
  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    BMIter iter;
    BMLoop *l_iter;

    BMO_vert_flag_enable(bm, v, VERT_INPUT);
    BM_ITER_ELEM (l_iter, &iter, v, BM_LOOPS_OF_VERT) {
      f = l_iter->f;
      if (!BMO_face_flag_test(bm, f, FACE_EXCLUDE)) {
        if (!BMO_face_flag_test(bm, f, FACE_TAG)) {
          BMO_face_flag_enable(bm, f, FACE_TAG);
          if (f->len > 3) {
            BLI_LINKSTACK_PUSH(faces, f);
          }
        }
      }

      /* flag edges even if these are not newly created
       * this way cut-pairs that include co-linear edges will get
       * predictable output. */
      if (BMO_vert_flag_test(bm, l_iter->prev->v, VERT_INPUT)) {
        BMO_edge_flag_enable(bm, l_iter->prev->e, EDGE_OUT_ADJ);
      }
      if (BMO_vert_flag_test(bm, l_iter->next->v, VERT_INPUT)) {
        BMO_edge_flag_enable(bm, l_iter->e, EDGE_OUT_ADJ);
      }
    }
  }

  /* connect faces */
  while ((f = BLI_LINKSTACK_POP(faces))) {
    if (bm_face_connect_verts(bm, f, check_degenerate) == -1) {
      BMO_error_raise(bm, op, BMERR_CONNECTVERT_FAILED, NULL);
    }
  }

  BLI_LINKSTACK_FREE(faces);

  BMO_slot_buffer_from_enabled_flag(
      bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT | EDGE_OUT_ADJ);
}
