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

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "ED_mesh.hh"

#include "tools/bmesh_intersect_edges.hh"

namespace blender {

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

static bool edbm_automerge_impl(Object *obedit,
                                bool update,
                                const char hflag,
                                const float dist,
                                const bool use_connected,
                                const bool use_centroid)
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
               "find_doubles verts=%av keep_verts=%Hv dist=%f use_connected=%b",
               hflag,
               dist,
               use_connected);

  BMO_op_exec(bm, &findop);

  /* weld the vertices */
  BMO_op_initf(bm, &weldop, BMO_FLAG_DEFAULTS, "weld_verts use_centroid=%b", use_centroid);
  BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");
  BMO_op_exec(bm, &weldop);

  BMO_op_finish(bm, &findop);
  BMO_op_finish(bm, &weldop);

  bool changed = totvert_prev != bm->totvert;
  if (changed && update) {
    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = false;
    params.is_destructive = true;
    EDBM_update(id_cast<Mesh *>(obedit->data), &params);
  }
  return changed;
}

bool EDBM_automerge(
    Object *obedit, bool update, const char hflag, const float dist, const bool use_centroid)
{
  return edbm_automerge_impl(obedit, update, hflag, dist, false, use_centroid);
}

bool EDBM_automerge_connected(Object *obedit, bool update, const char hflag, const float dist)
{
  return edbm_automerge_impl(obedit, update, hflag, dist, true, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Merge & Split Selection
 *
 * Used after transform operations.
 * \{ */

bool EDBM_automerge_and_split(Object *obedit,
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
    EDBM_update(id_cast<Mesh *>(obedit->data), &params);
  }

  return ok;
}

/** \} */

}  // namespace blender
