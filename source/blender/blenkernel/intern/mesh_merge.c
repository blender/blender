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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */
#include <string.h>  // for memcpy

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"
#include "BLI_edgehash.h"
#include "BLI_ghash.h"

#include "BKE_customdata.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"

/**
 * Poly compare with vtargetmap
 * Function used by #BKE_mesh_merge_verts.
 * The function compares poly_source after applying vtargetmap, with poly_target.
 * The two polys are identical if they share the same vertices in the same order,
 * or in reverse order, but starting position loopstart may be different.
 * The function is called with direct_reverse=1 for same order (i.e. same normal),
 * and may be called again with direct_reverse=-1 for reverse order.
 * \return 1 if polys are identical,  0 if polys are different.
 */
static int cddm_poly_compare(MLoop *mloop_array,
                             MPoly *mpoly_source,
                             MPoly *mpoly_target,
                             const int *vtargetmap,
                             const int direct_reverse)
{
  int vert_source, first_vert_source, vert_target;
  int i_loop_source;
  int i_loop_target, i_loop_target_start, i_loop_target_offset, i_loop_target_adjusted;
  bool compare_completed = false;
  bool same_loops = false;

  MLoop *mloop_source, *mloop_target;

  BLI_assert(direct_reverse == 1 || direct_reverse == -1);

  i_loop_source = 0;
  mloop_source = mloop_array + mpoly_source->loopstart;
  vert_source = mloop_source->v;

  if (vtargetmap[vert_source] != -1) {
    vert_source = vtargetmap[vert_source];
  }
  else {
    /* All source loop vertices should be mapped */
    BLI_assert(false);
  }

  /* Find same vertex within mpoly_target's loops */
  mloop_target = mloop_array + mpoly_target->loopstart;
  for (i_loop_target = 0; i_loop_target < mpoly_target->totloop; i_loop_target++, mloop_target++) {
    if (mloop_target->v == vert_source) {
      break;
    }
  }

  /* If same vertex not found, then polys cannot be equal */
  if (i_loop_target >= mpoly_target->totloop) {
    return false;
  }

  /* Now mloop_source and m_loop_target have one identical vertex */
  /* mloop_source is at position 0, while m_loop_target has advanced to find identical vertex */
  /* Go around the loop and check that all vertices match in same order */
  /* Skipping source loops when consecutive source vertices are mapped to same target vertex */

  i_loop_target_start = i_loop_target;
  i_loop_target_offset = 0;
  first_vert_source = vert_source;

  compare_completed = false;
  same_loops = false;

  while (!compare_completed) {

    vert_target = mloop_target->v;

    /* First advance i_loop_source, until it points to different vertex, after mapping applied */
    do {
      i_loop_source++;

      if (i_loop_source == mpoly_source->totloop) {
        /* End of loops for source, must match end of loop for target.  */
        if (i_loop_target_offset == mpoly_target->totloop - 1) {
          compare_completed = true;
          same_loops = true;
          break; /* Polys are identical */
        }
        else {
          compare_completed = true;
          same_loops = false;
          break; /* Polys are different */
        }
      }

      mloop_source++;
      vert_source = mloop_source->v;

      if (vtargetmap[vert_source] != -1) {
        vert_source = vtargetmap[vert_source];
      }
      else {
        /* All source loop vertices should be mapped */
        BLI_assert(false);
      }

    } while (vert_source == vert_target);

    if (compare_completed) {
      break;
    }

    /* Now advance i_loop_target as well */
    i_loop_target_offset++;

    if (i_loop_target_offset == mpoly_target->totloop) {
      /* End of loops for target only, that means no match */
      /* except if all remaining source vertices are mapped to first target */
      for (; i_loop_source < mpoly_source->totloop; i_loop_source++, mloop_source++) {
        vert_source = vtargetmap[mloop_source->v];
        if (vert_source != first_vert_source) {
          compare_completed = true;
          same_loops = false;
          break;
        }
      }
      if (!compare_completed) {
        same_loops = true;
      }
      break;
    }

    /* Adjust i_loop_target for cycling around and for direct/reverse order
     * defined by delta = +1 or -1 */
    i_loop_target_adjusted = (i_loop_target_start + direct_reverse * i_loop_target_offset) %
                             mpoly_target->totloop;
    if (i_loop_target_adjusted < 0) {
      i_loop_target_adjusted += mpoly_target->totloop;
    }
    mloop_target = mloop_array + mpoly_target->loopstart + i_loop_target_adjusted;
    vert_target = mloop_target->v;

    if (vert_target != vert_source) {
      same_loops = false; /* Polys are different */
      break;
    }
  }
  return same_loops;
}

