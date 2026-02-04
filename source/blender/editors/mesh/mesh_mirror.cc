/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Mirror calculation for edit-mode and object mode.
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"

#include "BLI_kdtree.hh"

#include "ED_mesh.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Mesh Spatial Mirror API
 * \{ */

#define KD_THRESH 0.00002f

static struct {
  KDTree_3d *tree;
} MirrKdStore = {nullptr};

void ED_mesh_mirror_spatial_table_begin(Object *ob, BMEditMesh *em, Mesh *mesh_eval)
{
  Mesh *mesh = id_cast<Mesh *>(ob->data);
  const bool use_em = (!mesh_eval && em && mesh->runtime->edit_mesh.get() == em);
  const int totvert = use_em    ? em->bm->totvert :
                      mesh_eval ? mesh_eval->verts_num :
                                  mesh->verts_num;

  if (MirrKdStore.tree) { /* happens when entering this call without ending it */
    ED_mesh_mirror_spatial_table_end(ob);
  }

  MirrKdStore.tree = kdtree_3d_new(totvert);

  if (use_em) {
    BMVert *eve;
    BMIter iter;
    int i;

    /* this needs to be valid for index lookups later (callers need) */
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);

    BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
      kdtree_3d_insert(MirrKdStore.tree, i, eve->co);
    }
  }
  else {
    const Span<float3> positions = mesh_eval ? mesh_eval->vert_positions() :
                                               mesh->vert_positions();
    for (int i = 0; i < totvert; i++) {
      kdtree_3d_insert(MirrKdStore.tree, i, positions[i]);
    }
  }

  kdtree_3d_balance(MirrKdStore.tree);
}

int ED_mesh_mirror_spatial_table_lookup(Object *ob,
                                        BMEditMesh *em,
                                        Mesh *mesh_eval,
                                        const float co[3])
{
  if (MirrKdStore.tree == nullptr) {
    ED_mesh_mirror_spatial_table_begin(ob, em, mesh_eval);
  }

  if (MirrKdStore.tree) {
    KDTreeNearest_3d nearest;
    const int i = kdtree_3d_find_nearest(MirrKdStore.tree, co, &nearest);

    if (i != -1) {
      if (nearest.dist < KD_THRESH) {
        return i;
      }
    }
  }
  return -1;
}

