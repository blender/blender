/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Simple edge offset functionality.
 *
 * \note Actual offset is done by edge-slide.
 * (this only changes topology)
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines_stack.h"

#include "BKE_customdata.hh"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

#define USE_CAP_OPTION

#define ELE_NEW (1 << 0)

#ifdef USE_CAP_OPTION
#  define ELE_VERT_ENDPOINT (1 << 1)
#endif

/* set for debugging */
#define OFFSET 0.0f

static BMFace *bm_face_split_walk_back(BMesh *bm, BMLoop *l_src, BMLoop **r_l)
{
  float (*cos)[3];
  BMLoop *l_dst;
  BMFace *f;
  int num, i;

  for (l_dst = l_src->prev, num = 0; BM_elem_index_get(l_dst->prev->v) != -1;
       l_dst = l_dst->prev, num++)
  {
    /* pass */
  }

  BLI_assert(num != 0);

  cos = BLI_array_alloca(cos, num);

  for (l_dst = l_src->prev, i = 0; BM_elem_index_get(l_dst->prev->v) != -1;
       l_dst = l_dst->prev, i++)
  {
    copy_v3_v3(cos[num - (i + 1)], l_dst->v->co);
  }

  f = BM_face_split_n(bm, l_src->f, l_dst->prev, l_src->next, cos, num, r_l, nullptr);

  return f;
}

void bmo_offset_edgeloops_exec(BMesh *bm, BMOperator *op)
{
  const int edges_num = BMO_slot_buffer_len(op->slots_in, "edges");
  BMVert **verts;
  STACK_DECLARE(verts);
  int i;

#ifdef USE_CAP_OPTION
  bool use_cap_endpoint = BMO_slot_bool_get(op->slots_in, "use_cap_endpoint");
  int v_edges_max = 0;
#endif

  BMOIter oiter;

  /* only so we can detect new verts (index == -1) */
  BM_mesh_elem_index_ensure(bm, BM_VERT);

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  /* over alloc */
  verts = MEM_malloc_arrayN<BMVert *>((edges_num * 2), __func__);

  STACK_INIT(verts, (edges_num * 2));

  {
    BMEdge *e;
    BMO_ITER (e, &oiter, op->slots_in, "edges", BM_EDGE) {
      int j;

      BM_elem_flag_enable(e, BM_ELEM_TAG);

      for (j = 0; j < 2; j++) {
        BMVert *v_edge = *(&(e->v1) + j);
        if (!BM_elem_flag_test(v_edge, BM_ELEM_TAG)) {
          BM_elem_flag_enable(v_edge, BM_ELEM_TAG);
          STACK_PUSH(verts, v_edge);
        }
      }
    }
  }

  /* -------------------------------------------------------------------- */
  /* Remove verts only used by tagged edges */

  for (i = 0; i < STACK_SIZE(verts); i++) {
    BMIter iter;
    int flag = 0;
    BMEdge *e;

    BM_ITER_ELEM (e, &iter, verts[i], BM_EDGES_OF_VERT) {
      flag |= BM_elem_flag_test(e, BM_ELEM_TAG) ? 1 : 2;
      if (flag == (1 | 2)) {
        break;
      }
    }

    /* only boundary verts are interesting */
    if (flag != (1 | 2)) {
      STACK_REMOVE(verts, i);
    }
  }

  /* possible but unlikely we have no mixed vertices */
  if (UNLIKELY(STACK_SIZE(verts) == 0)) {
    MEM_freeN(verts);
    return;
  }

  /* main loop */
  for (i = 0; i < STACK_SIZE(verts); i++) {
    int v_edges_num = 0;
    int v_edges_num_untag = 0;
    BMVert *v = verts[i];
    BMIter iter;
    BMEdge *e;

    BM_ITER_ELEM (e, &iter, verts[i], BM_EDGES_OF_VERT) {
      if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
        BMVert *v_other;
        BMIter liter;
        BMLoop *l;

        BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
          BM_elem_flag_enable(l->f, BM_ELEM_TAG);
        }

        v_other = BM_edge_other_vert(e, v);
        BM_edge_split(bm, e, v_other, nullptr, 1.0f - OFFSET);
      }
      else {
        v_edges_num_untag += 1;
      }

      v_edges_num += 1;
    }