/* Utility stuff for using GHash with polys, used by vertex merging. */

typedef struct PolyKey {
  int poly_index;        /* index of the MPoly within the derived mesh */
  int totloops;          /* number of loops in the poly */
  unsigned int hash_sum; /* Sum of all vertices indices */
  unsigned int hash_xor; /* Xor of all vertices indices */
} PolyKey;

static unsigned int poly_gset_hash_fn(const void *key)
{
  const PolyKey *pk = key;
  return pk->hash_sum;
}

static bool poly_gset_compare_fn(const void *k1, const void *k2)
{
  const PolyKey *pk1 = k1;
  const PolyKey *pk2 = k2;
  if ((pk1->hash_sum == pk2->hash_sum) && (pk1->hash_xor == pk2->hash_xor) &&
      (pk1->totloops == pk2->totloops)) {
    /* Equality - note that this does not mean equality of polys */
    return false;
  }
  else {
    return true;
  }
}

/**
 * Merge Verts
 *
 * This frees the given mesh and returns a new mesh.
 *
 * \param vtargetmap: The table that maps vertices to target vertices.  a value of -1
 * indicates a vertex is a target, and is to be kept.
 * This array is aligned with 'mesh->totvert'
 * \warning \a vtargetmap must **not** contain any chained mapping (v1 -> v2 -> v3 etc.),
 * this is not supported and will likely generate corrupted geometry.
 *
 * \param tot_vtargetmap: The number of non '-1' values in vtargetmap. (not the size)
 *
 * \param merge_mode: enum with two modes.
 * - #MESH_MERGE_VERTS_DUMP_IF_MAPPED
 * When called by the Mirror Modifier,
 * In this mode it skips any faces that have all vertices merged (to avoid creating pairs
 * of faces sharing the same set of vertices)
 * - #MESH_MERGE_VERTS_DUMP_IF_EQUAL
 * When called by the Array Modifier,
 * In this mode, faces where all vertices are merged are double-checked,
 * to see whether all target vertices actually make up a poly already.
 * Indeed it could be that all of a poly's vertices are merged,
 * but merged to vertices that do not make up a single poly,
 * in which case the original poly should not be dumped.
 * Actually this later behavior could apply to the Mirror Modifier as well,
 * but the additional checks are costly and not necessary in the case of mirror,
 * because each vertex is only merged to its own mirror.
 *
 * \note #BKE_mesh_recalc_tessellation has to run on the returned DM
 * if you want to access tessfaces.
 */
