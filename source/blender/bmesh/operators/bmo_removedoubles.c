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
 * Welding and merging functionality.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_alloca.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_utildefines_stack.h"
#include "BLI_stack.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"

static void remdoubles_splitface(BMFace *f, BMesh *bm, BMOperator *op, BMOpSlot *slot_targetmap)
{
  BMIter liter;
  BMLoop *l, *l_tar, *l_double;
  bool split = false;

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    BMVert *v_tar = BMO_slot_map_elem_get(slot_targetmap, l->v);
    /* ok: if v_tar is NULL (e.g. not in the map) then it's
     *     a target vert, otherwise it's a double */
    if (v_tar) {
      l_tar = BM_face_vert_share_loop(f, v_tar);

      if (l_tar && (l_tar != l) && !BM_loop_is_adjacent(l_tar, l)) {
        l_double = l;
        split = true;
        break;
      }
    }
  }

  if (split) {
    BMLoop *l_new;
    BMFace *f_new;

    f_new = BM_face_split(bm, f, l_double, l_tar, &l_new, NULL, false);

    remdoubles_splitface(f, bm, op, slot_targetmap);
    remdoubles_splitface(f_new, bm, op, slot_targetmap);
  }
}

#define ELE_DEL 1
#define EDGE_COL 2
#define VERT_IN_FACE 4

/**
 * helper function for bmo_weld_verts_exec so we can use stack memory
 */
static BMFace *remdoubles_createface(BMesh *bm,
                                     BMFace *f,
                                     BMOpSlot *slot_targetmap,
                                     bool *r_created)
{
  BMEdge *e_new;

  BMEdge **edges = BLI_array_alloca(edges, f->len); /* new ordered edges */
  BMVert **verts = BLI_array_alloca(verts, f->len); /* new ordered verts */
  BMLoop **loops = BLI_array_alloca(
      loops, f->len); /* original ordered loops to copy attrs into the new face */

  STACK_DECLARE(edges);
  STACK_DECLARE(loops);
  STACK_DECLARE(verts);

  STACK_INIT(edges, f->len);
  STACK_INIT(loops, f->len);
  STACK_INIT(verts, f->len);

  *r_created = false;

  {
#define LOOP_MAP_VERT_INIT(l_init, v_map, is_del) \
  v_map = l_init->v; \
  is_del = BMO_vert_flag_test_bool(bm, v_map, ELE_DEL); \
  if (is_del) { \
    v_map = BMO_slot_map_elem_get(slot_targetmap, v_map); \
  } \
  ((void)0)

    BMLoop *l_first, *l_curr, *l_next;
    BMVert *v_curr;
    bool is_del_v_curr;

    l_curr = l_first = BM_FACE_FIRST_LOOP(f);
    LOOP_MAP_VERT_INIT(l_curr, v_curr, is_del_v_curr);

    do {
      BMVert *v_next;
      bool is_del_v_next;

      l_next = l_curr->next;
      LOOP_MAP_VERT_INIT(l_next, v_next, is_del_v_next);

      /* only search for a new edge if one of the verts is mapped */
      if ((is_del_v_curr || is_del_v_next) == 0) {
        e_new = l_curr->e;
      }
      else if (v_curr == v_next) {
        e_new = NULL; /* skip */
      }
      else {
        e_new = BM_edge_exists(v_curr, v_next);
        BLI_assert(e_new); /* never fails */
      }

      if (e_new) {
        if (UNLIKELY(BMO_vert_flag_test(bm, v_curr, VERT_IN_FACE))) {
          /* we can't make the face, bail out */
          STACK_CLEAR(edges);
          goto finally;
        }
        BMO_vert_flag_enable(bm, v_curr, VERT_IN_FACE);

        STACK_PUSH(edges, e_new);
        STACK_PUSH(loops, l_curr);
        STACK_PUSH(verts, v_curr);
      }

      v_curr = v_next;
      is_del_v_curr = is_del_v_next;
    } while ((l_curr = l_next) != l_first);

#undef LOOP_MAP_VERT_INIT
  }

finally : {
  uint i;
  for (i = 0; i < STACK_SIZE(verts); i++) {
    BMO_vert_flag_disable(bm, verts[i], VERT_IN_FACE);
  }
}

  if (STACK_SIZE(edges) >= 3) {
    BMFace *f_new = BM_face_exists(verts, STACK_SIZE(verts));
    if (f_new) {
      return f_new;
    }
    f_new = BM_face_create(bm, verts, edges, STACK_SIZE(edges), f, BM_CREATE_NOP);
    BLI_assert(f_new != f);

    if (f_new) {
      uint i = 0;
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
      do {
        BM_elem_attrs_copy(bm, bm, loops[i], l_iter);
      } while ((void)i++, (l_iter = l_iter->next) != l_first);

      *r_created = true;
      return f_new;
    }
  }

  return NULL;
}

