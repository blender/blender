/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Triangulate.
 */

#include "DNA_modifier_types.h" /* for MOD_TRIANGULATE_NGON_BEAUTY only */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_heap.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

/* only for defines */
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"

#include "bmesh.h"

#include "bmesh_triangulate.h" /* own include */

/**
 * a version of #BM_face_triangulate that maps to #BMOpSlot
 */
static void bm_face_triangulate_mapping(BMesh *bm,
                                        BMFace *face,
                                        const int quad_method,
                                        const int ngon_method,
                                        const bool use_tag,
                                        BMOperator *op,
                                        BMOpSlot *slot_facemap_out,
                                        BMOpSlot *slot_facemap_double_out,

                                        MemArena *pf_arena,
                                        /* use for MOD_TRIANGULATE_NGON_BEAUTY only! */
                                        struct Heap *pf_heap)
{
  int faces_array_tot = face->len - 3;
  BMFace **faces_array = BLI_array_alloca(faces_array, faces_array_tot);
  LinkNode *faces_double = NULL;
  BLI_assert(face->len > 3);

  BM_face_triangulate(bm,
                      face,
                      faces_array,
                      &faces_array_tot,
                      NULL,
                      NULL,
                      &faces_double,
                      quad_method,
                      ngon_method,
                      use_tag,
                      pf_arena,
                      pf_heap);

  if (faces_array_tot) {
    int i;
    BMO_slot_map_elem_insert(op, slot_facemap_out, face, face);
    for (i = 0; i < faces_array_tot; i++) {
      BMO_slot_map_elem_insert(op, slot_facemap_out, faces_array[i], face);
    }

    while (faces_double) {
      LinkNode *next = faces_double->next;
      BMO_slot_map_elem_insert(op, slot_facemap_double_out, faces_double->link, face);
      MEM_freeN(faces_double);
      faces_double = next;
    }
  }
}

void BM_mesh_triangulate(BMesh *bm,
                         const int quad_method,
                         const int ngon_method,
                         const int min_vertices,
                         const bool tag_only,
                         BMOperator *op,
                         BMOpSlot *slot_facemap_out,
                         BMOpSlot *slot_facemap_double_out)
{
  BMIter iter;
  BMFace *face;
  MemArena *pf_arena;
  Heap *pf_heap;

  pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);

  if (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY) {
    pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
  }
  else {
    pf_heap = NULL;
  }

  if (slot_facemap_out) {
    /* same as below but call: bm_face_triangulate_mapping() */
    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (face->len >= min_vertices) {
        if (tag_only == false || BM_elem_flag_test(face, BM_ELEM_TAG)) {
          bm_face_triangulate_mapping(bm,
                                      face,
                                      quad_method,
                                      ngon_method,
                                      tag_only,
                                      op,
                                      slot_facemap_out,
                                      slot_facemap_double_out,
                                      pf_arena,
                                      pf_heap);
        }
      }
    }
  }
  else {
    LinkNode *faces_double = NULL;

    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (face->len >= min_vertices) {
        if (tag_only == false || BM_elem_flag_test(face, BM_ELEM_TAG)) {
          BM_face_triangulate(bm,
                              face,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              &faces_double,
                              quad_method,
                              ngon_method,
                              tag_only,
                              pf_arena,
                              pf_heap);
        }
      }
    }

    while (faces_double) {
      LinkNode *next = faces_double->next;
      BM_face_kill(bm, faces_double->link);
      MEM_freeN(faces_double);
      faces_double = next;
    }
  }

  BLI_memarena_free(pf_arena);

  if (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY) {
    BLI_heap_free(pf_heap, NULL);
  }
}