Mesh *BKE_mesh_merge_verts(Mesh *mesh,
                           const int *vtargetmap,
                           const int tot_vtargetmap,
                           const int merge_mode)
{
  /* This was commented out back in 2013, see commit f45d8827bafe6b9eaf9de42f4054e9d84a21955d. */
  // #define USE_LOOPS

  Mesh *result = NULL;

  const int totvert = mesh->totvert;
  const int totedge = mesh->totedge;
  const int totloop = mesh->totloop;
  const int totpoly = mesh->totpoly;

  const int totvert_final = totvert - tot_vtargetmap;

  MVert *mv, *mvert = MEM_malloc_arrayN(totvert_final, sizeof(*mvert), __func__);
  int *oldv = MEM_malloc_arrayN(totvert_final, sizeof(*oldv), __func__);
  int *newv = MEM_malloc_arrayN(totvert, sizeof(*newv), __func__);
  STACK_DECLARE(mvert);
  STACK_DECLARE(oldv);

  /* Note: create (totedge + totloop) elements because partially invalid polys due to merge may
   * require generating new edges, and while in 99% cases we'll still end with less final edges
   * than totedge, cases can be forged that would end requiring more. */
  MEdge *med, *medge = MEM_malloc_arrayN((totedge + totloop), sizeof(*medge), __func__);
  int *olde = MEM_malloc_arrayN((totedge + totloop), sizeof(*olde), __func__);
  int *newe = MEM_malloc_arrayN((totedge + totloop), sizeof(*newe), __func__);
  STACK_DECLARE(medge);
  STACK_DECLARE(olde);

  MLoop *ml, *mloop = MEM_malloc_arrayN(totloop, sizeof(*mloop), __func__);
  int *oldl = MEM_malloc_arrayN(totloop, sizeof(*oldl), __func__);
#ifdef USE_LOOPS
  int *newl = MEM_malloc_arrayN(totloop, sizeof(*newl), __func__);
#endif
  STACK_DECLARE(mloop);
  STACK_DECLARE(oldl);

  MPoly *mp, *mpoly = MEM_malloc_arrayN(totpoly, sizeof(*medge), __func__);
  int *oldp = MEM_malloc_arrayN(totpoly, sizeof(*oldp), __func__);
  STACK_DECLARE(mpoly);
  STACK_DECLARE(oldp);

  EdgeHash *ehash = BLI_edgehash_new_ex(__func__, totedge);

  int i, j, c;

  PolyKey *poly_keys;
  GSet *poly_gset = NULL;
  MeshElemMap *poly_map = NULL;
  int *poly_map_mem = NULL;

  STACK_INIT(oldv, totvert_final);
  STACK_INIT(olde, totedge);
  STACK_INIT(oldl, totloop);
  STACK_INIT(oldp, totpoly);

  STACK_INIT(mvert, totvert_final);
  STACK_INIT(medge, totedge);
  STACK_INIT(mloop, totloop);
  STACK_INIT(mpoly, totpoly);

  /* fill newv with destination vertex indices */
  mv = mesh->mvert;
  c = 0;
  for (i = 0; i < totvert; i++, mv++) {
    if (vtargetmap[i] == -1) {
      STACK_PUSH(oldv, i);
      STACK_PUSH(mvert, *mv);
      newv[i] = c++;
    }
    else {
      /* dummy value */
      newv[i] = 0;
    }
  }

  /* now link target vertices to destination indices */
  for (i = 0; i < totvert; i++) {
    if (vtargetmap[i] != -1) {
      newv[i] = newv[vtargetmap[i]];
    }
  }

  /* Don't remap vertices in cddm->mloop, because we need to know the original
   * indices in order to skip faces with all vertices merged.
   * The "update loop indices..." section further down remaps vertices in mloop.
   */

  /* now go through and fix edges and faces */
  med = mesh->medge;
  c = 0;
  for (i = 0; i < totedge; i++, med++) {
    const unsigned int v1 = (vtargetmap[med->v1] != -1) ? vtargetmap[med->v1] : med->v1;
    const unsigned int v2 = (vtargetmap[med->v2] != -1) ? vtargetmap[med->v2] : med->v2;
    if (LIKELY(v1 != v2)) {
      void **val_p;

      if (BLI_edgehash_ensure_p(ehash, v1, v2, &val_p)) {
        newe[i] = POINTER_AS_INT(*val_p);
      }
      else {
        STACK_PUSH(olde, i);
        STACK_PUSH(medge, *med);
        newe[i] = c;
        *val_p = POINTER_FROM_INT(c);
        c++;
      }
    }
    else {
      newe[i] = -1;
    }
  }

  if (merge_mode == MESH_MERGE_VERTS_DUMP_IF_EQUAL) {
    /* In this mode, we need to determine,  whenever a poly' vertices are all mapped */
    /* if the targets already make up a poly, in which case the new poly is dropped */
    /* This poly equality check is rather complex.
     * We use a BLI_ghash to speed it up with a first level check */
    PolyKey *mpgh;
    poly_keys = MEM_malloc_arrayN(totpoly, sizeof(PolyKey), __func__);
    poly_gset = BLI_gset_new_ex(poly_gset_hash_fn, poly_gset_compare_fn, __func__, totpoly);
    /* Duplicates allowed because our compare function is not pure equality */
    BLI_gset_flag_set(poly_gset, GHASH_FLAG_ALLOW_DUPES);

    mp = mesh->mpoly;
    mpgh = poly_keys;
    for (i = 0; i < totpoly; i++, mp++, mpgh++) {
      mpgh->poly_index = i;
      mpgh->totloops = mp->totloop;
      ml = mesh->mloop + mp->loopstart;
      mpgh->hash_sum = mpgh->hash_xor = 0;
      for (j = 0; j < mp->totloop; j++, ml++) {
        mpgh->hash_sum += ml->v;
        mpgh->hash_xor ^= ml->v;
      }
      BLI_gset_insert(poly_gset, mpgh);
    }

    /* Can we optimise by reusing an old pmap ?  How do we know an old pmap is stale ?  */
    /* When called by MOD_array.c, the cddm has just been created, so it has no valid pmap.   */
    BKE_mesh_vert_poly_map_create(
        &poly_map, &poly_map_mem, mesh->mpoly, mesh->mloop, totvert, totpoly, totloop);
  } /* done preparing for fast poly compare */

  mp = mesh->mpoly;
  mv = mesh->mvert;
  for (i = 0; i < totpoly; i++, mp++) {
    MPoly *mp_new;

    ml = mesh->mloop + mp->loopstart;

    /* check faces with all vertices merged */
    bool all_vertices_merged = true;

    for (j = 0; j < mp->totloop; j++, ml++) {
      if (vtargetmap[ml->v] == -1) {
        all_vertices_merged = false;
        /* This will be used to check for poly using several time the same vert. */
        mv[ml->v].flag &= ~ME_VERT_TMP_TAG;
      }
      else {
        /* This will be used to check for poly using several time the same vert. */
        mv[vtargetmap[ml->v]].flag &= ~ME_VERT_TMP_TAG;
      }
    }

    if (UNLIKELY(all_vertices_merged)) {
      if (merge_mode == MESH_MERGE_VERTS_DUMP_IF_MAPPED) {
        /* In this mode, all vertices merged is enough to dump face */
        continue;
      }
      else if (merge_mode == MESH_MERGE_VERTS_DUMP_IF_EQUAL) {
        /* Additional condition for face dump:  target vertices must make up an identical face */
        /* The test has 2 steps:  (1) first step is fast ghash lookup, but not failproof       */
        /*                        (2) second step is thorough but more costly poly compare     */
        int i_poly, v_target;
        bool found = false;
        PolyKey pkey;

        /* Use poly_gset for fast (although not 100% certain) identification of same poly */
        /* First, make up a poly_summary structure */
        ml = mesh->mloop + mp->loopstart;
        pkey.hash_sum = pkey.hash_xor = 0;
        pkey.totloops = 0;
        for (j = 0; j < mp->totloop; j++, ml++) {
          v_target = vtargetmap[ml->v]; /* Cannot be -1, they are all mapped */
          pkey.hash_sum += v_target;
          pkey.hash_xor ^= v_target;
          pkey.totloops++;
        }
        if (BLI_gset_haskey(poly_gset, &pkey)) {

          /* There might be a poly that matches this one.
           * We could just leave it there and say there is, and do a "continue".
           * ... but we are checking whether there is an exact poly match.
           * It's not so costly in terms of CPU since it's very rare, just a lot of complex code.
           */

          /* Consider current loop again */
          ml = mesh->mloop + mp->loopstart;
          /* Consider the target of the loop's first vert */
          v_target = vtargetmap[ml->v];
          /* Now see if v_target belongs to a poly that shares all vertices with source poly,
           * in same order, or reverse order */

          for (i_poly = 0; i_poly < poly_map[v_target].count; i_poly++) {
            MPoly *target_poly = mesh->mpoly + *(poly_map[v_target].indices + i_poly);

            if (cddm_poly_compare(mesh->mloop, mp, target_poly, vtargetmap, +1) ||
                cddm_poly_compare(mesh->mloop, mp, target_poly, vtargetmap, -1)) {
              found = true;
              break;
            }
          }
          if (found) {
            /* Current poly's vertices are mapped to a poly that is strictly identical */
            /* Current poly is dumped */
            continue;
          }
        }
      }
    }

    /* Here either the poly's vertices were not all merged
     * or they were all merged, but targets do not make up an identical poly,
     * the poly is retained.
     */
    ml = mesh->mloop + mp->loopstart;

    c = 0;
    MLoop *last_valid_ml = NULL;
    MLoop *first_valid_ml = NULL;
    bool need_edge_from_last_valid_ml = false;
    bool need_edge_to_first_valid_ml = false;
    int created_edges = 0;
    for (j = 0; j < mp->totloop; j++, ml++) {
      const uint mlv = (vtargetmap[ml->v] != -1) ? vtargetmap[ml->v] : ml->v;
#ifndef NDEBUG
      {
        MLoop *next_ml = mesh->mloop + mp->loopstart + ((j + 1) % mp->totloop);
        uint next_mlv = (vtargetmap[next_ml->v] != -1) ? vtargetmap[next_ml->v] : next_ml->v;
        med = mesh->medge + ml->e;
        uint v1 = (vtargetmap[med->v1] != -1) ? vtargetmap[med->v1] : med->v1;
        uint v2 = (vtargetmap[med->v2] != -1) ? vtargetmap[med->v2] : med->v2;
        BLI_assert((mlv == v1 && next_mlv == v2) || (mlv == v2 && next_mlv == v1));
      }
#endif
      /* A loop is only valid if its matching edge is,
       * and it's not reusing a vertex already used by this poly. */
      if (LIKELY((newe[ml->e] != -1) && ((mv[mlv].flag & ME_VERT_TMP_TAG) == 0))) {
        mv[mlv].flag |= ME_VERT_TMP_TAG;

        if (UNLIKELY(last_valid_ml != NULL && need_edge_from_last_valid_ml)) {
          /* We need to create a new edge between last valid loop and this one! */
          void **val_p;

          uint v1 = (vtargetmap[last_valid_ml->v] != -1) ? vtargetmap[last_valid_ml->v] :
                                                           last_valid_ml->v;
          uint v2 = mlv;
          BLI_assert(v1 != v2);
          if (BLI_edgehash_ensure_p(ehash, v1, v2, &val_p)) {
            last_valid_ml->e = POINTER_AS_INT(*val_p);
          }
          else {
            const int new_eidx = STACK_SIZE(medge);
            STACK_PUSH(olde, olde[last_valid_ml->e]);
            STACK_PUSH(medge, mesh->medge[last_valid_ml->e]);
            medge[new_eidx].v1 = last_valid_ml->v;
            medge[new_eidx].v2 = ml->v;
            /* DO NOT change newe mapping,
             * could break actual values due to some deleted original edges. */
            *val_p = POINTER_FROM_INT(new_eidx);
            created_edges++;

            last_valid_ml->e = new_eidx;
          }
          need_edge_from_last_valid_ml = false;
        }

#ifdef USE_LOOPS
        newl[j + mp->loopstart] = STACK_SIZE(mloop);
#endif
        STACK_PUSH(oldl, j + mp->loopstart);
        last_valid_ml = STACK_PUSH_RET_PTR(mloop);
        *last_valid_ml = *ml;
        if (first_valid_ml == NULL) {
          first_valid_ml = last_valid_ml;
        }
        c++;

        /* We absolutely HAVE to handle edge index remapping here, otherwise potential newly
         * created edges in that part of code make remapping later totally unreliable. */
        BLI_assert(newe[ml->e] != -1);
        last_valid_ml->e = newe[ml->e];
      }
      else {
        if (last_valid_ml != NULL) {
          need_edge_from_last_valid_ml = true;
        }
        else {
          need_edge_to_first_valid_ml = true;
        }
      }
    }
    if (UNLIKELY(last_valid_ml != NULL && !ELEM(first_valid_ml, NULL, last_valid_ml) &&
                 (need_edge_to_first_valid_ml || need_edge_from_last_valid_ml))) {
      /* We need to create a new edge between last valid loop and first valid one! */
      void **val_p;

      uint v1 = (vtargetmap[last_valid_ml->v] != -1) ? vtargetmap[last_valid_ml->v] :
                                                       last_valid_ml->v;
      uint v2 = (vtargetmap[first_valid_ml->v] != -1) ? vtargetmap[first_valid_ml->v] :
                                                        first_valid_ml->v;
      BLI_assert(v1 != v2);
      if (BLI_edgehash_ensure_p(ehash, v1, v2, &val_p)) {
        last_valid_ml->e = POINTER_AS_INT(*val_p);
      }
      else {
        const int new_eidx = STACK_SIZE(medge);
        STACK_PUSH(olde, olde[last_valid_ml->e]);
        STACK_PUSH(medge, mesh->medge[last_valid_ml->e]);
        medge[new_eidx].v1 = last_valid_ml->v;
        medge[new_eidx].v2 = first_valid_ml->v;
        /* DO NOT change newe mapping,
         * could break actual values due to some deleted original edges. */
        *val_p = POINTER_FROM_INT(new_eidx);
        created_edges++;

        last_valid_ml->e = new_eidx;
      }
      need_edge_to_first_valid_ml = need_edge_from_last_valid_ml = false;
    }

    if (UNLIKELY(c == 0)) {
      BLI_assert(created_edges == 0);
      continue;
    }
    else if (UNLIKELY(c < 3)) {
      STACK_DISCARD(oldl, c);
      STACK_DISCARD(mloop, c);
      if (created_edges > 0) {
        for (j = STACK_SIZE(medge) - created_edges; j < STACK_SIZE(medge); j++) {
          BLI_edgehash_remove(ehash, medge[j].v1, medge[j].v2, NULL);
        }
        STACK_DISCARD(olde, created_edges);
        STACK_DISCARD(medge, created_edges);
      }
      continue;
    }

    mp_new = STACK_PUSH_RET_PTR(mpoly);
    *mp_new = *mp;
    mp_new->totloop = c;
    BLI_assert(mp_new->totloop >= 3);
    mp_new->loopstart = STACK_SIZE(mloop) - c;

    STACK_PUSH(oldp, i);
  } /* end of the loop that tests polys   */

  if (poly_gset) {
    // printf("hash quality %.6f\n", BLI_gset_calc_quality(poly_gset));

    BLI_gset_free(poly_gset, NULL);
    MEM_freeN(poly_keys);
  }

  /*create new cddm*/
  result = BKE_mesh_new_nomain_from_template(
      mesh, STACK_SIZE(mvert), STACK_SIZE(medge), 0, STACK_SIZE(mloop), STACK_SIZE(mpoly));

  /*update edge indices and copy customdata*/
  med = medge;
  for (i = 0; i < result->totedge; i++, med++) {
    BLI_assert(newv[med->v1] != -1);
    med->v1 = newv[med->v1];
    BLI_assert(newv[med->v2] != -1);
    med->v2 = newv[med->v2];

    /* Can happen in case vtargetmap contains some double chains, we do not support that. */
    BLI_assert(med->v1 != med->v2);

    CustomData_copy_data(&mesh->edata, &result->edata, olde[i], i, 1);
  }

  /*update loop indices and copy customdata*/
  ml = mloop;
  for (i = 0; i < result->totloop; i++, ml++) {
    /* Edge remapping has already be done in main loop handling part above. */
    BLI_assert(newv[ml->v] != -1);
    ml->v = newv[ml->v];

    CustomData_copy_data(&mesh->ldata, &result->ldata, oldl[i], i, 1);
  }

  /*copy vertex customdata*/
  mv = mvert;
  for (i = 0; i < result->totvert; i++, mv++) {
    CustomData_copy_data(&mesh->vdata, &result->vdata, oldv[i], i, 1);
  }

  /*copy poly customdata*/
  mp = mpoly;
  for (i = 0; i < result->totpoly; i++, mp++) {
    CustomData_copy_data(&mesh->pdata, &result->pdata, oldp[i], i, 1);
  }

  /*copy over data.  CustomData_add_layer can do this, need to look it up.*/
  memcpy(result->mvert, mvert, sizeof(MVert) * STACK_SIZE(mvert));
  memcpy(result->medge, medge, sizeof(MEdge) * STACK_SIZE(medge));
  memcpy(result->mloop, mloop, sizeof(MLoop) * STACK_SIZE(mloop));
  memcpy(result->mpoly, mpoly, sizeof(MPoly) * STACK_SIZE(mpoly));

  MEM_freeN(mvert);
  MEM_freeN(medge);
  MEM_freeN(mloop);
  MEM_freeN(mpoly);

  MEM_freeN(newv);
  MEM_freeN(newe);
#ifdef USE_LOOPS
  MEM_freeN(newl);
#endif

  MEM_freeN(oldv);
  MEM_freeN(olde);
  MEM_freeN(oldl);
  MEM_freeN(oldp);

  BLI_edgehash_free(ehash, NULL);

  if (poly_map != NULL) {
    MEM_freeN(poly_map);
  }
  if (poly_map_mem != NULL) {
    MEM_freeN(poly_map_mem);
  }

  BKE_id_free(NULL, mesh);

  return result;
}