/**
 * \note with 'targetmap', multiple 'keys' are currently supported,
 * though no callers should be using.
 * (because slot maps currently use GHash without the GHASH_FLAG_ALLOW_DUPES flag set)
 */
void bmo_weld_verts_exec(BMesh *bm, BMOperator *op)
{
  BMIter iter, liter;
  BMVert *v;
  BMEdge *e;
  BMLoop *l;
  BMFace *f;
  BMOpSlot *slot_targetmap = BMO_slot_get(op->slots_in, "targetmap");

  /* Maintain selection history. */
  const bool has_selected = !BLI_listbase_is_empty(&bm->selected);
  const bool use_targetmap_all = has_selected;
  GHash *targetmap_all = NULL;
  if (use_targetmap_all) {
    /* Map deleted to keep elem. */
    targetmap_all = BLI_ghash_ptr_new(__func__);
  }

  /* mark merge verts for deletion */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BMVert *v_dst = BMO_slot_map_elem_get(slot_targetmap, v);
    if (v_dst != NULL) {
      BMO_vert_flag_enable(bm, v, ELE_DEL);

      /* merge the vertex flags, else we get randomly selected/unselected verts */
      BM_elem_flag_merge_ex(v, v_dst, BM_ELEM_HIDDEN);

      if (use_targetmap_all) {
        BLI_assert(v != v_dst);
        BLI_ghash_insert(targetmap_all, v, v_dst);
      }
    }
  }

  /* check if any faces are getting their own corners merged
   * together, split face if so */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    remdoubles_splitface(f, bm, op, slot_targetmap);
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    BMVert *v1, *v2;
    const bool is_del_v1 = BMO_vert_flag_test_bool(bm, (v1 = e->v1), ELE_DEL);
    const bool is_del_v2 = BMO_vert_flag_test_bool(bm, (v2 = e->v2), ELE_DEL);

    if (is_del_v1 || is_del_v2) {
      if (is_del_v1) {
        v1 = BMO_slot_map_elem_get(slot_targetmap, v1);
      }
      if (is_del_v2) {
        v2 = BMO_slot_map_elem_get(slot_targetmap, v2);
      }

      if (v1 == v2) {
        BMO_edge_flag_enable(bm, e, EDGE_COL);
      }
      else {
        /* always merge flags, even for edges we already created */
        BMEdge *e_new = BM_edge_exists(v1, v2);
        if (e_new == NULL) {
          e_new = BM_edge_create(bm, v1, v2, e, BM_CREATE_NOP);
        }
        BM_elem_flag_merge_ex(e_new, e, BM_ELEM_HIDDEN);
        if (use_targetmap_all) {
          BLI_assert(e != e_new);
          BLI_ghash_insert(targetmap_all, e, e_new);
        }
      }

      BMO_edge_flag_enable(bm, e, ELE_DEL);
    }
  }

  /* faces get "modified" by creating new faces here, then at the
   * end the old faces are deleted */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    bool vert_delete = false;
    int edge_collapse = 0;

    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      if (BMO_vert_flag_test(bm, l->v, ELE_DEL)) {
        vert_delete = true;
      }
      if (BMO_edge_flag_test(bm, l->e, EDGE_COL)) {
        edge_collapse++;
      }
    }

    if (vert_delete) {
      bool use_in_place = false;
      BMFace *f_new = NULL;
      BMO_face_flag_enable(bm, f, ELE_DEL);

      if (f->len - edge_collapse >= 3) {
        bool created;
        f_new = remdoubles_createface(bm, f, slot_targetmap, &created);
        /* do this so we don't need to return a list of created faces */
        if (f_new) {
          if (created) {
            bmesh_face_swap_data(f_new, f);

            if (bm->use_toolflags) {
              SWAP(BMFlagLayer *, ((BMFace_OFlag *)f)->oflags, ((BMFace_OFlag *)f_new)->oflags);
            }

            BMO_face_flag_disable(bm, f, ELE_DEL);
            BM_face_kill(bm, f_new);
            use_in_place = true;
          }
          else {
            BM_elem_flag_merge_ex(f_new, f, BM_ELEM_HIDDEN);
          }
        }
      }

      if ((use_in_place == false) && (f_new != NULL)) {
        BLI_assert(f != f_new);
        if (use_targetmap_all) {
          BLI_ghash_insert(targetmap_all, f, f_new);
        }
        if (bm->act_face && (f == bm->act_face)) {
          bm->act_face = f_new;
        }
      }
    }
  }

  if (has_selected) {
    BM_select_history_merge_from_targetmap(bm, targetmap_all, targetmap_all, targetmap_all, true);
  }

  if (use_targetmap_all) {
    BLI_ghash_free(targetmap_all, NULL, NULL);
  }

  BMO_mesh_delete_oflag_context(bm, ELE_DEL, DEL_ONLYTAGGED);
}

