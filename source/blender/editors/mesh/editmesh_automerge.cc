/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Utility functions for merging geometry once transform has finished:
 *
 * - #EDBM_automerge
 * - #EDBM_automerge_and_split
 */

#include "BKE_editmesh.hh"

#include "DNA_object_types.h"

#include "ED_mesh.hh"

#include "tools/bmesh_intersect_edges.hh"

// #define DEBUG_TIME
#ifdef DEBUG_TIME
#  include "BLI_time.h"
#endif

/* use bmesh operator flags for a few operators */
#define BMO_ELE_TAG 1

/* -------------------------------------------------------------------- */
/** \name Auto-Merge Selection
 *
 * Used after transform operations.
 * \{ */

void EDBM_automerge(Object *obedit, bool update, const char hflag, const float dist)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  int totvert_prev = bm->totvert;

  BMOperator findop, weldop;

  /* Search for doubles among all vertices, but only merge non-VERT_KEEP
   * vertices into VERT_KEEP vertices. */
  BMO_op_initf(bm,
               &findop,
               BMO_FLAG_DEFAULTS,
               "find_doubles verts=%av keep_verts=%Hv dist=%f",
               hflag,
               dist);

  BMO_op_exec(bm, &findop);

  /* weld the vertices */
  BMO_op_init(bm, &weldop, BMO_FLAG_DEFAULTS, "weld_verts");
  BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");
  BMO_op_exec(bm, &weldop);

  BMO_op_finish(bm, &findop);
  BMO_op_finish(bm, &weldop);

  EDBMUpdate_Params params{};
  params.calc_looptris = true;
  params.calc_normals = false;
  params.is_destructive = true;
  if ((totvert_prev != bm->totvert) && update) {
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Merge & Split Selection
 *
 * Used after transform operations.
 * \{ */

void EDBM_automerge_and_split(Object *obedit,
                              const bool /*split_edges*/,
                              const bool split_faces,
                              const bool update,
                              const char hflag,
                              const float dist)
{
  bool ok = false;

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

#ifdef DEBUG_TIME
  em->bm = BM_mesh_copy(bm);

  double t1 = BLI_time_now_seconds();
  EDBM_automerge(obedit, false, hflag, dist);
  t1 = BLI_time_now_seconds() - t1;

  BM_mesh_free(em->bm);
  em->bm = bm;
  double t2 = BLI_time_now_seconds();
#endif

  BMOperator weldop;
  BMOpSlot *slot_targetmap;

  BMO_op_init(bm, &weldop, BMO_FLAG_DEFAULTS, "weld_verts");
  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");

  GHash *ghash_targetmap = BMO_SLOT_AS_GHASH(slot_targetmap);

  ok = BM_mesh_intersect_edges(bm, hflag, dist, split_faces, ghash_targetmap);

  if (ok) {
    BMO_op_exec(bm, &weldop);
  }

  BMO_op_finish(bm, &weldop);

#ifdef DEBUG_TIME
  t2 = BLI_time_now_seconds() - t2;
  printf("t1: %lf; t2: %lf; fac: %lf\n", t1, t2, t1 / t2);
#endif

  if (LIKELY(ok) && update) {
    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = false;
    params.is_destructive = true;
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);
  }
}

/** \} */
