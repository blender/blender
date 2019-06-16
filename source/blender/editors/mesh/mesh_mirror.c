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
 * \ingroup edmesh
 *
 * Mirror calculation for edit-mode and object mode.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_editmesh.h"
#include "BLI_kdtree.h"
#include "BKE_mesh.h"

#include "ED_mesh.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Spatial Mirror API
 * \{ */

#define KD_THRESH 0.00002f

static struct {
  void *tree;
} MirrKdStore = {NULL};

/* mode is 's' start, or 'e' end, or 'u' use */
/* if end, ob can be NULL */
int ED_mesh_mirror_spatial_table(
    Object *ob, BMEditMesh *em, Mesh *me_eval, const float co[3], char mode)
{
  if (mode == 'u') { /* use table */
    if (MirrKdStore.tree == NULL) {
      ED_mesh_mirror_spatial_table(ob, em, me_eval, NULL, 's');
    }

    if (MirrKdStore.tree) {
      KDTreeNearest_3d nearest;
      const int i = BLI_kdtree_3d_find_nearest(MirrKdStore.tree, co, &nearest);

      if (i != -1) {
        if (nearest.dist < KD_THRESH) {
          return i;
        }
      }
    }
    return -1;
  }
  else if (mode == 's') { /* start table */
    Mesh *me = ob->data;
    const bool use_em = (!me_eval && em && me->edit_mesh == em);
    const int totvert = use_em ? em->bm->totvert : me_eval ? me_eval->totvert : me->totvert;

    if (MirrKdStore.tree) { /* happens when entering this call without ending it */
      ED_mesh_mirror_spatial_table(ob, em, me_eval, co, 'e');
    }

    MirrKdStore.tree = BLI_kdtree_3d_new(totvert);

    if (use_em) {
      BMVert *eve;
      BMIter iter;
      int i;

      /* this needs to be valid for index lookups later (callers need) */
      BM_mesh_elem_table_ensure(em->bm, BM_VERT);

      BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        BLI_kdtree_3d_insert(MirrKdStore.tree, i, eve->co);
      }
    }
    else {
      MVert *mvert = me_eval ? me_eval->mvert : me->mvert;
      int i;

      for (i = 0; i < totvert; i++, mvert++) {
        BLI_kdtree_3d_insert(MirrKdStore.tree, i, mvert->co);
      }
    }

    BLI_kdtree_3d_balance(MirrKdStore.tree);
  }
  else if (mode == 'e') { /* end table */
    if (MirrKdStore.tree) {
      BLI_kdtree_3d_free(MirrKdStore.tree);
      MirrKdStore.tree = NULL;
    }
  }
  else {
    BLI_assert(0);
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Topology Mirror API
 * \{ */

typedef unsigned int MirrTopoHash_t;

typedef struct MirrTopoVert_t {
  MirrTopoHash_t hash;
  int v_index;
} MirrTopoVert_t;

static int mirrtopo_hash_sort(const void *l1, const void *l2)
{
  if ((MirrTopoHash_t)(intptr_t)l1 > (MirrTopoHash_t)(intptr_t)l2) {
    return 1;
  }
  else if ((MirrTopoHash_t)(intptr_t)l1 < (MirrTopoHash_t)(intptr_t)l2) {
    return -1;
  }
  return 0;
}

static int mirrtopo_vert_sort(const void *v1, const void *v2)
{
  if (((MirrTopoVert_t *)v1)->hash > ((MirrTopoVert_t *)v2)->hash) {
    return 1;
  }
  else if (((MirrTopoVert_t *)v1)->hash < ((MirrTopoVert_t *)v2)->hash) {
    return -1;
  }
  return 0;
}

bool ED_mesh_mirrtopo_recalc_check(Mesh *me, Mesh *me_eval, MirrTopoStore_t *mesh_topo_store)
{
  const bool is_editmode = (me->edit_mesh != NULL);
  int totvert;
  int totedge;

  if (me_eval) {
    totvert = me_eval->totvert;
    totedge = me_eval->totedge;
  }
  else if (me->edit_mesh) {
    totvert = me->edit_mesh->bm->totvert;
    totedge = me->edit_mesh->bm->totedge;
  }
  else {
    totvert = me->totvert;
    totedge = me->totedge;
  }

  if ((mesh_topo_store->index_lookup == NULL) ||
      (mesh_topo_store->prev_is_editmode != is_editmode) ||
      (totvert != mesh_topo_store->prev_vert_tot) || (totedge != mesh_topo_store->prev_edge_tot)) {
    return true;
  }
  else {
    return false;
  }
}

void ED_mesh_mirrtopo_init(Mesh *me,
                           Mesh *me_eval,
                           MirrTopoStore_t *mesh_topo_store,
                           const bool skip_em_vert_array_init)
{
  const bool is_editmode = (me->edit_mesh != NULL);
  MEdge *medge = NULL, *med;
  BMEditMesh *em = me_eval ? NULL : me->edit_mesh;

  /* editmode*/
  BMEdge *eed;
  BMIter iter;

  int a, last;
  int totvert, totedge;
  int tot_unique = -1, tot_unique_prev = -1;
  int tot_unique_edges = 0, tot_unique_edges_prev;

  MirrTopoHash_t *topo_hash = NULL;
  MirrTopoHash_t *topo_hash_prev = NULL;
  MirrTopoVert_t *topo_pairs;
  MirrTopoHash_t topo_pass = 1;

  intptr_t *index_lookup; /* direct access to mesh_topo_store->index_lookup */

  /* reallocate if needed */
  ED_mesh_mirrtopo_free(mesh_topo_store);

  mesh_topo_store->prev_is_editmode = is_editmode;

  if (em) {
    BM_mesh_elem_index_ensure(em->bm, BM_VERT);

    totvert = em->bm->totvert;
  }
  else {
    totvert = me_eval ? me_eval->totvert : me->totvert;
  }

  topo_hash = MEM_callocN(totvert * sizeof(MirrTopoHash_t), "TopoMirr");

  /* Initialize the vert-edge-user counts used to detect unique topology */
  if (em) {
    totedge = me->edit_mesh->bm->totedge;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      const int i1 = BM_elem_index_get(eed->v1), i2 = BM_elem_index_get(eed->v2);
      topo_hash[i1]++;
      topo_hash[i2]++;
    }
  }
  else {
    totedge = me_eval ? me_eval->totedge : me->totedge;
    medge = me_eval ? me_eval->medge : me->medge;

    for (a = 0, med = medge; a < totedge; a++, med++) {
      const unsigned int i1 = med->v1, i2 = med->v2;
      topo_hash[i1]++;
      topo_hash[i2]++;
    }
  }

  topo_hash_prev = MEM_dupallocN(topo_hash);

  tot_unique_prev = -1;
  tot_unique_edges_prev = -1;
  while (1) {
    /* use the number of edges per vert to give verts unique topology IDs */

    tot_unique_edges = 0;

    /* This can make really big numbers, wrapping around here is fine */
    if (em) {
      BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
        const int i1 = BM_elem_index_get(eed->v1), i2 = BM_elem_index_get(eed->v2);
        topo_hash[i1] += topo_hash_prev[i2] * topo_pass;
        topo_hash[i2] += topo_hash_prev[i1] * topo_pass;
        tot_unique_edges += (topo_hash[i1] != topo_hash[i2]);
      }
    }
    else {
      for (a = 0, med = medge; a < totedge; a++, med++) {
        const unsigned int i1 = med->v1, i2 = med->v2;
        topo_hash[i1] += topo_hash_prev[i2] * topo_pass;
        topo_hash[i2] += topo_hash_prev[i1] * topo_pass;
        tot_unique_edges += (topo_hash[i1] != topo_hash[i2]);
      }
    }
    memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

    /* sort so we can count unique values */
    qsort(topo_hash_prev, totvert, sizeof(MirrTopoHash_t), mirrtopo_hash_sort);

    tot_unique = 1; /* account for skipping the first value */
    for (a = 1; a < totvert; a++) {
      if (topo_hash_prev[a - 1] != topo_hash_prev[a]) {
        tot_unique++;
      }
    }

    if ((tot_unique <= tot_unique_prev) && (tot_unique_edges <= tot_unique_edges_prev)) {
      /* Finish searching for unique values when 1 loop dosn't give a
       * higher number of unique values compared to the previous loop. */
      break;
    }
    else {
      tot_unique_prev = tot_unique;
      tot_unique_edges_prev = tot_unique_edges;
    }
    /* Copy the hash calculated this iteration, so we can use them next time */
    memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

    topo_pass++;
  }

  /* Hash/Index pairs are needed for sorting to find index pairs */
  topo_pairs = MEM_callocN(sizeof(MirrTopoVert_t) * totvert, "MirrTopoPairs");

  /* since we are looping through verts, initialize these values here too */
  index_lookup = MEM_mallocN(totvert * sizeof(*index_lookup), "mesh_topo_lookup");

  if (em) {
    if (skip_em_vert_array_init == false) {
      BM_mesh_elem_table_ensure(em->bm, BM_VERT);
    }
  }

  for (a = 0; a < totvert; a++) {
    topo_pairs[a].hash = topo_hash[a];
    topo_pairs[a].v_index = a;

    /* initialize lookup */
    index_lookup[a] = -1;
  }

  qsort(topo_pairs, totvert, sizeof(MirrTopoVert_t), mirrtopo_vert_sort);

  last = 0;

  /* Get the pairs out of the sorted hashes, note, totvert+1 means we can use the previous 2,
   * but you cant ever access the last 'a' index of MirrTopoPairs */
  if (em) {
    BMVert **vtable = em->bm->vtable;
    for (a = 1; a <= totvert; a++) {
      // printf("I %d %ld %d\n",
      //        (a - last), MirrTopoPairs[a].hash, MirrTopoPairs[a].v_index);
      if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
        const int match_count = a - last;
        if (match_count == 2) {
          const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
          index_lookup[j] = (intptr_t)vtable[k];
          index_lookup[k] = (intptr_t)vtable[j];
        }
        else if (match_count == 1) {
          /* Center vertex. */
          const int j = topo_pairs[a - 1].v_index;
          index_lookup[j] = (intptr_t)vtable[j];
        }
        last = a;
      }
    }
  }
  else {
    /* same as above, for mesh */
    for (a = 1; a <= totvert; a++) {
      if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
        const int match_count = a - last;
        if (match_count == 2) {
          const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
          index_lookup[j] = k;
          index_lookup[k] = j;
        }
        else if (match_count == 1) {
          /* Center vertex. */
          const int j = topo_pairs[a - 1].v_index;
          index_lookup[j] = j;
        }
        last = a;
      }
    }
  }

  MEM_freeN(topo_pairs);
  topo_pairs = NULL;

  MEM_freeN(topo_hash);
  MEM_freeN(topo_hash_prev);

  mesh_topo_store->index_lookup = index_lookup;
  mesh_topo_store->prev_vert_tot = totvert;
  mesh_topo_store->prev_edge_tot = totedge;
}

void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store)
{
  if (mesh_topo_store->index_lookup) {
    MEM_freeN(mesh_topo_store->index_lookup);
  }
  mesh_topo_store->index_lookup = NULL;
  mesh_topo_store->prev_vert_tot = -1;
  mesh_topo_store->prev_edge_tot = -1;
}

/** \} */