#define VERT_KEEP 8
#define VERT_IN 32

#define EDGE_MARK 1

void bmo_pointmerge_facedata_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMIter iter;
  BMVert *v, *vert_snap;
  BMLoop *l, *l_first = NULL;
  float fac;
  int i, tot;

  vert_snap = BMO_slot_buffer_get_single(BMO_slot_get(op->slots_in, "vert_snap"));
  tot = BM_vert_face_count(vert_snap);

  if (!tot) {
    return;
  }

  fac = 1.0f / tot;
  BM_ITER_ELEM (l, &iter, vert_snap, BM_LOOPS_OF_VERT) {
    if (l_first == NULL) {
      l_first = l;
    }

    for (i = 0; i < bm->ldata.totlayer; i++) {
      if (CustomData_layer_has_math(&bm->ldata, i)) {
        const int type = bm->ldata.layers[i].type;
        const int offset = bm->ldata.layers[i].offset;
        void *e1, *e2;

        e1 = BM_ELEM_CD_GET_VOID_P(l_first, offset);
        e2 = BM_ELEM_CD_GET_VOID_P(l, offset);

        CustomData_data_multiply(type, e2, fac);

        if (l != l_first) {
          CustomData_data_add(type, e1, e2);
        }
      }
    }
  }

  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
      if (l == l_first) {
        continue;
      }

      CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, l_first->head.data, &l->head.data);
    }
  }
}

void bmo_average_vert_facedata_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMIter iter;
  BMVert *v;
  BMLoop *l /* , *firstl = NULL */;
  CDBlockBytes min, max;
  int i;

  for (i = 0; i < bm->ldata.totlayer; i++) {
    const int type = bm->ldata.layers[i].type;
    const int offset = bm->ldata.layers[i].offset;

    if (!CustomData_layer_has_math(&bm->ldata, i)) {
      continue;
    }

    CustomData_data_initminmax(type, &min, &max);

    BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        void *block = BM_ELEM_CD_GET_VOID_P(l, offset);
        CustomData_data_dominmax(type, block, &min, &max);
      }
    }

    CustomData_data_multiply(type, &min, 0.5f);
    CustomData_data_multiply(type, &max, 0.5f);
    CustomData_data_add(type, &min, &max);

    BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        void *block = BM_ELEM_CD_GET_VOID_P(l, offset);
        CustomData_data_copy_value(type, &min, block);
      }
    }
  }
}

void bmo_pointmerge_exec(BMesh *bm, BMOperator *op)
{
  BMOperator weldop;
  BMOIter siter;
  BMVert *v, *vert_snap = NULL;
  float vec[3];
  BMOpSlot *slot_targetmap;

  BMO_slot_vec_get(op->slots_in, "merge_co", vec);

  // BMO_op_callf(bm, op->flag, "collapse_uvs edges=%s", op, "edges");
  BMO_op_init(bm, &weldop, op->flag, "weld_verts");

  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");

  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    if (!vert_snap) {
      vert_snap = v;
      copy_v3_v3(vert_snap->co, vec);
    }
    else {
      BMO_slot_map_elem_insert(&weldop, slot_targetmap, v, vert_snap);
    }
  }

  BMO_op_exec(bm, &weldop);
  BMO_op_finish(bm, &weldop);
}