#ifdef USE_CAP_OPTION
    if (v_edges_num_untag == 1) {
      BMO_vert_flag_enable(bm, v, ELE_VERT_ENDPOINT);
    }

    CLAMP_MIN(v_edges_max, v_edges_num);
#endif
  }

  for (i = 0; i < STACK_SIZE(verts); i++) {
    BMVert *v = verts[i];
    BMIter liter;
    BMLoop *l;

    BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
      if (BM_elem_flag_test(l->f, BM_ELEM_TAG) && (l->f->len != 3)) {
        BMFace *f_cmp = l->f;
        if ((BM_elem_index_get(l->next->v) == -1) && (BM_elem_index_get(l->prev->v) == -1)) {
#ifdef USE_CAP_OPTION
          if (use_cap_endpoint || (BMO_vert_flag_test(bm, v, ELE_VERT_ENDPOINT) == 0))
#endif
          {
            BMLoop *l_new;
            BM_face_split(bm, l->f, l->prev, l->next, &l_new, nullptr, true);
            BLI_assert(f_cmp == l->f);
            BLI_assert(f_cmp != l_new->f);
            UNUSED_VARS_NDEBUG(f_cmp);
            BMO_edge_flag_enable(bm, l_new->e, ELE_NEW);
          }
        }
        else if (l->f->len > 4) {
          if (BM_elem_flag_test(l->e, BM_ELEM_TAG) != BM_elem_flag_test(l->prev->e, BM_ELEM_TAG)) {
            if (BM_elem_index_get(l->next->v) == -1) {
              if (BM_elem_index_get(l->prev->prev->v) == -1) {
                BMLoop *l_new;
                BM_face_split(bm, l->f, l->prev->prev, l->next, &l_new, nullptr, true);
                BLI_assert(f_cmp == l->f);
                BLI_assert(f_cmp != l_new->f);
                BMO_edge_flag_enable(bm, l_new->e, ELE_NEW);
                BM_elem_flag_disable(l->f, BM_ELEM_TAG);
              }
              else {
                /* walk backwards */
                BMLoop *l_new;
                bm_face_split_walk_back(bm, l, &l_new);
                do {
                  BMO_edge_flag_enable(bm, l_new->e, ELE_NEW);
                  l_new = l_new->next;
                } while (BM_vert_is_edge_pair(l_new->v));
                BM_elem_flag_disable(l->f, BM_ELEM_TAG);
              }
            }

/* NOTE: instead of duplicate code in alternate direction,
 * we can be sure to hit the other vertex, so the code above runs. */
#if 0
            else if (BM_elem_index_get(l->prev->v) == -1) {
              if (BM_elem_index_get(l->next->next->v) == -1) {
                /* pass */
              }
            }
#endif
          }
        }
      }
    }
  }

#ifdef USE_CAP_OPTION
  if (use_cap_endpoint == false) {
    BMVert **varr = BLI_array_alloca(varr, v_edges_max);
    STACK_DECLARE(varr);
    BMVert *v;

    for (i = 0; i < STACK_SIZE(verts); i++) {
      BMIter iter;
      BMEdge *e;

      v = verts[i];

      STACK_INIT(varr, v_edges_max);

      BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
        BMVert *v_other;
        v_other = BM_edge_other_vert(e, v);
        if (BM_elem_index_get(v_other) == -1) {
          if (BM_vert_is_edge_pair(v_other)) {
            /* defer bmesh_kernel_join_edge_kill_vert to avoid looping over data we're removing */
            v_other->e = e;
            STACK_PUSH(varr, v_other);
          }
        }
      }

      while ((v = STACK_POP(varr))) {
        bmesh_kernel_join_edge_kill_vert(bm, v->e, v, true, false, false, true);
      }
    }
  }
#endif

  MEM_freeN(verts);

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, ELE_NEW);
}