void ED_mesh_mirror_spatial_table_end(Object * /*ob*/)
{
  /* TODO: store this in object/object-data (keep unused argument for now). */
  if (MirrKdStore.tree) {
    kdtree_3d_free(MirrKdStore.tree);
    MirrKdStore.tree = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Topology Mirror API
 * \{ */

using MirrTopoHash_t = uint;

struct MirrTopoVert_t {
  MirrTopoHash_t hash;
  int v_index;
};

static int mirrtopo_hash_sort(const void *l1, const void *l2)
{
  if (MirrTopoHash_t(intptr_t(l1)) > MirrTopoHash_t(intptr_t(l2))) {
    return 1;
  }
  if (MirrTopoHash_t(intptr_t(l1)) < MirrTopoHash_t(intptr_t(l2))) {
    return -1;
  }
  return 0;
}

static int mirrtopo_vert_sort(const void *v1, const void *v2)
{
  if ((static_cast<MirrTopoVert_t *>(const_cast<void *>(v1)))->hash >
      (static_cast<MirrTopoVert_t *>(const_cast<void *>(v2)))->hash)
  {
    return 1;
  }
  if ((static_cast<MirrTopoVert_t *>(const_cast<void *>(v1)))->hash <
      (static_cast<MirrTopoVert_t *>(const_cast<void *>(v2)))->hash)
  {
    return -1;
  }
  return 0;
}

bool ED_mesh_mirrtopo_recalc_check(BMEditMesh *em, Mesh *mesh, MirrTopoStore_t *mesh_topo_store)
{
  const bool is_editmode = em != nullptr;
  int totvert;
  int totedge;

  if (em) {
    totvert = em->bm->totvert;
    totedge = em->bm->totedge;
  }
  else {
    totvert = mesh->verts_num;
    totedge = mesh->edges_num;
  }

  if ((mesh_topo_store->index_lookup == nullptr) ||
      (mesh_topo_store->prev_is_editmode != is_editmode) ||
      (totvert != mesh_topo_store->prev_vert_tot) || (totedge != mesh_topo_store->prev_edge_tot))
  {
    return true;
  }
  return false;
}

void ED_mesh_mirrtopo_init(BMEditMesh *em,
                           Mesh *mesh,
                           MirrTopoStore_t *mesh_topo_store,
                           const bool skip_em_vert_array_init)
{
  if (em) {
    BLI_assert(mesh == nullptr);
  }
  const bool is_editmode = (em != nullptr);

  /* Edit-mode variables. */
  BMEdge *eed;
  BMIter iter;

  int a, last;
  int totvert, totedge;
  int tot_unique = -1, tot_unique_prev = -1;
  int tot_unique_edges = 0, tot_unique_edges_prev;

  MirrTopoHash_t topo_pass = 1;

  /* reallocate if needed */
  ED_mesh_mirrtopo_free(mesh_topo_store);

  mesh_topo_store->prev_is_editmode = is_editmode;

  if (em) {
    BM_mesh_elem_index_ensure(em->bm, BM_VERT);

    totvert = em->bm->totvert;
  }
  else {
    totvert = mesh->verts_num;
  }

  MirrTopoHash_t *topo_hash = MEM_new_array_zeroed<MirrTopoHash_t>(totvert, __func__);

  /* Initialize the vert-edge-user counts used to detect unique topology */
  if (em) {
    totedge = em->bm->totedge;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      const int i1 = BM_elem_index_get(eed->v1), i2 = BM_elem_index_get(eed->v2);
      topo_hash[i1]++;
      topo_hash[i2]++;
    }
  }
  else {
    totedge = mesh->edges_num;
    for (const int2 &edge : mesh->edges()) {
      topo_hash[edge[0]]++;
      topo_hash[edge[1]]++;
    }
  }

  MirrTopoHash_t *topo_hash_prev = MEM_dupalloc(topo_hash);

  tot_unique_prev = -1;
  tot_unique_edges_prev = -1;
  while (true) {
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
      for (const int2 &edge : mesh->edges()) {
        const int i1 = edge[0], i2 = edge[1];
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
      /* Finish searching for unique values when 1 loop doesn't give a
       * higher number of unique values compared to the previous loop. */
      break;
    }
    tot_unique_prev = tot_unique;
    tot_unique_edges_prev = tot_unique_edges;
    /* Copy the hash calculated this iteration, so we can use them next time */
    memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

    topo_pass++;
  }

  /* Hash/Index pairs are needed for sorting to find index pairs */
  MirrTopoVert_t *topo_pairs = MEM_new_array_zeroed<MirrTopoVert_t>(totvert, "MirrTopoPairs");

  /* since we are looping through verts, initialize these values here too */
  intptr_t *index_lookup = MEM_new_array_uninitialized<intptr_t>(totvert, "mesh_topo_lookup");

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

  /* Get the pairs out of the sorted hashes.
   * NOTE: `totvert + 1` means we can use the previous 2,
   * but you can't ever access the last 'a' index of #MirrTopoPairs. */
  if (em) {
    BMVert **vtable = em->bm->vtable;
    for (a = 1; a <= totvert; a++) {
      // printf("I %d %ld %d\n",
      //        (a - last), MirrTopoPairs[a].hash, MirrTopoPairs[a].v_index);
      if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
        const int match_count = a - last;
        if (match_count == 2) {
          const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
          index_lookup[j] = intptr_t(vtable[k]);
          index_lookup[k] = intptr_t(vtable[j]);
        }
        else if (match_count == 1) {
          /* Center vertex. */
          const int j = topo_pairs[a - 1].v_index;
          index_lookup[j] = intptr_t(vtable[j]);
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

  MEM_delete(topo_pairs);
  topo_pairs = nullptr;

  MEM_delete(topo_hash);
  MEM_delete(topo_hash_prev);

  mesh_topo_store->index_lookup = index_lookup;
  mesh_topo_store->prev_vert_tot = totvert;
  mesh_topo_store->prev_edge_tot = totedge;
}

void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store)
{
  MEM_SAFE_DELETE(mesh_topo_store->index_lookup);
  mesh_topo_store->prev_vert_tot = -1;
  mesh_topo_store->prev_edge_tot = -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name EditMeshSymmetryHelper API
 * \{ */

std::optional<EditMeshSymmetryHelper> EditMeshSymmetryHelper::create_if_needed(Object *ob,
                                                                               uchar htype)
{
  BLI_assert(htype != 0);
  BLI_assert((htype & ~(BM_VERT | BM_EDGE | BM_FACE)) == 0);

  if (!ob || !ob->data) {
    return std::nullopt;
  }
  Mesh *mesh = id_cast<Mesh *>(ob->data);
  BMEditMesh *em = BKE_editmesh_from_object(ob);

  if (!em || !em->bm || mesh->symmetry == 0) {
    return std::nullopt;
  }
  return EditMeshSymmetryHelper(ob, htype);
}

EditMeshSymmetryHelper::EditMeshSymmetryHelper(Object *ob, uchar htype)
    : em_(BKE_editmesh_from_object(ob)), mesh_(id_cast<Mesh *>(ob->data)), htype_(htype)
{
  BMesh *bmesh = em_->bm;
  use_topology_mirror_ = (mesh_->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  Set<BMVert *> processed_verts;
  Set<BMEdge *> processed_edges;
  Set<BMFace *> processed_faces;

  BMIter iter;

  for (int axis = 0; axis < 3; axis++) {
    if (mesh_->symmetry & (ME_SYMMETRY_X << axis)) {
      EDBM_verts_mirror_cache_begin(em_,
                                    axis,
                                    (htype_ & BM_VERT) != 0,
                                    (htype_ & BM_EDGE) != 0,
                                    (htype_ & BM_FACE) != 0,
                                    use_topology_mirror_);

      if (htype_ & BM_VERT) {
        BMVert *v_curr;
        BM_ITER_MESH (v_curr, &iter, bmesh, BM_VERTS_OF_MESH) {
          if (processed_verts.contains(v_curr)) {
            continue;
          }
          BMVert *v_mirr = EDBM_verts_mirror_get(em_, v_curr);
          if (v_mirr && v_mirr != v_curr) {
            BMVert *v_mirr_check = EDBM_verts_mirror_get(em_, v_mirr);
            if (v_mirr_check == v_curr) {
              vert_to_mirror_map_.lookup_or_add(v_curr, {}).append(v_mirr);
              vert_to_mirror_map_.lookup_or_add(v_mirr, {}).append(v_curr);
              processed_verts.add(v_curr);
              processed_verts.add(v_mirr);
            }
          }
        }
      }

      if (htype_ & BM_EDGE) {
        BMEdge *e_curr;
        BM_ITER_MESH (e_curr, &iter, bmesh, BM_EDGES_OF_MESH) {
          if (processed_edges.contains(e_curr)) {
            continue;
          }
          BMEdge *e_mirr = EDBM_verts_mirror_get_edge(em_, e_curr);
          if (e_mirr && e_mirr != e_curr) {
            BMEdge *e_mirr_check = EDBM_verts_mirror_get_edge(em_, e_mirr);
            if (e_mirr_check == e_curr) {
              edge_to_mirror_map_.lookup_or_add(e_curr, {}).append(e_mirr);
              edge_to_mirror_map_.lookup_or_add(e_mirr, {}).append(e_curr);
              processed_edges.add(e_curr);
              processed_edges.add(e_mirr);
            }
          }
        }
      }

      if (htype_ & BM_FACE) {
        BMFace *f_curr;
        BM_ITER_MESH (f_curr, &iter, bmesh, BM_FACES_OF_MESH) {
          if (processed_faces.contains(f_curr)) {
            continue;
          }
          BMFace *f_mirr = EDBM_verts_mirror_get_face(em_, f_curr);
          if (f_mirr && f_mirr != f_curr) {
            BMFace *f_mirr_check = EDBM_verts_mirror_get_face(em_, f_mirr);
            if (f_mirr_check == f_curr) {
              face_to_mirror_map_.lookup_or_add(f_curr, {}).append(f_mirr);
              face_to_mirror_map_.lookup_or_add(f_mirr, {}).append(f_curr);
              processed_faces.add(f_curr);
              processed_faces.add(f_mirr);
            }
          }
        }
      }

      EDBM_verts_mirror_cache_end(em_);
    }
  }
}
void EditMeshSymmetryHelper::apply_on_mirror_verts(BMVert *v, FunctionRef<void(BMVert *)> op) const
{
  BLI_assert((this->htype_ & BM_VERT) != 0);
  const Vector<BMVert *> *mirrors = this->vert_to_mirror_map_.lookup_ptr(v);
  if (mirrors) {
    for (BMVert *v_mirr : *mirrors) {
      op(v_mirr);
    }
  }
}

void EditMeshSymmetryHelper::apply_on_mirror_edges(BMEdge *e, FunctionRef<void(BMEdge *)> op) const
{
  BLI_assert((this->htype_ & BM_EDGE) != 0);
  const Vector<BMEdge *> *mirrors = this->edge_to_mirror_map_.lookup_ptr(e);
  if (mirrors) {
    for (BMEdge *e_mirr : *mirrors) {
      op(e_mirr);
    }
  }
}

void EditMeshSymmetryHelper::apply_on_mirror_faces(BMFace *f, FunctionRef<void(BMFace *)> op) const
{
  BLI_assert((this->htype_ & BM_FACE) != 0);
  const Vector<BMFace *> *mirrors = this->face_to_mirror_map_.lookup_ptr(f);
  if (mirrors) {
    for (BMFace *f_mirr : *mirrors) {
      op(f_mirr);
    }
  }
}

bool EditMeshSymmetryHelper::any_mirror_vert_selected(BMVert *v, const char hflag) const
{
  BLI_assert((this->htype_ & BM_VERT) != 0);
  const Vector<BMVert *> *mirrors = this->vert_to_mirror_map_.lookup_ptr(v);
  if (mirrors) {
    for (BMVert *v_mirr : *mirrors) {
      if (BM_elem_flag_test(v_mirr, hflag) && !BM_elem_flag_test(v_mirr, BM_ELEM_HIDDEN)) {
        return true;
      }
    }
  }
  return false;
}

bool EditMeshSymmetryHelper::any_mirror_edge_selected(BMEdge *e, const char hflag) const
{
  BLI_assert((this->htype_ & BM_EDGE) != 0);
  const Vector<BMEdge *> *mirrors = this->edge_to_mirror_map_.lookup_ptr(e);
  if (mirrors) {
    for (BMEdge *e_mirr : *mirrors) {
      if (BM_elem_flag_test(e_mirr, hflag) && !BM_elem_flag_test(e_mirr, BM_ELEM_HIDDEN)) {
        return true;
      }
    }
  }
  return false;
}

bool EditMeshSymmetryHelper::any_mirror_face_selected(BMFace *f, const char hflag) const
{
  BLI_assert((this->htype_ & BM_FACE) != 0);
  const Vector<BMFace *> *mirrors = this->face_to_mirror_map_.lookup_ptr(f);
  if (mirrors) {
    for (BMFace *f_mirr : *mirrors) {
      if (BM_elem_flag_test(f_mirr, hflag) && !BM_elem_flag_test(f_mirr, BM_ELEM_HIDDEN)) {
        return true;
      }
    }
  }
  return false;
}

void EditMeshSymmetryHelper::set_hflag_on_mirror_verts(BMVert *v,
                                                       const char hflag,
                                                       const bool value) const
{
  apply_on_mirror_verts(v, [this, hflag, value](BMVert *v_mirr) {
    if (hflag & BM_ELEM_SELECT) {
      BM_vert_select_set(this->em_->bm, v_mirr, value);
    }
    const char hflag_test = char(hflag & ~BM_ELEM_SELECT);
    if (hflag_test) {
      BM_elem_flag_set(v_mirr, hflag_test, value);
    }
  });
}

void EditMeshSymmetryHelper::set_hflag_on_mirror_edges(BMEdge *e,
                                                       char hflag,
                                                       const bool value) const
{
  apply_on_mirror_edges(e, [this, hflag, value](BMEdge *e_mirr) {
    if (hflag & BM_ELEM_SELECT) {
      BM_edge_select_set(this->em_->bm, e_mirr, value);
    }
    char hflag_test = char(hflag & ~BM_ELEM_SELECT);
    if (hflag_test) {
      BM_elem_flag_set(e_mirr, hflag_test, value);
    }
  });
}

void EditMeshSymmetryHelper::set_hflag_on_mirror_faces(BMFace *f,
                                                       const char hflag,
                                                       const bool value) const
{
  apply_on_mirror_faces(f, [this, hflag, value](BMFace *f_mirr) {
    if (hflag & BM_ELEM_SELECT) {
      BM_face_select_set(this->em_->bm, f_mirr, value);
    }
    char hflag_test = char(hflag & ~BM_ELEM_SELECT);
    if (hflag_test) {
      BM_elem_flag_set(f_mirr, hflag_test, value);
    }
  });
}

/** \} */

}  // namespace blender