void bmo_collapse_exec(BMesh *bm, BMOperator *op)
{
  BMOperator weldop;
  BMWalker walker;
  BMIter iter;
  BMEdge *e;
  BLI_Stack *edge_stack;
  BMOpSlot *slot_targetmap;

  if (BMO_slot_bool_get(op->slots_in, "uvs")) {
    BMO_op_callf(bm, op->flag, "collapse_uvs edges=%s", op, "edges");
  }

  BMO_op_init(bm, &weldop, op->flag, "weld_verts");
  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");

  BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

  BMW_init(&walker,
           bm,
           BMW_VERT_SHELL,
           BMW_MASK_NOP,
           EDGE_MARK,
           BMW_MASK_NOP,
           BMW_FLAG_NOP, /* no need to use BMW_FLAG_TEST_HIDDEN, already marked data */
           BMW_NIL_LAY);

  edge_stack = BLI_stack_new(sizeof(BMEdge *), __func__);

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    float center[3];
    int count = 0;
    BMVert *v_tar;

    zero_v3(center);

    if (!BMO_edge_flag_test(bm, e, EDGE_MARK)) {
      continue;
    }

    BLI_assert(BLI_stack_is_empty(edge_stack));

    for (e = BMW_begin(&walker, e->v1); e; e = BMW_step(&walker)) {
      BLI_stack_push(edge_stack, &e);

      add_v3_v3(center, e->v1->co);
      add_v3_v3(center, e->v2->co);

      count += 2;

      /* prevent adding to slot_targetmap multiple times */
      BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
      BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
    }

    if (!BLI_stack_is_empty(edge_stack)) {
      mul_v3_fl(center, 1.0f / count);

      /* snap edges to a point.  for initial testing purposes anyway */
      e = *(BMEdge **)BLI_stack_peek(edge_stack);
      v_tar = e->v1;

      while (!BLI_stack_is_empty(edge_stack)) {
        uint j;
        BLI_stack_pop(edge_stack, &e);

        for (j = 0; j < 2; j++) {
          BMVert *v_src = *((&e->v1) + j);

          copy_v3_v3(v_src->co, center);
          if ((v_src != v_tar) && !BM_elem_flag_test(v_src, BM_ELEM_TAG)) {
            BM_elem_flag_enable(v_src, BM_ELEM_TAG);
            BMO_slot_map_elem_insert(&weldop, slot_targetmap, v_src, v_tar);
          }
        }
      }
    }
  }

  BLI_stack_free(edge_stack);

  BMO_op_exec(bm, &weldop);
  BMO_op_finish(bm, &weldop);

  BMW_end(&walker);
}

/* uv collapse function */
static void bmo_collapsecon_do_layer(BMesh *bm, const int layer, const short oflag)
{
  const int type = bm->ldata.layers[layer].type;
  const int offset = bm->ldata.layers[layer].offset;
  BMIter iter, liter;
  BMFace *f;
  BMLoop *l, *l2;
  BMWalker walker;
  BLI_Stack *block_stack;
  CDBlockBytes min, max;

  BMW_init(&walker,
           bm,
           BMW_LOOPDATA_ISLAND,
           BMW_MASK_NOP,
           oflag,
           BMW_MASK_NOP,
           BMW_FLAG_NOP, /* no need to use BMW_FLAG_TEST_HIDDEN, already marked data */
           layer);

  block_stack = BLI_stack_new(sizeof(void *), __func__);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      if (BMO_edge_flag_test(bm, l->e, oflag)) {
        /* walk */
        BLI_assert(BLI_stack_is_empty(block_stack));

        CustomData_data_initminmax(type, &min, &max);
        for (l2 = BMW_begin(&walker, l); l2; l2 = BMW_step(&walker)) {
          void *block = BM_ELEM_CD_GET_VOID_P(l2, offset);
          CustomData_data_dominmax(type, block, &min, &max);
          BLI_stack_push(block_stack, &block);
        }

        if (!BLI_stack_is_empty(block_stack)) {
          CustomData_data_multiply(type, &min, 0.5f);
          CustomData_data_multiply(type, &max, 0.5f);
          CustomData_data_add(type, &min, &max);

          /* snap CD (uv, vcol) points to their centroid */
          while (!BLI_stack_is_empty(block_stack)) {
            void *block;
            BLI_stack_pop(block_stack, &block);
            CustomData_data_copy_value(type, &min, block);
          }
        }
      }
    }
  }

  BLI_stack_free(block_stack);

  BMW_end(&walker);
}

void bmo_collapse_uvs_exec(BMesh *bm, BMOperator *op)
{
  const short oflag = EDGE_MARK;
  int i;

  /* check flags dont change once set */
#ifndef NDEBUG
  int tot_test;
#endif

  if (!CustomData_has_math(&bm->ldata)) {
    return;
  }

  BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, oflag);

#ifndef NDEBUG
  tot_test = BM_iter_mesh_count_flag(BM_EDGES_OF_MESH, bm, oflag, true);
#endif

  for (i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_layer_has_math(&bm->ldata, i)) {
      bmo_collapsecon_do_layer(bm, i, oflag);
    }
  }

#ifndef NDEBUG
  BLI_assert(tot_test == BM_iter_mesh_count_flag(BM_EDGES_OF_MESH, bm, EDGE_MARK, true));
#endif
}

static void bmesh_find_doubles_common(BMesh *bm,
                                      BMOperator *op,
                                      BMOperator *optarget,
                                      BMOpSlot *optarget_slot)
{
  const BMOpSlot *slot_verts = BMO_slot_get(op->slots_in, "verts");
  BMVert *const *verts = (BMVert **)slot_verts->data.buf;
  const int verts_len = slot_verts->len;

  bool has_keep_vert = false;
  bool found_duplicates = false;

  const float dist = BMO_slot_float_get(op->slots_in, "dist");

  /* Test whether keep_verts arg exists and is non-empty */
  if (BMO_slot_exists(op->slots_in, "keep_verts")) {
    BMOIter oiter;
    has_keep_vert = BMO_iter_new(&oiter, op->slots_in, "keep_verts", BM_VERT) != NULL;
  }

  /* Flag keep_verts */
  if (has_keep_vert) {
    BMO_slot_buffer_flag_enable(bm, op->slots_in, "keep_verts", BM_VERT, VERT_KEEP);
  }

  int *duplicates = MEM_mallocN(sizeof(int) * verts_len, __func__);
  {
    KDTree_3d *tree = BLI_kdtree_3d_new(verts_len);
    for (int i = 0; i < verts_len; i++) {
      BLI_kdtree_3d_insert(tree, i, verts[i]->co);
      if (has_keep_vert && BMO_vert_flag_test(bm, verts[i], VERT_KEEP)) {
        duplicates[i] = i;
      }
      else {
        duplicates[i] = -1;
      }
    }

    BLI_kdtree_3d_balance(tree);
    found_duplicates = BLI_kdtree_3d_calc_duplicates_fast(tree, dist, false, duplicates) != 0;
    BLI_kdtree_3d_free(tree);
  }

  if (found_duplicates) {
    for (int i = 0; i < verts_len; i++) {
      BMVert *v_check = verts[i];
      if (duplicates[i] == -1) {
        /* nop (others can use as target) */
      }
      else if (duplicates[i] == i) {
        /* keep (others can use as target) */
      }
      else {
        BMVert *v_other = verts[duplicates[i]];
        BLI_assert(ELEM(duplicates[duplicates[i]], -1, duplicates[i]));
        BMO_slot_map_elem_insert(optarget, optarget_slot, v_check, v_other);
      }
    }
  }

  MEM_freeN(duplicates);
}

void bmo_remove_doubles_exec(BMesh *bm, BMOperator *op)
{
  BMOperator weldop;
  BMOpSlot *slot_targetmap;

  BMO_op_init(bm, &weldop, op->flag, "weld_verts");
  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");
  bmesh_find_doubles_common(bm, op, &weldop, slot_targetmap);
  BMO_op_exec(bm, &weldop);
  BMO_op_finish(bm, &weldop);
}

void bmo_find_doubles_exec(BMesh *bm, BMOperator *op)
{
  BMOpSlot *slot_targetmap_out;
  slot_targetmap_out = BMO_slot_get(op->slots_out, "targetmap.out");
  bmesh_find_doubles_common(bm, op, op, slot_targetmap_out);
}

void bmo_automerge_exec(BMesh *bm, BMOperator *op)
{
  BMOperator findop, weldop;
  BMIter viter;
  BMVert *v;

  /* The "verts" input sent to this op is the set of verts that
   * can be merged away into any other verts. Mark all other verts
   * as VERT_KEEP. */
  BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts", BM_VERT, VERT_IN);
  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (!BMO_vert_flag_test(bm, v, VERT_IN)) {
      BMO_vert_flag_enable(bm, v, VERT_KEEP);
    }
  }

  /* Search for doubles among all vertices, but only merge non-VERT_KEEP
   * vertices into VERT_KEEP vertices. */
  BMO_op_initf(bm, &findop, op->flag, "find_doubles verts=%av keep_verts=%fv", VERT_KEEP);
  BMO_slot_copy(op, slots_in, "dist", &findop, slots_in, "dist");
  BMO_op_exec(bm, &findop);

  /* weld the vertices */
  BMO_op_init(bm, &weldop, op->flag, "weld_verts");
  BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");
  BMO_op_exec(bm, &weldop);

  BMO_op_finish(bm, &findop);
  BMO_op_finish(bm, &weldop);
}
