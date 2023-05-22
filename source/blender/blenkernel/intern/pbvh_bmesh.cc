/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/*

TODO:

Convergence improvements:
1. DONE: Limit number of edges processed per run.
2. DONE: Scale split steps by ratio of long to short edges to
   prevent runaway tesselation.
3. DONE: Detect and dissolve three and four valence vertices that are surrounded by
   all tris.
4. DONE: Use different (coarser) brush spacing for applying dyntopo

Drawing improvements:
4. PARTIAL DONE: Build and cache vertex index buffers, to reduce GPU bandwidth

Topology rake:
5. DONE: Enable new curvature topology rake code and add to UI.
6. DONE: Add code to cache curvature data per vertex in a CD layer.

*/

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_buffer.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_heap_simple.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_sort_utils.h"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"
#include "atomic_ops.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DRW_pbvh.hh"

#include "atomic_ops.h"
#include "bmesh.h"
#include "bmesh_log.h"
#include "dyntopo_intern.hh"
#include "pbvh_intern.hh"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using blender::Span;

#include <cstdarg>

using blender::float2;
using blender::float3;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Vector;

template<typename T> T *c_array_from_vector(Vector<T> &array)
{
  T *ret = MEM_cnew_array<T>(array.size(), __func__);
  memcpy(static_cast<void *>(ret), static_cast<void *>(array.data()), sizeof(T) * array.size());
  return ret;
}

static void _debugprint(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

#ifdef PBVH_BMESH_DEBUG
void pbvh_bmesh_check_nodes_simple(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;
    BMFace *f;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    TGSET_ITER (f, node->bm_faces) {
      if (!f || f->head.htype != BM_FACE) {
        _debugprint("Corrupted (freed?) face in node->bm_faces\n");
        continue;
      }

      if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) != i) {
        _debugprint("Face in more then one node\n");
      }
    }
    TGSET_ITER_END;
  }
}

void pbvh_bmesh_check_nodes(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (node->flag & PBVH_Leaf) {
      pbvh_bmesh_check_other_verts(node);
    }
  }

  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

    if (ni >= 0 && (!v->e || !v->e->l)) {
      _debugprint("wire vert had node reference: %p (type %d)\n", v, v->head.htype);
      // BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
    }

    if (ni < -1 || ni >= pbvh->totnode) {
      _debugprint("vert node ref was invalid: %p (type %d)\n", v, v->head.htype);
      continue;
    }

    if (ni == -1) {
      continue;
    }

    PBVHNode *node = pbvh->nodes + ni;
    if (!(node->flag & PBVH_Leaf) || !node->bm_unique_verts) {
      _debugprint("vert node ref was in non leaf node");
      continue;
    }

    if (!BLI_table_gset_haskey(node->bm_unique_verts, v)) {
      _debugprint("vert not in node->bm_unique_verts\n");
    }

    if (BLI_table_gset_haskey(node->bm_other_verts, v)) {
      _debugprint("vert in node->bm_other_verts");
    }

    BKE_pbvh_bmesh_check_valence(pbvh, (PBVHVertRef){.i = (intptr_t)v});

    if (BM_vert_edge_count(v) != BM_ELEM_CD_GET_INT(v, pbvh->cd_valence)) {
      _debugprint("cached vertex valence mismatch; old: %d, should be: %d\n",
                  mv->valence,
                  BM_vert_edge_count(v));
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;
    BMVert *v;
    BMFace *f;

    // delete nodes should
    if (node->flag & PBVH_Delete) {
      _debugprint("orphaned delete node\n");
    }

    if (!(node->flag & PBVH_Leaf)) {
      if (node->bm_unique_verts || node->bm_other_verts || node->bm_faces) {
        _debugprint("dangling leaf pointers in non-leaf node\n");
      }

      continue;
    }

    TGSET_ITER (v, node->bm_unique_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

      if (ni != i) {
        if (ni >= 0 && ni < pbvh->totnode) {
          PBVHNode *node2 = pbvh->nodes + ni;
          _debugprint("v node offset is wrong, %d\n",
                      !node2->bm_unique_verts ? 0 :
                                                BLI_table_gset_haskey(node2->bm_unique_verts, v));
        }
        else {
          _debugprint("v node offset is wrong\n");
        }
      }

      if (!v || v->head.htype != BM_VERT) {
        _debugprint("corruption in pbvh! bm_unique_verts\n");
      }
      else if (BLI_table_gset_haskey(node->bm_other_verts, v)) {
        _debugprint("v in both unique and other verts\n");
      }
    }
    TGSET_ITER_END;

    TGSET_ITER (f, node->bm_faces) {
      if (!f || f->head.htype != BM_FACE) {
        _debugprint("corruption in pbvh! bm_faces\n");
        continue;
      }

      int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
      if (pbvh->nodes + ni != node) {
        _debugprint("face in multiple nodes!\n");
      }
    }
    TGSET_ITER_END;

    TGSET_ITER (v, node->bm_other_verts) {
      if (!v || v->head.htype != BM_VERT) {
        _debugprint("corruption in pbvh! bm_other_verts\n");
      }
      else if (BLI_table_gset_haskey(node->bm_unique_verts, v)) {
        _debugprint("v in both unique and other verts\n");
      }
    }
    TGSET_ITER_END;
  }
}

extern "C" void BKE_pbvh_bmesh_check_nodes(PBVH *pbvh)
{
  pbvh_bmesh_check_nodes(pbvh);
}
#else
extern "C" void BKE_pbvh_bmesh_check_nodes(PBVH * /*pbvh*/) {}
#endif

/** \} */

/****************************** Vertex/Face APIs ******************************/
namespace blender::bke::dyntopo {

void pbvh_kill_vert(PBVH *pbvh, BMVert *v, bool log_vert, bool log_edges)
{
  BMEdge *e = v->e;
  bm_logstack_push();

  if (e && log_edges) {
    do {
      BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, e);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }

  /* Release IDs. */
  if (e) {
    do {
      BMLoop *l = e->l;
      if (l) {
        do {
          int id = BM_idmap_get_id(pbvh->bm_idmap, reinterpret_cast<BMElem *>(l->f));
          if (id != BM_ID_NONE) {
            BM_idmap_release(pbvh->bm_idmap, (BMElem *)l->f, true);
          }
        } while ((l = l->radial_next) != e->l);
      }

      BM_idmap_release(pbvh->bm_idmap, (BMElem *)e, true);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }

  if (log_vert) {
    BM_log_vert_removed(pbvh->header.bm, pbvh->bm_log, v);
  }

  BM_idmap_release(pbvh->bm_idmap, (BMElem *)v, true);
  BM_vert_kill(pbvh->header.bm, v);
  bm_logstack_pop();
}

static BMVert *pbvh_bmesh_vert_create(PBVH *pbvh,
                                      int node_index,
                                      const float co[3],
                                      const float no[3],
                                      BMVert *v_example,
                                      const int /*cd_vert_mask_offset*/)
{
  PBVHNode *node = &pbvh->nodes[node_index];

  BLI_assert((pbvh->totnode == 1 || node_index) && node_index <= pbvh->totnode);

  /* avoid initializing customdata because its quite involved */
  BMVert *v = BM_vert_create(pbvh->header.bm, co, nullptr, BM_CREATE_NOP);

  pbvh_boundary_update_bmesh(pbvh, v);
  dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE);

  if (v_example) {
    v->head.hflag = v_example->head.hflag;

    CustomData_bmesh_copy_data(
        &pbvh->header.bm->vdata, &pbvh->header.bm->vdata, v_example->head.data, &v->head.data);

    /* This value is logged below */
    copy_v3_v3(v->no, no);

    // keep MSculptVert copied from v_example as-is
  }
  else {
#if 0 /* XXX: do we need to load original data here ? */
    MSculptVert *mv = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v);

    copy_v3_v3(mv->origco, co);
    copy_v3_v3(mv->origno, no);
    mv->origmask = 0.0f;
#endif

    /* This value is logged below */
    copy_v3_v3(v->no, no);
  }

  BLI_table_gset_insert(node->bm_unique_verts, v);
  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, node_index);

  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris | PBVH_UpdateOtherVerts;

  /* Log the new vertex */
  BM_log_vert_added(pbvh->header.bm, pbvh->bm_log, v);
  v->head.index = pbvh->header.bm->totvert;  // set provisional index

  return v;
}

static BMFace *bmesh_face_create_edge_log(PBVH *pbvh,
                                          BMVert *v_tri[3],
                                          BMEdge *e_tri[3],
                                          const BMFace *f_example)
{
  BMFace *f;

  if (!e_tri) {
    BMEdge *e_tri2[3];

    for (int i = 0; i < 3; i++) {
      BMVert *v1 = v_tri[i];
      BMVert *v2 = v_tri[(i + 1) % 3];

      BMEdge *e = BM_edge_exists(v1, v2);

      if (!e) {
        e = BM_edge_create(pbvh->header.bm, v1, v2, nullptr, BM_CREATE_NOP);
        BM_log_edge_added(pbvh->header.bm, pbvh->bm_log, e);
      }

      e_tri2[i] = e;
    }

    // f = BM_face_create_verts(pbvh->header.bm, v_tri, 3, f_example, BM_CREATE_NOP, true);
    f = BM_face_create(pbvh->header.bm, v_tri, e_tri2, 3, f_example, BM_CREATE_NOP);
  }
  else {
    f = BM_face_create(pbvh->header.bm, v_tri, e_tri, 3, f_example, BM_CREATE_NOP);
  }

  if (f_example) {
    f->head.hflag = f_example->head.hflag;
  }

  return f;
}

/**
 * \note Callers are responsible for checking if the face exists before adding.
 */
BMFace *pbvh_bmesh_face_create(PBVH *pbvh,
                               int node_index,
                               BMVert *v_tri[3],
                               BMEdge *e_tri[3],
                               const BMFace *f_example,
                               bool ensure_verts,
                               bool log_face)
{
  PBVHNode *node = &pbvh->nodes[node_index];

  /* ensure we never add existing face */
  BLI_assert(!BM_face_exists(v_tri, 3));

  BMFace *f = bmesh_face_create_edge_log(pbvh, v_tri, e_tri, f_example);

  BLI_table_gset_insert(node->bm_faces, f);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, node_index);

  /* mark node for update */
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris |
                PBVH_UpdateOtherVerts | PBVH_UpdateCurvatureDir | PBVH_UpdateTriAreas;
  node->flag &= ~PBVH_FullyHidden;

  /* Log the new face */
  if (log_face) {
    BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f);
  }

  int cd_vert_node = pbvh->cd_vert_node_offset;

  if (ensure_verts) {
    BMLoop *l = f->l_first;
    do {
      int ni = BM_ELEM_CD_GET_INT(l->v, cd_vert_node);

      if (ni == DYNTOPO_NODE_NONE) {
        BLI_table_gset_add(node->bm_unique_verts, l->v);
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, node_index);

        node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris |
                      PBVH_UpdateOtherVerts;
      }

      pbvh_boundary_update_bmesh(pbvh, l->v);
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE);

      l = l->next;
    } while (l != f->l_first);
  }
  else {
    BMLoop *l = f->l_first;
    do {
      pbvh_boundary_update_bmesh(pbvh, l->v);
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE);
    } while ((l = l->next) != f->l_first);
  }

  return f;
}

BMVert *BKE_pbvh_vert_create_bmesh(
    PBVH *pbvh, float co[3], float no[3], PBVHNode *node, BMVert *v_example)
{
  if (!node) {
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node2 = pbvh->nodes + i;

      if (!(node2->flag & PBVH_Leaf)) {
        continue;
      }

      /* Ensure we have at least some node somewhere picked. */
      node = node2;

      bool ok = true;

      for (int j = 0; j < 3; j++) {
        if (co[j] < node2->vb.bmin[j] || co[j] >= node2->vb.bmax[j]) {
          continue;
        }
      }

      if (ok) {
        break;
      }
    }
  }

  BMVert *v;

  if (!node) {
    printf("possible pbvh error\n");
    v = BM_vert_create(pbvh->header.bm, co, v_example, BM_CREATE_NOP);
    BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

    pbvh_boundary_update_bmesh(pbvh, v);
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE);

#if 0 /* XXX: do we need to load origco here? */
    copy_v3_v3(mv->origco, co);
#endif

    return v;
  }

  return pbvh_bmesh_vert_create(
      pbvh, node - pbvh->nodes, co, no, v_example, pbvh->cd_vert_mask_offset);
}

PBVHNode *BKE_pbvh_node_from_face_bmesh(PBVH *pbvh, BMFace *f)
{
  return pbvh->nodes + BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
}

BMFace *BKE_pbvh_face_create_bmesh(PBVH *pbvh,
                                   BMVert *v_tri[3],
                                   BMEdge *e_tri[3],
                                   const BMFace *f_example)
{
  int ni = DYNTOPO_NODE_NONE;

  for (int i = 0; i < 3; i++) {
    BMVert *v = v_tri[i];
    BMLoop *l;
    BMIter iter;

    BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
      int ni2 = BM_ELEM_CD_GET_INT(l->f, pbvh->cd_face_node_offset);
      if (ni2 != DYNTOPO_NODE_NONE) {
        ni = ni2;
        break;
      }
    }
  }

  if (ni == DYNTOPO_NODE_NONE) {
    BMFace *f;

    /* No existing nodes? Find one. */
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if (!(node->flag & PBVH_Leaf)) {
        continue;
      }

      for (int j = 0; j < 3; j++) {
        BMVert *v = v_tri[j];

        bool ok = true;

        for (int k = 0; k < 3; k++) {
          if (v->co[k] < node->vb.bmin[k] || v->co[k] >= node->vb.bmax[k]) {
            ok = false;
          }
        }

        if (ok &&
            (ni == DYNTOPO_NODE_NONE || BLI_table_gset_len(node->bm_faces) < pbvh->leaf_limit)) {
          ni = i;
          break;
        }
      }

      if (ni != DYNTOPO_NODE_NONE) {
        break;
      }
    }

    if (ni == DYNTOPO_NODE_NONE) {
      /* Empty pbvh? */
      f = bmesh_face_create_edge_log(pbvh, v_tri, e_tri, f_example);

      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

      return f;
    }
  }

  return pbvh_bmesh_face_create(pbvh, ni, v_tri, e_tri, f_example, true, true);
}

#define pbvh_bmesh_node_vert_use_count_is_equal(pbvh, node, v, n) \
  (pbvh_bmesh_node_vert_use_count_at_most(pbvh, node, v, (n) + 1) == n)

static int pbvh_bmesh_node_vert_use_count_at_most(PBVH *pbvh,
                                                  PBVHNode *node,
                                                  BMVert *v,
                                                  const int count_max)
{
  int count = 0;
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);
    if (f_node == node) {
      count++;
      if (count == count_max) {
        return count;
      }
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return count;
}

/* Return a node that uses vertex 'v' other than its current owner */
static PBVHNode *pbvh_bmesh_vert_other_node_find(PBVH *pbvh, BMVert *v)
{
  PBVHNode *current_node = pbvh_bmesh_node_from_vert(pbvh, v);
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);

    if (f_node != current_node) {
      return f_node;
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return nullptr;
}

static void pbvh_bmesh_vert_ownership_transfer(PBVH *pbvh, PBVHNode *new_owner, BMVert *v)
{
  PBVHNode *current_owner = pbvh_bmesh_node_from_vert(pbvh, v);
  /* Mark node for update. */

  if (current_owner) {
    current_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB;

    BLI_assert(current_owner != new_owner);

    /* Remove current ownership. */
    BLI_table_gset_remove(current_owner->bm_unique_verts, v, nullptr);
  }

  /* Set new ownership. */
  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, new_owner - pbvh->nodes);
  BLI_table_gset_insert(new_owner->bm_unique_verts, v);

  /* Mark node for update. */
  new_owner->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateOtherVerts;
}

void pbvh_bmesh_vert_remove(PBVH *pbvh, BMVert *v)
{
  /* never match for first time */
  int f_node_index_prev = DYNTOPO_NODE_NONE;
  const int updateflag = PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris |
                         PBVH_UpdateNormals | PBVH_UpdateOtherVerts;

  PBVHNode *v_node = pbvh_bmesh_node_from_vert(pbvh, v);

  if (v_node && v_node->bm_unique_verts) {
    BLI_table_gset_remove(v_node->bm_unique_verts, v, nullptr);
    v_node->flag |= (PBVHNodeFlags)updateflag;
  }

  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

  /* Have to check each neighboring face's node */
  BMFace *f;
  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int f_node_index = pbvh_bmesh_node_index_from_face(pbvh, f);

    if (f_node_index == DYNTOPO_NODE_NONE) {
      continue;
    }

    /* faces often share the same node,
     * quick check to avoid redundant #BLI_table_gset_remove calls */
    if (f_node_index_prev != f_node_index) {
      f_node_index_prev = f_node_index;

      PBVHNode *f_node = &pbvh->nodes[f_node_index];
      f_node->flag |= (PBVHNodeFlags)updateflag;  // flag update of bm_other_verts

      BLI_assert(!BLI_table_gset_haskey(f_node->bm_unique_verts, v));
    }
  }
  BM_FACES_OF_VERT_ITER_END;
}

void pbvh_bmesh_face_remove(
    PBVH *pbvh, BMFace *f, bool log_face, bool check_verts, bool ensure_ownership_transfer)
{
  PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);

  if (!f_node || !(f_node->flag & PBVH_Leaf)) {
    printf("pbvh corruption\n");
    fflush(stdout);
    return;
  }

  bm_logstack_push();

  /* Check if any of this face's vertices need to be removed
   * from the node */
  if (check_verts) {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BMVert *v = l_iter->v;
      if (pbvh_bmesh_node_vert_use_count_is_equal(pbvh, f_node, v, 1)) {
        if (BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset) == f_node - pbvh->nodes) {
          // if (BLI_table_gset_haskey(f_node->bm_unique_verts, v)) {
          /* Find a different node that uses 'v' */
          PBVHNode *new_node;

          new_node = pbvh_bmesh_vert_other_node_find(pbvh, v);
          // BLI_assert(new_node || BM_vert_face_count_is_equal(v, 1));

          if (new_node) {
            pbvh_bmesh_vert_ownership_transfer(pbvh, new_node, v);
          }
          else if (ensure_ownership_transfer && !BM_vert_face_count_is_equal(v, 1)) {
            pbvh_bmesh_vert_remove(pbvh, v);

            f_node->flag |= PBVH_RebuildNodeVerts | PBVH_UpdateOtherVerts;
            // printf("failed to find new_node\n");
          }
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* Remove face from node and top level */
  BLI_table_gset_remove(f_node->bm_faces, f, nullptr);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

  /* Log removed face */
  if (log_face) {
    BM_log_face_removed(pbvh->header.bm, pbvh->bm_log, f);
  }

  /* mark node for update */
  f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris |
                  PBVH_UpdateOtherVerts | PBVH_UpdateTriAreas | PBVH_UpdateCurvatureDir;

  bm_logstack_pop();
}
}  // namespace blender::bke::dyntopo

/****************************** Building ******************************/

/* Update node data after splitting */
static void pbvh_bmesh_node_finalize(PBVH *pbvh,
                                     const int node_index,
                                     const int cd_vert_node_offset,
                                     const int cd_face_node_offset,
                                     bool add_orco)
{
  PBVHNode *n = &pbvh->nodes[node_index];
  bool has_visible = false;

  n->draw_batches = nullptr;

  /* Create vert hash sets */
  if (!n->bm_unique_verts) {
    n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  }
  n->bm_other_verts = BLI_table_gset_new("bm_other_verts");

  BB_reset(&n->vb);
  BB_reset(&n->orig_vb);
  BMFace *f;

  TGSET_ITER (f, n->bm_faces) {
    /* Update ownership of faces */
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    do {
      BMVert *v = l_iter->v;

      int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
      *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

      if (!BLI_table_gset_haskey(n->bm_unique_verts, v)) {
        if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
          BLI_table_gset_add(n->bm_other_verts, v);
        }
        else {
          BLI_table_gset_insert(n->bm_unique_verts, v);
          BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);
        }
      }
      /* Update node bounding box */
      BB_expand(&n->vb, v->co);
      BB_expand(&n->orig_vb, BM_ELEM_CD_PTR<float *>(v, pbvh->cd_origco));
    } while ((l_iter = l_iter->next) != l_first);

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      has_visible = true;
    }
  }
  TGSET_ITER_END

  BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
             n->vb.bmin[2] <= n->vb.bmax[2]);

  /* Build GPU buffers for new node and update vertex normals */
  BKE_pbvh_node_mark_rebuild_draw(n);

  BKE_pbvh_node_fully_hidden_set(n, !has_visible);
  n->flag |= PBVH_UpdateNormals | PBVH_UpdateCurvatureDir | PBVH_UpdateTris;
  n->flag |= PBVH_UpdateBB | PBVH_UpdateOriginalBB;

  if (add_orco) {
    BKE_pbvh_bmesh_check_tris(pbvh, n);
  }
}

static void pbvh_print_mem_size(PBVH *pbvh)
{
  BMesh *bm = pbvh->header.bm;
  CustomData *cdatas[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};

  int tots[4] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};
  int sizes[4] = {
      (int)sizeof(BMVert), (int)sizeof(BMEdge), (int)sizeof(BMLoop), (int)sizeof(BMFace)};

  float memsize1[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float memsize2[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float tot = 0.0f;

  for (int i = 0; i < 4; i++) {
    CustomData *cdata = cdatas[i];

    memsize1[i] = (float)(sizes[i] * tots[i]) / 1024.0f / 1024.0f;
    memsize2[i] = (float)(cdata->totsize * tots[i]) / 1024.0f / 1024.0f;

    tot += memsize1[i] + memsize2[i];
  }

  printf("base sizes:\n");
  printf("  v: %.2fmb e: %.2fmb l: %.2fmb f: %.2fmb\n",
         memsize1[0],
         memsize1[1],
         memsize1[2],
         memsize1[3]);

  printf("custom attribute sizes:\n");
  printf("  v: %.2fmb e: %.2fmb l: %.2fmb f: %.2fmb\n",
         memsize2[0],
         memsize2[1],
         memsize2[2],
         memsize2[3]);

  int ptrsize = (int)sizeof(void *);

  float memsize3[3] = {(float)(ptrsize * pbvh->bm_idmap->map_size) / 1024.0f / 1024.0f,
                       (float)(ptrsize * pbvh->bm_idmap->freelist.capacity()) / 1024.0f / 1024.0f,
                       pbvh->bm_idmap->free_idx_map ?
                           (float)(4 * pbvh->bm_idmap->free_idx_map->capacity()) / 1024.0f /
                               1024.0f :
                           0.0f};

  printf("idmap sizes:\n  map_size: %.2fmb freelist_len: %.2fmb free_ids_size: %.2fmb\n",
         memsize3[0],
         memsize3[1],
         memsize3[2]);

  tot += memsize3[0] + memsize3[1] + memsize3[2];

  printf("total: %.2f\n", tot);

#ifdef WITH_BM_ID_FREELIST
  if (bm->idmap.free_idx_map) {
    printf("freelist length: %d\n", bm->idmap.freelist_len);
    /* printf("free_idx_map: nentries %d, size %d: nfreecells: %d\n",
           bm->idmap.free_idx_map->nentries,
           bm->idmap.free_idx_map->nbuckets,
           bm->idmap.free_idx_map->nfreecells);*/
  }
#endif
}

/* Recursively split the node if it exceeds the leaf_limit */
static void pbvh_bmesh_node_split(
    PBVH *pbvh, const BBC *bbc_array, int node_index, bool add_orco, int depth)
{
  const int cd_vert_node_offset = pbvh->cd_vert_node_offset;
  const int cd_face_node_offset = pbvh->cd_face_node_offset;
  PBVHNode *n = &pbvh->nodes[node_index];

#ifdef PROXY_ADVANCED
  BKE_pbvh_free_proxyarray(pbvh, n);
#endif

  if (n->depth >= PBVH_STACK_FIXED_DEPTH || BLI_table_gset_len(n->bm_faces) <= pbvh->leaf_limit) {
    /* Node limit not exceeded */
    pbvh_bmesh_node_finalize(pbvh, node_index, cd_vert_node_offset, cd_face_node_offset, add_orco);
    return;
  }

  /* Calculate bounding box around primitive centroids */
  BB cb;
  BB_reset(&cb);
  BMFace *f;

  TGSET_ITER (f, n->bm_faces) {
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    BB_expand(&cb, bbc->bcentroid);
  }
  TGSET_ITER_END

  /* Find widest axis and its midpoint */
  const int axis = BB_widest_axis(&cb);
  const float mid = (cb.bmax[axis] + cb.bmin[axis]) * 0.5f;

  if (isnan(mid)) {
    printf("NAN ERROR! %s\n", __func__);
  }

  /* Add two new child nodes */
  const int children = pbvh->totnode;
  n->children_offset = children;
  pbvh_grow_nodes(pbvh, pbvh->totnode + 2);

  /* Array reallocated, update current node pointer */
  n = &pbvh->nodes[node_index];

  /* Initialize children */
  PBVHNode *c1 = &pbvh->nodes[children], *c2 = &pbvh->nodes[children + 1];

  c1->draw_batches = c2->draw_batches = nullptr;
  c1->depth = c2->depth = n->depth + 1;

  c1->flag |= PBVH_Leaf;
  c2->flag |= PBVH_Leaf;

  c1->bm_faces = BLI_table_gset_new_ex("bm_faces", BLI_table_gset_len(n->bm_faces) / 2);
  c2->bm_faces = BLI_table_gset_new_ex("bm_faces", BLI_table_gset_len(n->bm_faces) / 2);

  c1->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  c2->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");

  c1->bm_other_verts = c2->bm_other_verts = nullptr;

  /* Partition the parent node's faces between the two children */
  TGSET_ITER (f, n->bm_faces) {
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    if (bbc->bcentroid[axis] < mid) {
      BLI_table_gset_insert(c1->bm_faces, f);
    }
    else {
      BLI_table_gset_insert(c2->bm_faces, f);
    }
  }
  TGSET_ITER_END
#if 0
    /* Enforce at least one primitive in each node */
    TableGSet *empty = nullptr,*other;
  if (BLI_table_gset_len(c1->bm_faces) == 0) {
    empty = c1->bm_faces;
    other = c2->bm_faces;
  } else if (BLI_table_gset_len(c2->bm_faces) == 0) {
    empty = c2->bm_faces;
    other = c1->bm_faces;
  }

  if (empty) {
    void *key;
    TGSET_ITER (key,other) {
      BLI_table_gset_insert(empty,key);
      BLI_table_gset_remove(other,key,nullptr);
      break;
    }
    TGSET_ITER_END
  }
#endif
  /* Clear this node */

  BMVert *v;

  /* Assign verts to c1 and c2.  Note that the previous
     method of simply marking them as untaken and rebuilding
     unique verts later doesn't work, as it assumes that dyntopo
     never assigns verts to nodes that don't contain their
     faces.*/
  if (n->bm_unique_verts) {
    TGSET_ITER (v, n->bm_unique_verts) {
      if (v->co[axis] < mid) {
        BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, (c1 - pbvh->nodes));
        BLI_table_gset_add(c1->bm_unique_verts, v);
      }
      else {
        BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, (c2 - pbvh->nodes));
        BLI_table_gset_add(c2->bm_unique_verts, v);
      }
    }
    TGSET_ITER_END

    BLI_table_gset_free(n->bm_unique_verts, nullptr);
  }

  if (n->bm_faces) {
    /* Unclaim faces */
    TGSET_ITER (f, n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
    }
    TGSET_ITER_END

    BLI_table_gset_free(n->bm_faces, nullptr);
  }

  if (n->bm_other_verts) {
    BLI_table_gset_free(n->bm_other_verts, nullptr);
  }

  if (n->layer_disp) {
    MEM_freeN(n->layer_disp);
  }

  if (n->tribuf || n->tri_buffers) {
    BKE_pbvh_bmesh_free_tris(pbvh, n);
  }

  n->bm_faces = nullptr;
  n->bm_unique_verts = nullptr;
  n->bm_other_verts = nullptr;
  n->layer_disp = nullptr;

  if (n->draw_batches) {
    DRW_pbvh_node_free(n->draw_batches);
    n->draw_batches = nullptr;
  }
  n->flag &= ~PBVH_Leaf;

  /* Recurse */
  pbvh_bmesh_node_split(pbvh, bbc_array, children, add_orco, depth + 1);
  pbvh_bmesh_node_split(pbvh, bbc_array, children + 1, add_orco, depth + 1);

  /* Array maybe reallocated, update current node pointer */
  n = &pbvh->nodes[node_index];

  /* Update bounding box */
  BB_reset(&n->vb);
  BB_expand_with_bb(&n->vb, &pbvh->nodes[n->children_offset].vb);
  BB_expand_with_bb(&n->vb, &pbvh->nodes[n->children_offset + 1].vb);
  n->orig_vb = n->vb;
}

/* Recursively split the node if it exceeds the leaf_limit */
bool pbvh_bmesh_node_limit_ensure(PBVH *pbvh, int node_index)
{
  TableGSet *bm_faces = pbvh->nodes[node_index].bm_faces;
  const int bm_faces_size = BLI_table_gset_len(bm_faces);

  if (bm_faces_size <= pbvh->leaf_limit || pbvh->nodes[node_index].depth >= PBVH_STACK_FIXED_DEPTH)
  {
    /* Node limit not exceeded */
    return false;
  }

  /* Trigger draw manager cache invalidation. */
  pbvh->draw_cache_invalid = true;

  /* For each BMFace, store the AABB and AABB centroid */
  BBC *bbc_array = MEM_cnew_array<BBC>(bm_faces_size, "BBC");

  BMFace *f;

  int i;

  /*
  TGSET_ITER_INDEX(f, bm_faces, i)
  {
  }
  TGSET_ITER_INDEX_END
  printf("size: %d %d\n", i + 1, bm_faces_size);
  */

  TGSET_ITER_INDEX(f, bm_faces, i)
  {
    BBC *bbc = &bbc_array[i];

    BB_reset((BB *)bbc);
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BB_expand((BB *)bbc, l_iter->v->co);
    } while ((l_iter = l_iter->next) != l_first);
    BBC_update_centroid(bbc);

    /* so we can do direct lookups on 'bbc_array' */
    BM_elem_index_set(f, i); /* set_dirty! */
  }
  TGSET_ITER_INDEX_END

  /* Likely this is already dirty. */
  pbvh->header.bm->elem_index_dirty |= BM_FACE;

  pbvh_bmesh_node_split(pbvh, bbc_array, node_index, false, 0);

  MEM_freeN(bbc_array);

  return true;
}

/**********************************************************************/

static bool point_in_node(const PBVHNode *node, const float co[3])
{
  return co[0] >= node->vb.bmin[0] && co[0] <= node->vb.bmax[0] && co[1] >= node->vb.bmin[1] &&
         co[1] <= node->vb.bmax[1] && co[2] >= node->vb.bmin[2] && co[2] <= node->vb.bmax[2];
}

void bke_pbvh_insert_face_finalize(PBVH *pbvh, BMFace *f, const int ni)
{
  PBVHNode *node = pbvh->nodes + ni;
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, ni);

  if (!(node->flag & PBVH_Leaf)) {
    printf("%s: major pbvh corruption error\n", __func__);
    return;
  }

  BLI_table_gset_add(node->bm_faces, f);

  PBVHNodeFlags updateflag = PBVH_UpdateTris | PBVH_UpdateBB | PBVH_UpdateDrawBuffers |
                             PBVH_UpdateCurvatureDir | PBVH_UpdateOtherVerts;
  updateflag |= PBVH_UpdateColor | PBVH_UpdateMask | PBVH_UpdateNormals | PBVH_UpdateOriginalBB;
  updateflag |= PBVH_UpdateVisibility | PBVH_UpdateRedraw | PBVH_RebuildDrawBuffers |
                PBVH_UpdateTriAreas;

  node->flag |= updateflag;

  // ensure verts are in pbvh
  BMLoop *l = f->l_first;
  do {
    const int ni2 = BM_ELEM_CD_GET_INT(l->v, pbvh->cd_vert_node_offset);

    BB_expand(&node->vb, l->v->co);
    BB_expand(&node->orig_vb, BM_ELEM_CD_PTR<float *>(l->v, pbvh->cd_origco));

    if (ni2 == DYNTOPO_NODE_NONE) {
      BM_ELEM_CD_SET_INT(l->v, pbvh->cd_vert_node_offset, ni);
      BLI_table_gset_add(node->bm_unique_verts, l->v);
    }
    else {
      PBVHNode *node2 = pbvh->nodes + ni2;

      if (ni != ni2) {
        BLI_table_gset_add(node->bm_other_verts, l->v);
      }

      node2->flag |= updateflag;

      float *origco = pbvh->cd_origco != -1 ? BM_ELEM_CD_PTR<float *>(l->v, pbvh->cd_origco) :
                                              l->v->co;

      BB_expand(&node2->vb, l->v->co);
      BB_expand(&node2->orig_vb, origco);
    }
    l = l->next;
  } while (l != f->l_first);
}

void bke_pbvh_insert_face(PBVH *pbvh, struct BMFace *f)
{
  int i = 0;
  bool ok = false;
  int ni = -1;

  while (i < pbvh->totnode) {
    PBVHNode *node = pbvh->nodes + i;
    bool ok2 = false;

    if (node->flag & PBVH_Leaf) {
      ok = true;
      ni = i;
      break;
    }

    if (node->children_offset == 0) {
      continue;
    }

    for (int j = 0; j < 2; j++) {
      int ni2 = node->children_offset + j;
      if (ni2 == 0) {
        continue;
      }

      PBVHNode *node2 = pbvh->nodes + ni2;
      BMLoop *l = f->l_first;

      do {
        if (point_in_node(node2, l->v->co)) {
          i = ni2;
          ok2 = true;
          break;
        }

        l = l->next;
      } while (l != f->l_first);

      if (ok2) {
        break;
      }
    }

    if (!ok2) {
      break;
    }
  }

  if (!ok) {
    // find closest node
    float co[3];
    int tot = 0;
    BMLoop *l = f->l_first;

    zero_v3(co);

    do {
      add_v3_v3(co, l->v->co);
      l = l->next;
      tot++;
    } while (l != f->l_first);

    mul_v3_fl(co, 1.0f / (float)tot);
    float mindis = 1e17;

    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if (!(node->flag & PBVH_Leaf)) {
        continue;
      }

      float cent[3];
      add_v3_v3v3(cent, node->vb.bmin, node->vb.bmax);
      mul_v3_fl(cent, 0.5f);

      float dis = len_squared_v3v3(co, cent);
      if (dis < mindis) {
        mindis = dis;
        ni = i;
      }
    }
  }

  if (ni < 0 || !(pbvh->nodes[ni].flag & PBVH_Leaf)) {
    fprintf(stderr, "pbvh error! failed to find node to insert face into!\n");
    fflush(stderr);
    return;
  }

  bke_pbvh_insert_face_finalize(pbvh, f, ni);
}

static void pbvh_bmesh_regen_node_verts(PBVH *pbvh, PBVHNode *node, bool report)
{
  node->flag &= ~PBVH_RebuildNodeVerts;

  int usize = BLI_table_gset_len(node->bm_unique_verts);
  int osize = BLI_table_gset_len(node->bm_other_verts);

  TableGSet *old_unique_verts = node->bm_unique_verts;
  TableGSet *old_other_verts = node->bm_other_verts;

  const int cd_vert_node = pbvh->cd_vert_node_offset;
  const int ni = (int)(node - pbvh->nodes);

  auto check_vert = [&](BMVert *v) {
    if (BM_elem_is_free(reinterpret_cast<BMElem *>(v), BM_VERT)) {
      if (report) {
        printf("%s: corrupted vertex %p\n", __func__, v);
      }
      return;
    }
    int ni2 = BM_ELEM_CD_GET_INT(v, cd_vert_node);

    bool bad = ni2 == ni || ni2 < 0 || ni2 >= pbvh->totnode;
    bad = bad || pbvh->nodes[ni2].flag & PBVH_Delete;
    bad = bad || !(pbvh->nodes[ni2].flag & PBVH_Leaf);

    if (bad) {
      BM_ELEM_CD_SET_INT(v, cd_vert_node, DYNTOPO_NODE_NONE);
    }
  };

  BMVert *v;
  TGSET_ITER (v, old_unique_verts) {
    check_vert(v);
  }
  TGSET_ITER_END;

  TGSET_ITER (v, old_other_verts) {
    check_vert(v);
  }
  TGSET_ITER_END;

  node->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  node->bm_other_verts = BLI_table_gset_new("bm_other_verts");

  bool update = false;

  BMFace *f;
  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;
    do {
      int ni2 = BM_ELEM_CD_GET_INT(l->v, cd_vert_node);

      if (ni2 == DYNTOPO_NODE_NONE) {
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, ni);
        ni2 = ni;
        update = true;
      }

      if (ni2 == ni) {
        BLI_table_gset_add(node->bm_unique_verts, l->v);
      }
      else {
        BLI_table_gset_add(node->bm_other_verts, l->v);
      }
    } while ((l = l->next) != f->l_first);
  }
  TGSET_ITER_END;

  TGSET_ITER (v, old_unique_verts) {
    if (BM_elem_is_free(reinterpret_cast<BMElem *>(v), BM_VERT)) {
      if (report) {
        printf("%s: corrupted vertex %p\n", __func__, v);
      }
      continue;
    }

    if (BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset) == -1) {
      // try to find node to insert into
      BMIter iter2;
      BMFace *f2;
      bool ok = false;

      BM_ITER_ELEM (f2, &iter2, v, BM_FACES_OF_VERT) {
        int ni2 = BM_ELEM_CD_GET_INT(f2, pbvh->cd_face_node_offset);

        if (ni2 >= 0) {
          BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, ni2);
          PBVHNode *node = pbvh->nodes + ni2;

          BLI_table_gset_add(node->bm_unique_verts, v);
          BLI_table_gset_remove(node->bm_other_verts, v, nullptr);

          ok = true;
          break;
        }
      }

      if (!ok) {
        printf("pbvh error: orphaned vert node reference\n");
      }
    }
  }
  TGSET_ITER_END;

  if (usize != BLI_table_gset_len(node->bm_unique_verts)) {
    update = true;
#if 0
    printf("possible pbvh error: bm_unique_verts might have had bad data. old: %d, new: %d\n",
      usize,
      BLI_table_gset_len(node->bm_unique_verts));
#endif
  }

  if (osize != BLI_table_gset_len(node->bm_other_verts)) {
    update = true;
#if 0
    printf("possible pbvh error: bm_other_verts might have had bad data. old: %d, new: %d\n",
      osize,
      BLI_table_gset_len(node->bm_other_verts));
#endif
  }

  if (update) {
    node->flag |= PBVH_UpdateNormals | PBVH_UpdateDrawBuffers | PBVH_RebuildDrawBuffers |
                  PBVH_UpdateBB;
    node->flag |= PBVH_UpdateOriginalBB | PBVH_UpdateRedraw | PBVH_UpdateColor | PBVH_UpdateTris |
                  PBVH_UpdateVisibility;
  }

  BLI_table_gset_free(old_unique_verts, nullptr);
  BLI_table_gset_free(old_other_verts, nullptr);
}

void BKE_pbvh_bmesh_mark_node_regen(PBVH * /*pbvh*/, PBVHNode *node)
{
  node->flag |= PBVH_RebuildNodeVerts;
}

PBVHNode *BKE_pbvh_get_node_leaf_safe(PBVH *pbvh, int i)
{
  if (i >= 0 && i < pbvh->totnode) {
    PBVHNode *node = pbvh->nodes + i;
    if ((node->flag & PBVH_Leaf) && !(node->flag & PBVH_Delete)) {
      return node;
    }
  }

  return nullptr;
}

void BKE_pbvh_bmesh_regen_node_verts(PBVH *pbvh, bool report)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf) || !(node->flag & PBVH_RebuildNodeVerts)) {
      continue;
    }

    pbvh_bmesh_regen_node_verts(pbvh, node, report);
  }
}

/************************* Called from pbvh.c *************************/

static bool pbvh_poly_hidden(PBVH * /*pbvh*/, BMFace *f)
{
  return BM_elem_flag_test(f, BM_ELEM_HIDDEN);
}

bool BKE_pbvh_bmesh_check_origdata(SculptSession *ss, BMVert *v, int /*stroke_id*/)
{
  PBVHVertRef vertex = {(intptr_t)v};
  return blender::bke::paint::get_original_vertex(ss, vertex, nullptr, nullptr, nullptr, nullptr);
}

bool pbvh_bmesh_node_raycast(SculptSession *ss,
                             PBVH *pbvh,
                             PBVHNode *node,
                             const float ray_start[3],
                             const float ray_normal[3],
                             struct IsectRayPrecalc *isect_precalc,
                             int *hit_count,
                             float *depth,
                             float *back_depth,
                             bool use_original,
                             PBVHVertRef *r_active_vertex,
                             PBVHFaceRef *r_active_face,
                             float *r_face_normal,
                             int stroke_id)
{
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  BKE_pbvh_bmesh_check_tris(pbvh, node);

  for (int i = 0; i < node->tribuf->tottri; i++) {
    PBVHTri *tri = node->tribuf->tris + i;
    BMVert *verts[3] = {
        (BMVert *)node->tribuf->verts[tri->v[0]].i,
        (BMVert *)node->tribuf->verts[tri->v[1]].i,
        (BMVert *)node->tribuf->verts[tri->v[2]].i,
    };

    float *cos[3];
    float *nos[3];

    if (use_original) {
      BKE_pbvh_bmesh_check_origdata(ss, verts[0], stroke_id);
      BKE_pbvh_bmesh_check_origdata(ss, verts[1], stroke_id);
      BKE_pbvh_bmesh_check_origdata(ss, verts[2], stroke_id);

      cos[0] = BM_ELEM_CD_PTR<float *>(verts[0], pbvh->cd_origco);
      cos[1] = BM_ELEM_CD_PTR<float *>(verts[1], pbvh->cd_origco);
      cos[2] = BM_ELEM_CD_PTR<float *>(verts[2], pbvh->cd_origco);

      nos[0] = BM_ELEM_CD_PTR<float *>(verts[0], pbvh->cd_origno);
      nos[1] = BM_ELEM_CD_PTR<float *>(verts[1], pbvh->cd_origno);
      nos[2] = BM_ELEM_CD_PTR<float *>(verts[2], pbvh->cd_origno);
    }
    else {
      for (int j = 0; j < 3; j++) {
        cos[j] = verts[j]->co;
        nos[j] = verts[j]->no;
      }
    }

    if (ray_face_intersection_depth_tri(
            ray_start, isect_precalc, cos[0], cos[1], cos[2], depth, back_depth, hit_count))
    {
      hit = true;

      if (r_face_normal) {
        normal_tri_v3(r_face_normal, cos[0], cos[1], cos[2]);
      }

      if (r_active_vertex) {
        float location[3] = {0.0f};
        madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);
        for (int j = 0; j < 3; j++) {
          if (j == 0 ||
              len_squared_v3v3(location, cos[j]) < len_squared_v3v3(location, nearest_vertex_co)) {
            copy_v3_v3(nearest_vertex_co, cos[j]);
            r_active_vertex->i = (intptr_t)verts[j];
          }
        }
      }

      if (r_active_face) {
        *r_active_face = tri->f;
      }
    }
  }

  return hit;
}

bool BKE_pbvh_bmesh_node_raycast_detail(PBVH *pbvh,
                                        PBVHNode *node,
                                        const float ray_start[3],
                                        struct IsectRayPrecalc *isect_precalc,
                                        float *depth,
                                        float *r_edge_length)
{
  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  BKE_pbvh_bmesh_check_tris(pbvh, node);
  for (int i = 0; i < node->tribuf->tottri; i++) {
    PBVHTri *tri = node->tribuf->tris + i;
    BMVert *v1 = (BMVert *)node->tribuf->verts[tri->v[0]].i;
    BMVert *v2 = (BMVert *)node->tribuf->verts[tri->v[1]].i;
    BMVert *v3 = (BMVert *)node->tribuf->verts[tri->v[2]].i;
    BMFace *f = (BMFace *)tri->f.i;

    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    bool hit_local = ray_face_intersection_tri(
        ray_start, isect_precalc, v1->co, v2->co, v3->co, depth);

    if (hit_local) {
      float len1 = len_squared_v3v3(v1->co, v2->co);
      float len2 = len_squared_v3v3(v2->co, v3->co);
      float len3 = len_squared_v3v3(v3->co, v1->co);

      /* detail returned will be set to the maximum allowed size, so take max here */
      *r_edge_length = sqrtf(max_fff(len1, len2, len3));

      return true;
    }
  }

  return false;
}

bool pbvh_bmesh_node_nearest_to_ray(SculptSession *ss,
                                    PBVH *pbvh,
                                    PBVHNode *node,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth,
                                    float *dist_sq,
                                    bool use_original,
                                    int stroke_id)
{
  bool hit = false;

  BKE_pbvh_bmesh_check_tris(pbvh, node);
  PBVHTriBuf *tribuf = node->tribuf;

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;
    BMFace *f = (BMFace *)tri->f.i;

    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    BMVert *v1 = (BMVert *)tribuf->verts[tri->v[0]].i;
    BMVert *v2 = (BMVert *)tribuf->verts[tri->v[1]].i;
    BMVert *v3 = (BMVert *)tribuf->verts[tri->v[2]].i;

    float *co1, *co2, *co3;

    if (use_original) {
      BKE_pbvh_bmesh_check_origdata(ss, v1, stroke_id);
      BKE_pbvh_bmesh_check_origdata(ss, v2, stroke_id);
      BKE_pbvh_bmesh_check_origdata(ss, v3, stroke_id);

      co1 = BM_ELEM_CD_PTR<float *>(v1, pbvh->cd_origco);
      co2 = BM_ELEM_CD_PTR<float *>(v2, pbvh->cd_origco);
      co3 = BM_ELEM_CD_PTR<float *>(v3, pbvh->cd_origco);
    }
    else {
      co1 = v1->co;
      co2 = v2->co;
      co3 = v3->co;
    }

    hit |= ray_face_nearest_tri(ray_start, ray_normal, co1, co2, co3, depth, dist_sq);
  }

  return hit;
}

struct UpdateNormalsTaskData {
  PBVHNode *node;
  Vector<BMVert *> border_verts;
  int cd_flag;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  int node_nr;
};

static void pbvh_update_normals_task_cb(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict /* tls */)
{
  BMVert *v;
  BMFace *f;
  UpdateNormalsTaskData *data = ((UpdateNormalsTaskData *)userdata) + n;
  PBVHNode *node = data->node;
  const int node_nr = data->node_nr;

  const int cd_face_node_offset = data->cd_face_node_offset;
  const int cd_vert_node_offset = data->cd_vert_node_offset;

  node->flag |= PBVH_UpdateCurvatureDir;

#ifdef NORMAL_VERT_BAD
#  undef NORMAL_VERT_BAD
#endif
#define NORMAL_VERT_BAD(v) \
  (!v->e || BM_ELEM_CD_GET_INT((v), cd_vert_node_offset) != node_nr || \
   ((*BM_ELEM_CD_PTR<uint8_t *>(v, data->cd_flag)) & SCULPTFLAG_PBVH_BOUNDARY))

  const char tag = BM_ELEM_TAG_ALT;

  TGSET_ITER (v, node->bm_unique_verts) {
    PBVH_CHECK_NAN(v->no);

    if (NORMAL_VERT_BAD(v)) {
      v->head.hflag |= tag;
      data->border_verts.append(v);
      continue;
    }

    v->head.hflag &= ~tag;

    BMEdge *e = v->e;
    do {
      BMLoop *l = e->l;

      if (!l) {
        continue;
      }

      do {
        if (BM_ELEM_CD_GET_INT(l->f, cd_face_node_offset) != node_nr) {
          v->head.hflag |= tag;
          goto loop_exit;
        }
      } while ((l = l->radial_next) != e->l);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  loop_exit:

    if (v->head.hflag & tag) {
      data->border_verts.append(v);
      continue;
    }

    zero_v3(v->no);
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
    BM_face_normal_update(f);

    PBVH_CHECK_NAN(f->no);

    BMLoop *l = f->l_first;
    do {
      PBVH_CHECK_NAN(l->v->no);

      if (BM_ELEM_CD_GET_INT(l->v, cd_vert_node_offset) == node_nr && !(l->v->head.hflag & tag)) {
        add_v3_v3(l->v->no, f->no);
      }
    } while ((l = l->next) != f->l_first);
  }
  TGSET_ITER_END

  TGSET_ITER (v, node->bm_unique_verts) {
    PBVH_CHECK_NAN(v->no);

    if (dot_v3v3(v->no, v->no) == 0.0f) {
      data->border_verts.append(v);

      continue;
    }

    if (!(v->head.hflag & tag)) {
      normalize_v3(v->no);
    }
  }
  TGSET_ITER_END

  node->flag &= ~PBVH_UpdateNormals;
}

void pbvh_bmesh_normals_update(PBVH *pbvh, Span<PBVHNode *> nodes)
{
  TaskParallelSettings settings;
  Vector<UpdateNormalsTaskData> datas;
  datas.resize(nodes.size());

  for (int i : nodes.index_range()) {
    datas[i].node = nodes[i];
    datas[i].cd_flag = pbvh->cd_flag;
    datas[i].cd_vert_node_offset = pbvh->cd_vert_node_offset;
    datas[i].cd_face_node_offset = pbvh->cd_face_node_offset;
    datas[i].node_nr = nodes[i] - pbvh->nodes;

    BKE_pbvh_bmesh_check_tris(pbvh, nodes[i]);
  }

  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), datas.data(), pbvh_update_normals_task_cb, &settings);

  /* not sure it's worth calling BM_mesh_elem_index_ensure here */
#if 0
  BLI_bitmap *visit = BLI_BITMAP_NEW(bm->totvert, "visit");
  BM_mesh_elem_index_ensure(bm, BM_VERT);
#endif

  for (int i = 0; i < datas.size(); i++) {
    UpdateNormalsTaskData *data = &datas[i];

#if 0
    printf("%.2f%% : %d %d\n",
      100.0f * (float)data->tot_border_verts / (float)data->node->bm_unique_verts->length,
      data->tot_border_verts,
      data->node->bm_unique_verts->length);
#endif

    for (int j = 0; j < data->border_verts.size(); j++) {
      BMVert *v = data->border_verts[j];

      if (BM_elem_is_free((BMElem *)v, BM_VERT)) {
        printf("%s: error, v was freed!\n", __func__);
        continue;
      }

#if 0
      if (v->head.index < 0 || v->head.index >= bm->totvert) {
        printf("%s: error, v->head.index was out of bounds!\n", __func__);
        continue;
      }

      if (BLI_BITMAP_TEST(visit, v->head.index)) {
        continue;
      }

      BLI_BITMAP_ENABLE(visit, v->head.index);
#endif

      // manual iteration
      BMEdge *e = v->e;

      if (!e) {
        continue;
      }

      zero_v3(v->no);

      do {
        if (e->l) {
          add_v3_v3(v->no, e->l->f->no);
        }
        e = BM_DISK_EDGE_NEXT(e, v);
      } while (e != v->e);

      normalize_v3(v->no);
    }
  }

#if 0
  MEM_SAFE_FREE(visit);
#endif
}

struct FastNodeBuildInfo {
  int totface; /* number of faces */
  int start;   /* start of faces in array */
  int depth;
  int node_index;
  struct FastNodeBuildInfo *child1;
  struct FastNodeBuildInfo *child2;
  float cent[3], no[3];
  int tag;
};

/**
 * Recursively split the node if it exceeds the leaf_limit.
 * This function is multi-thread-able since each invocation applies
 * to a sub part of the arrays.
 */
static void pbvh_bmesh_node_limit_ensure_fast(PBVH *pbvh,
                                              BMFace **nodeinfo,
                                              BBC *bbc_array,
                                              FastNodeBuildInfo *node,
                                              Vector<FastNodeBuildInfo *> &leaves,
                                              MemArena *arena)
{
  FastNodeBuildInfo *child1, *child2;

  if (node->totface <= pbvh->leaf_limit || node->depth >= PBVH_STACK_FIXED_DEPTH) {
    return;
  }

  /* Calculate bounding box around primitive centroids */
  BB cb;
  BB_reset(&cb);
  for (int i = 0; i < node->totface; i++) {
    BMFace *f = nodeinfo[i + node->start];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    BB_expand(&cb, bbc->bcentroid);
  }

  /* initialize the children */

  /* Find widest axis and its midpoint */
  const int axis = BB_widest_axis(&cb);
  const float mid = (cb.bmax[axis] + cb.bmin[axis]) * 0.5f;

  int num_child1 = 0, num_child2 = 0;

  /* split vertices along the middle line */
  const int end = node->start + node->totface;
  for (int i = node->start; i < end - num_child2; i++) {
    BMFace *f = nodeinfo[i];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    if (bbc->bcentroid[axis] > mid) {
      int i_iter = end - num_child2 - 1;
      int candidate = -1;
      /* found a face that should be part of another node, look for a face to substitute with */

      for (; i_iter > i; i_iter--) {
        BMFace *f_iter = nodeinfo[i_iter];
        const BBC *bbc_iter = &bbc_array[BM_elem_index_get(f_iter)];
        if (bbc_iter->bcentroid[axis] <= mid) {
          candidate = i_iter;
          break;
        }

        num_child2++;
      }

      if (candidate != -1) {
        BMFace *tmp = nodeinfo[i];
        nodeinfo[i] = nodeinfo[candidate];
        nodeinfo[candidate] = tmp;
        /* increase both counts */
        num_child1++;
        num_child2++;
      }
      else {
        /* not finding candidate means second half of array part is full of
         * second node parts, just increase the number of child nodes for it */
        num_child2++;
      }
    }
    else {
      num_child1++;
    }
  }

  /* ensure at least one child in each node */
  if (num_child2 == 0) {
    num_child2++;
    num_child1--;
  }
  else if (num_child1 == 0) {
    num_child1++;
    num_child2--;
  }

  /* at this point, faces should have been split along the array range sequentially,
   * each sequential part belonging to one node only */
  BLI_assert((num_child1 + num_child2) == node->totface);

  node->child1 = child1 = (FastNodeBuildInfo *)BLI_memarena_alloc(arena,
                                                                  sizeof(FastNodeBuildInfo));
  node->child2 = child2 = (FastNodeBuildInfo *)BLI_memarena_alloc(arena,
                                                                  sizeof(FastNodeBuildInfo));

  child1->totface = num_child1;
  child1->start = node->start;
  child1->depth = node->depth + 1;

  child2->totface = num_child2;
  child2->start = node->start + num_child1;
  child2->depth = node->depth + 2;

  child1->child1 = child1->child2 = child2->child1 = child2->child2 = nullptr;

  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, child1, leaves, arena);
  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, child2, leaves, arena);

  if (!child1->child1 && !child1->child2) {
    leaves.append(child1);
  }

  if (!child2->child1 && !child2->child2) {
    leaves.append(child2);
  }
}

struct LeafBuilderThreadData {
  PBVH *pbvh;
  BMFace **nodeinfo;
  BBC *bbc_array;
  Vector<FastNodeBuildInfo *> leaves;
};

static void pbvh_bmesh_create_leaf_fast_task_cb(void *__restrict userdata,
                                                const int i,
                                                const TaskParallelTLS *__restrict /* tls */)
{
  LeafBuilderThreadData *data = (LeafBuilderThreadData *)userdata;
  PBVH *pbvh = data->pbvh;
  BMFace **nodeinfo = data->nodeinfo;
  BBC *bbc_array = data->bbc_array;
  struct FastNodeBuildInfo *node = data->leaves[i];

  /* node does not have children so it's a leaf node, populate with faces and tag accordingly
   * this is an expensive part but it's not so easily thread-able due to vertex node indices */
  // const int cd_vert_node_offset = pbvh->cd_vert_node_offset;
  const int cd_face_node_offset = pbvh->cd_face_node_offset;

  PBVHNode *n = pbvh->nodes + node->node_index;
  const int node_index = node->node_index;

  bool has_visible = false;

  /* Build GPU buffers for new node */

  n->flag = PBVH_Leaf | PBVH_UpdateTris | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
            PBVH_UpdateTriAreas | PBVH_UpdateColor | PBVH_UpdateVisibility |
            PBVH_UpdateDrawBuffers | PBVH_RebuildDrawBuffers | PBVH_UpdateCurvatureDir |
            PBVH_UpdateMask | PBVH_UpdateRedraw;

  n->bm_faces = BLI_table_gset_new_ex("bm_faces", node->totface);

  /* Create vert hash sets */
  n->bm_unique_verts = BLI_table_gset_new_ex("bm_unique_verts", node->totface * 3);
  n->bm_other_verts = BLI_table_gset_new_ex("bm_other_verts", node->totface * 3);

  BB_reset(&n->vb);

  const int end = node->start + node->totface;

  for (int i = node->start; i < end; i++) {
    BMFace *f = nodeinfo[i];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    /* Update ownership of faces */
    BLI_table_gset_insert(n->bm_faces, f);
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BMVert *v = l_iter->v;

      int old = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

      char *ptr = (char *)v->head.data;
      ptr += pbvh->cd_vert_node_offset;

      if (old == DYNTOPO_NODE_NONE &&
          atomic_cas_int32((int32_t *)ptr, DYNTOPO_NODE_NONE, node_index) == DYNTOPO_NODE_NONE)
      {
        BLI_table_gset_insert(n->bm_unique_verts, v);
      }
      else {
        BLI_table_gset_add(n->bm_other_verts, v);
      }
    } while ((l_iter = l_iter->next) != l_first);

    /* Update node bounding box */
    if (!pbvh_poly_hidden(pbvh, f)) {
      has_visible = true;
    }

    BB_expand_with_bb(&n->vb, (BB *)bbc);
  }

  BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
             n->vb.bmin[2] <= n->vb.bmax[2]);

  n->orig_vb = n->vb;

  BKE_pbvh_node_fully_hidden_set(n, !has_visible);
  n->flag |= PBVH_UpdateNormals | PBVH_UpdateCurvatureDir;
}

static void pbvh_bmesh_create_nodes_fast_recursive_create(PBVH *pbvh,
                                                          BMFace **nodeinfo,
                                                          BBC *bbc_array,
                                                          struct FastNodeBuildInfo *node)
{
  /* two cases, node does not have children or does have children */
  if (node->child1) {
    int children_offset = pbvh->totnode;
    pbvh_grow_nodes(pbvh, pbvh->totnode + 2);

    PBVHNode *n = pbvh->nodes + node->node_index;
    n->children_offset = children_offset;

    n->depth = node->depth;
    (n + 1)->depth = node->child1->depth;
    (n + 2)->depth = node->child2->depth;

    node->child1->node_index = children_offset;
    node->child2->node_index = children_offset + 1;

    pbvh_bmesh_create_nodes_fast_recursive_create(pbvh, nodeinfo, bbc_array, node->child1);
    pbvh_bmesh_create_nodes_fast_recursive_create(pbvh, nodeinfo, bbc_array, node->child2);
  }
}

static void pbvh_bmesh_create_nodes_fast_recursive_final(PBVH *pbvh,
                                                         BMFace **nodeinfo,
                                                         BBC *bbc_array,
                                                         struct FastNodeBuildInfo *node)
{
  /* two cases, node does not have children or does have children */
  if (node->child1) {
    pbvh_bmesh_create_nodes_fast_recursive_final(pbvh, nodeinfo, bbc_array, node->child1);
    pbvh_bmesh_create_nodes_fast_recursive_final(pbvh, nodeinfo, bbc_array, node->child2);

    PBVHNode *n = pbvh->nodes + node->node_index;

    /* Update bounding box */
    BB_reset(&n->vb);
    BB_expand_with_bb(&n->vb, &pbvh->nodes[node->child1->node_index].vb);
    BB_expand_with_bb(&n->vb, &pbvh->nodes[node->child2->node_index].vb);
    n->orig_vb = n->vb;
  }
}

/***************************** Public API *****************************/

struct FSetTemp {
  BMVert *v;
  int fset;
  bool boundary;
};

int BKE_pbvh_do_fset_symmetry(int fset, const int symflag, const float *co)
{
  fset = abs(fset);

  // don't affect base face set
  if (fset == 1) {
    return 1;
  }

  /*
    flag symmetry by shifting by 24 bits;
    surely we don't need more then 8 million face sets?
  */
  if (co[0] < 0.0f) {
    fset |= (symflag & 1) << 24;
  }

  if (co[1] < 0.0f) {
    fset |= (symflag & 2) << 24;
  }

  if (co[2] < 0.0f) {
    fset |= (symflag & 4) << 24;
  }

  return fset;
}

//#define MV_COLOR_BOUNDARY

#ifdef MV_COLOR_BOUNDARY
static int color_boundary_key(float col[4])
{
  const float steps = 2.0f;
  float hsv[3];

  rgb_to_hsv(col[0], col[1], col[2], hsv, hsv + 1, hsv + 2);

  int x = (int)((hsv[0] * 0.5f + 0.5f) * steps + 0.5f);
  int y = (int)(hsv[1] * steps + 0.5f);
  int z = (int)(hsv[2] * steps + 0.5f);

  return z * steps * steps + y * steps + x;
}
#endif

static bool test_colinear_tri(BMFace *f)
{
  BMLoop *l = f->l_first;

  float area_limit = 0.00001f;
  area_limit = len_squared_v3v3(l->v->co, l->next->v->co) * 0.001;

  return area_tri_v3(l->v->co, l->next->v->co, l->prev->v->co) <= area_limit;
}

float BKE_pbvh_test_sharp_faces_bmesh(BMFace *f1, BMFace *f2, float limit)
{
  float angle = saacos(dot_v3v3(f1->no, f2->no));

#if 0  // XXX
  float no1[3], no2[3];
  normal_tri_v3(no1, f1->l_first->v->co, f1->l_first->next->v->co, f1->l_first->next->next->v->co);
  normal_tri_v3(no2, f2->l_first->v->co, f2->l_first->next->v->co, f2->l_first->next->next->v->co);

  angle = saacos(dot_v3v3(no1, no2));
#endif

  /* Detect coincident triangles. */
  if (f1->len == 3 && test_colinear_tri(f1)) {
    return false;
  }
  if (f2->len == 3 && test_colinear_tri(f2)) {
    return false;
  }

  /* Try to ignore folded over edges. */
  if (angle > M_PI * 0.6) {
    return false;
  }

  return angle > limit;
}

void BKE_pbvh_update_vert_boundary(int cd_faceset_offset,
                                   int cd_vert_node_offset,
                                   int cd_face_node_offset,
                                   int /*cd_vcol*/,
                                   int cd_boundary_flag,
                                   const int cd_flag,
                                   const int cd_valence,
                                   BMVert *v,
                                   int bound_symmetry,
                                   const CustomData *ldata,
                                   const int totuv,
                                   const bool do_uvs,
                                   float sharp_angle_limit)
{
  int newflag = *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag);
  int boundflag = 0;

  BMEdge *e = v->e;
  newflag &= ~(SCULPTFLAG_VERT_FSET_HIDDEN | SCULPTFLAG_PBVH_BOUNDARY);

  if (!e) {
    boundflag |= SCULPT_BOUNDARY_MESH;

    BM_ELEM_CD_SET_INT(v, cd_valence, 0);
    BM_ELEM_CD_SET_INT(v, cd_boundary_flag, 0);
    *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag) = newflag;

    return;
  }

  int val = 0;

  int ni = BM_ELEM_CD_GET_INT(v, cd_vert_node_offset);

  int sharpcount = 0;
  int seamcount = 0;
  int quadcount = 0;

#ifdef MV_COLOR_BOUNDARY
  int last_key = -1;
#endif

#if 0
  struct FaceSetRef {
    int fset;
    BMVert *v2;
    BMEdge *e;
  } *fsets = nullptr;
#endif
  Vector<int, 16> fsets;

  float(*lastuv)[2] = do_uvs ? (float(*)[2])BLI_array_alloca(lastuv, totuv) : nullptr;
  float(*lastuv2)[2] = do_uvs ? (float(*)[2])BLI_array_alloca(lastuv2, totuv) : nullptr;

  int *disjount_uv_count = do_uvs ? (int *)BLI_array_alloca(disjount_uv_count, totuv) : nullptr;
  int *cd_uvs = (int *)BLI_array_alloca(cd_uvs, totuv);
  int base_uv_idx = ldata->typemap[CD_PROP_FLOAT2];
  bool uv_first = true;

  if (do_uvs) {
    for (int i = 0; i < totuv; i++) {
      CustomDataLayer *layer = ldata->layers + base_uv_idx + i;
      cd_uvs[i] = layer->offset;
      disjount_uv_count[i] = 0;
    }
  }

  int sharp_angle_num = 0;
  do {
    BMVert *v2 = v == e->v1 ? e->v2 : e->v1;

    if (BM_ELEM_CD_GET_INT(v2, cd_vert_node_offset) != ni) {
      newflag |= SCULPTFLAG_PBVH_BOUNDARY;
    }

    if (e->l && e->l != e->l->radial_next) {
      if (BKE_pbvh_test_sharp_faces_bmesh(e->l->f, e->l->radial_next->f, sharp_angle_limit)) {
        boundflag |= SCULPT_BOUNDARY_SHARP_ANGLE;
        sharp_angle_num++;
      }
    }

    if (e->head.hflag & BM_ELEM_SEAM) {
      boundflag |= SCULPT_BOUNDARY_SEAM;
      seamcount++;

      if (seamcount > 2) {
        boundflag |= SCULPT_CORNER_SEAM;
      }
    }

#ifdef MV_COLOR_BOUNDARY
#  error "fix me! need to handle loops too"
#endif

#ifdef MV_COLOR_BOUNDARY  // CURRENTLY BROKEN
    if (cd_vcol >= 0) {
      float *color1 = BM_ELEM_CD_PTR<float *>(v, cd_vcol);
      float *color2 = BM_ELEM_CD_PTR<float *>(v2, cd_vcol);

      int colorkey1 = color_boundary_key(color1);
      int colorkey2 = color_boundary_key(color2);

      if (colorkey1 != colorkey2) {
        boundflag |= SCUKPT_BOUNDARY_FACE_SETS;
      }
    }
#endif

    if (!(e->head.hflag & BM_ELEM_SMOOTH)) {
      boundflag |= SCULPT_BOUNDARY_SHARP_MARK;
      sharpcount++;

      if (sharpcount > 2) {
        boundflag |= SCULPT_CORNER_SHARP_MARK;
      }
    }

    if (e->l) {
      /* detect uv island boundaries */
      if (do_uvs && totuv) {
        BMLoop *l_iter = e->l;
        do {
          BMLoop *l = l_iter->v != v ? l_iter->next : l_iter;

          for (int i = 0; i < totuv; i++) {
            float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uvs[i]);

            if (uv_first) {
              copy_v2_v2(lastuv[i], luv);
              copy_v2_v2(lastuv2[i], luv);

              continue;
            }

            const float uv_snap_limit = 0.01f * 0.01f;

            float dist = len_squared_v2v2(luv, lastuv[i]);
            bool same = dist <= uv_snap_limit;

            bool corner = len_squared_v2v2(lastuv[i], lastuv2[i]) > uv_snap_limit &&
                          len_squared_v2v2(lastuv[i], luv) > uv_snap_limit &&
                          len_squared_v2v2(lastuv2[i], luv) > uv_snap_limit;

            if (!same) {
              boundflag |= SCULPT_BOUNDARY_UV;
            }

            if (corner) {
              boundflag |= SCULPT_CORNER_UV;
            }

            if (!same) {
              copy_v2_v2(lastuv2[i], lastuv[i]);
              copy_v2_v2(lastuv[i], luv);
            }
          }

          uv_first = false;
        } while ((l_iter = l_iter->radial_next) != e->l);
      }

      if (BM_ELEM_CD_GET_INT(e->l->f, cd_face_node_offset) != ni) {
        newflag |= SCULPTFLAG_PBVH_BOUNDARY;
      }

      if (e->l != e->l->radial_next) {
        if (e->l->f->len > 3) {
          quadcount++;
        }

        if (e->l->radial_next->f->len > 3) {
          quadcount++;
        }

        if (BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_face_node_offset) != ni) {
          newflag |= SCULPTFLAG_PBVH_BOUNDARY;
        }
      }

      if (e->l->f->len > 3) {
        newflag |= SCULPTFLAG_NEED_TRIANGULATE;
      }

      if (cd_faceset_offset != -1) {
        int fset = BKE_pbvh_do_fset_symmetry(
            BM_ELEM_CD_GET_INT(e->l->f, cd_faceset_offset), bound_symmetry, v2->co);

        bool ok = true;
        for (int i = 0; i < fsets.size(); i++) {
          if (fsets[i] == fset) {
            ok = false;
          }
        }

        if (ok) {
          fsets.append(fset);
        }
      }

      // also check e->l->radial_next, in case we are not manifold
      // which can mess up the loop order
      if (e->l->radial_next != e->l) {
        if (cd_faceset_offset != -1) {
          // fset = abs(BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset));
          int fset2 = BKE_pbvh_do_fset_symmetry(
              BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset), bound_symmetry, v2->co);

          bool ok2 = true;
          for (int i = 0; i < fsets.size(); i++) {
            if (fsets[i] == fset2) {
              ok2 = false;
            }
          }

          if (ok2) {
            fsets.append(fset2);
          }
        }

        if (e->l->radial_next->f->len > 3) {
          newflag |= SCULPTFLAG_NEED_TRIANGULATE;
        }
      }
    }

    if (!e->l || e->l->radial_next == e->l) {
      boundflag |= SCULPT_BOUNDARY_MESH;
    }

    val++;
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (fsets.size() > 1) {
    boundflag |= SCULPT_BOUNDARY_FACE_SET;
  }

  if (fsets.size() > 2) {
    boundflag |= SCULPT_CORNER_FACE_SET;
  }

  if (sharp_angle_num > 2) {
    boundflag |= SCULPT_CORNER_SHARP_ANGLE;
  }

  if (!ELEM(sharpcount, 0, 2)) {
    boundflag |= SCULPT_CORNER_SHARP_MARK;
  }

  if (seamcount == 1) {
    boundflag |= SCULPT_CORNER_SEAM;
  }

  if ((boundflag & SCULPT_BOUNDARY_MESH) && quadcount >= 3) {
    boundflag |= SCULPT_CORNER_MESH;
  }

  /* XXX: does this need to be threadsafe? */
  BM_ELEM_CD_SET_INT(v, cd_boundary_flag, boundflag);
  *(BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag)) = newflag;
  BM_ELEM_CD_SET_INT(v, cd_valence, val);
}

bool BKE_pbvh_check_vert_boundary(PBVH *pbvh, BMVert *v)
{
  return pbvh_check_vert_boundary(pbvh, v);
}

void BKE_pbvh_sharp_limit_set(PBVH *pbvh, float limit)
{
  pbvh->sharp_angle_limit = limit;
}

/*Used by symmetrize to update boundary flags*/
void BKE_pbvh_recalc_bmesh_boundary(PBVH *pbvh)
{
  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    BKE_pbvh_update_vert_boundary(pbvh->cd_faceset_offset,
                                  pbvh->cd_vert_node_offset,
                                  pbvh->cd_face_node_offset,
                                  pbvh->cd_vcol_offset,
                                  pbvh->cd_boundary_flag,
                                  pbvh->cd_flag,
                                  pbvh->cd_valence,
                                  v,
                                  pbvh->boundary_symmetry,
                                  &pbvh->header.bm->ldata,
                                  pbvh->flags & PBVH_IGNORE_UVS ? 0 : pbvh->totuv,
                                  pbvh->flags & PBVH_IGNORE_UVS,
                                  pbvh->sharp_angle_limit);
  }
}

void BKE_pbvh_set_idmap(PBVH *pbvh, BMIdMap *idmap)
{
  pbvh->bm_idmap = idmap;
}

/* Build a PBVH from a BMesh */
void BKE_pbvh_build_bmesh(PBVH *pbvh,
                          Mesh *me,
                          BMesh *bm,
                          bool smooth_shading,
                          BMLog *log,
                          BMIdMap *idmap,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset,
                          const int cd_face_areas,
                          const int cd_boundary_flag,
                          const int /*cd_flag_offset*/,
                          const int /*cd_valence_offset*/,
                          const int cd_origco,
                          const int cd_origno,
                          bool fast_draw)
{
  // coalese_pbvh(pbvh, bm);

  pbvh->bm_idmap = idmap;

  pbvh->cd_face_area = cd_face_areas;
  pbvh->cd_vert_node_offset = cd_vert_node_offset;
  pbvh->cd_face_node_offset = cd_face_node_offset;
  pbvh->cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  pbvh->cd_boundary_flag = cd_boundary_flag;
  pbvh->cd_origco = cd_origco;
  pbvh->cd_origno = cd_origno;

  pbvh->cd_flag = CustomData_get_offset_named(
      &pbvh->header.bm->vdata, CD_PROP_INT8, ".sculpt_flags");
  pbvh->cd_valence = CustomData_get_offset_named(
      &pbvh->header.bm->vdata, CD_PROP_INT8, ".sculpt_valence");
  pbvh->cd_boundary_flag = CustomData_get_offset_named(
      &pbvh->header.bm->vdata, CD_PROP_INT8, ".sculpt_boundary_flags");

  pbvh->mesh = me;

  smooth_shading |= fast_draw;

  pbvh->header.bm = bm;

  blender::bke::dyntopo::detail_size_set(pbvh, 0.75f, 0.4f);

  pbvh->header.type = PBVH_BMESH;
  pbvh->bm_log = log;
  pbvh->cd_faceset_offset = CustomData_get_offset_named(
      &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  int tottri = poly_to_tri_count(bm->totface, bm->totloop);

  /* TODO: choose leaf limit better */
  if (tottri > 4 * 1000 * 1000) {
    pbvh->leaf_limit = 10000;
  }
  else {
    pbvh->leaf_limit = 1000;
  }

  BMIter iter;
  BMVert *v;

  if (smooth_shading) {
    pbvh->flags |= PBVH_DYNTOPO_SMOOTH_SHADING;
  }

  if (fast_draw) {
    pbvh->flags |= PBVH_FAST_DRAW;
  }

  /* bounding box array of all faces, no need to recalculate every time */
  BBC *bbc_array = MEM_cnew_array<BBC>(bm->totface, "BBC");
  BMFace **nodeinfo = MEM_cnew_array<BMFace *>(bm->totface, "nodeinfo");
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "fast PBVH node storage");

  BMFace *f;
  int i;
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    BBC *bbc = &bbc_array[i];
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    // check for currupted faceset
    if (pbvh->cd_faceset_offset != -1 && BM_ELEM_CD_GET_INT(f, pbvh->cd_faceset_offset) == 0) {
      BM_ELEM_CD_SET_INT(f, pbvh->cd_faceset_offset, 1);
    }

    BB_reset((BB *)bbc);
    do {
      BB_expand((BB *)bbc, l_iter->v->co);
    } while ((l_iter = l_iter->next) != l_first);
    BBC_update_centroid(bbc);

    /* so we can do direct lookups on 'bbc_array' */
    BM_elem_index_set(f, i); /* set_dirty! */
    nodeinfo[i] = f;
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
  /* Likely this is already dirty. */
  bm->elem_index_dirty |= BM_FACE;

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }

  /* setup root node */
  struct FastNodeBuildInfo rootnode = {0};

  Vector<FastNodeBuildInfo *> leaves;

  rootnode.totface = bm->totface;
  int totleaf = 0;

  /* start recursion, assign faces to nodes accordingly */
  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, &rootnode, leaves, arena);

  pbvh_grow_nodes(pbvh, 1);
  rootnode.node_index = 0;

  pbvh_bmesh_create_nodes_fast_recursive_create(pbvh, nodeinfo, bbc_array, &rootnode);
  totleaf = leaves.size();

  if (!totleaf) {
    leaves.append(&rootnode);
    totleaf = 1;
  }

  /* build leaf nodes */
  LeafBuilderThreadData tdata;
  tdata.pbvh = pbvh;
  tdata.nodeinfo = nodeinfo;
  tdata.bbc_array = bbc_array;
  tdata.leaves = leaves;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totleaf);
  BLI_task_parallel_range(0, totleaf, &tdata, pbvh_bmesh_create_leaf_fast_task_cb, &settings);

  /* take root node and visit and populate children recursively */
  pbvh_bmesh_create_nodes_fast_recursive_final(pbvh, nodeinfo, bbc_array, &rootnode);

  BLI_memarena_free(arena);
  MEM_freeN(bbc_array);
  MEM_freeN(nodeinfo);

  if (me) { /* Ensure pbvh->vcol_type, vcol_domain and cd_vcol_offset are up to date. */
    CustomDataLayer *cl;
    eAttrDomain domain;

    BKE_pbvh_get_color_layer(pbvh, me, &cl, &domain);
  }

  /*final check that nodes are sufficiently subdivided*/
  int totnode = pbvh->totnode;

  for (int i = 0; i < totnode; i++) {
    PBVHNode *n = pbvh->nodes + i;

    if (totnode != pbvh->totnode) {
#ifdef PROXY_ADVANCED
      BKE_pbvh_free_proxyarray(pbvh, n);
#endif
    }

    if (n->flag & PBVH_Leaf) {
      /* Recursively split nodes that have gotten too many
       * elements */
      pbvh_bmesh_node_limit_ensure(pbvh, i);
    }
  }

  pbvh_print_mem_size(pbvh);

  /* update face areas */
  const int cd_face_area = pbvh->cd_face_area;
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BKE_pbvh_bmesh_check_tris(pbvh, node);

    node->flag |= PBVH_UpdateTriAreas;
    BKE_pbvh_check_tri_areas(pbvh, node);

    int area_src_i = pbvh->face_area_i ^ 1;
    int area_dst_i = pbvh->face_area_i;

    /* make sure read side of double buffer is set too */
    TGSET_ITER (f, node->bm_faces) {
      float *areabuf = BM_ELEM_CD_PTR<float *>(f, cd_face_area);
      areabuf[area_dst_i] = areabuf[area_src_i];
    }
    TGSET_ITER_END;
  }
}

void BKE_pbvh_set_bm_log(PBVH *pbvh, BMLog *log)
{
  pbvh->bm_log = log;
  BM_log_set_idmap(log, pbvh->bm_idmap);
}

namespace blender::bke::dyntopo {
bool remesh_topology_nodes(blender::bke::dyntopo::BrushTester *brush_tester,
                           SculptSession *ss,
                           PBVH *pbvh,
                           bool (*searchcb)(PBVHNode *node, void *data),
                           void (*undopush)(PBVHNode *node, void *data),
                           void *searchdata,
                           PBVHTopologyUpdateMode mode,
                           const bool use_frontface,
                           float3 view_normal,
                           bool updatePBVH,
                           DyntopoMaskCB mask_cb,
                           void *mask_cb_data,
                           int edge_limit_multiply)
{
  bool modified = false;
  Vector<PBVHNode *> nodes;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf) || !searchcb(node, searchdata)) {
      continue;
    }

    if (node->flag & PBVH_Leaf) {
      undopush(node, searchdata);

      nodes.append(node);
    }
  }

  for (PBVHNode *node : nodes) {
    node->flag |= PBVH_UpdateCurvatureDir;
    BKE_pbvh_node_mark_topology_update(node);
  }

  modified = remesh_topology(brush_tester,
                             ss,
                             pbvh,
                             mode,
                             use_frontface,
                             view_normal,
                             updatePBVH,
                             mask_cb,
                             mask_cb_data,
                             edge_limit_multiply);

  return modified;
}
}  // namespace blender::bke::dyntopo

static void pbvh_free_tribuf(PBVHTriBuf *tribuf)
{
  MEM_SAFE_FREE(tribuf->verts);
  MEM_SAFE_FREE(tribuf->tris);
  MEM_SAFE_FREE(tribuf->loops);
  MEM_SAFE_FREE(tribuf->edges);

  BLI_smallhash_release(&tribuf->vertmap);

  tribuf->verts = nullptr;
  tribuf->tris = nullptr;
  tribuf->loops = nullptr;
  tribuf->edges = nullptr;

  tribuf->totloop = tribuf->tottri = tribuf->totedge = tribuf->totvert = 0;

  tribuf->verts_size = 0;
  tribuf->tris_size = 0;
  tribuf->edges_size = 0;
}

PBVHTriBuf *BKE_pbvh_bmesh_get_tris(PBVH *pbvh, PBVHNode *node)
{
  BKE_pbvh_bmesh_check_tris(pbvh, node);

  return node->tribuf;
}

void BKE_pbvh_bmesh_free_tris(PBVH * /*pbvh*/, PBVHNode *node)
{
  if (node->tribuf) {
    pbvh_free_tribuf(node->tribuf);
    MEM_freeN(node->tribuf);
    node->tribuf = nullptr;
  }

  if (node->tri_buffers) {
    for (int i = 0; i < node->tot_tri_buffers; i++) {
      pbvh_free_tribuf(node->tri_buffers + i);
    }

    MEM_SAFE_FREE(node->tri_buffers);

    node->tri_buffers = nullptr;
    node->tot_tri_buffers = 0;
  }
}

BLI_INLINE PBVHTri *pbvh_tribuf_add_tri(PBVHTriBuf *tribuf)
{
  tribuf->tottri++;

  if (tribuf->tottri >= tribuf->tris_size) {
    size_t newsize = (size_t)32 + (size_t)tribuf->tris_size + (size_t)(tribuf->tris_size >> 1);

    if (!tribuf->tris) {
      tribuf->tris = MEM_cnew_array<PBVHTri>(newsize, "tribuf tris");
    }
    else {
      tribuf->tris = (PBVHTri *)MEM_reallocN_id(
          (void *)tribuf->tris, sizeof(*tribuf->tris) * newsize, "tribuf tris");
    }

    tribuf->tris_size = newsize;
  }

  return tribuf->tris + tribuf->tottri - 1;
}

BLI_INLINE void pbvh_tribuf_add_vert(PBVHTriBuf *tribuf, PBVHVertRef vertex, BMLoop *l)
{
  tribuf->totvert++;
  tribuf->totloop++;

  if (tribuf->totvert >= tribuf->verts_size) {
    size_t newsize = (size_t)32 + (size_t)(tribuf->verts_size << 1);

    if (!tribuf->verts) {
      tribuf->verts = MEM_cnew_array<PBVHVertRef>(newsize, "tribuf verts");
      tribuf->loops = MEM_cnew_array<intptr_t>(newsize, "tribuf loops");
    }
    else {
      tribuf->verts = (PBVHVertRef *)MEM_reallocN_id(
          (void *)tribuf->verts, sizeof(*tribuf->verts) * newsize, "tribuf verts");
      tribuf->loops = (intptr_t *)MEM_reallocN_id(
          (void *)tribuf->loops, sizeof(*tribuf->loops) * newsize, "tribuf loops");
    }

    tribuf->verts_size = newsize;
  }

  tribuf->verts[tribuf->totvert - 1] = vertex;
  tribuf->loops[tribuf->totloop - 1] = (uintptr_t)l;
}

BLI_INLINE void pbvh_tribuf_add_edge(PBVHTriBuf *tribuf, int v1, int v2)
{
  tribuf->totedge++;

  if (tribuf->totedge >= tribuf->edges_size) {
    size_t newsize = (size_t)32 + (size_t)(tribuf->edges_size << 1);

    if (!tribuf->edges) {
      tribuf->edges = MEM_cnew_array<int>(2ULL * newsize, "tribuf edges");
    }
    else {
      tribuf->edges = (int *)MEM_reallocN_id(
          (void *)tribuf->edges, sizeof(*tribuf->edges) * 2ULL * newsize, "tribuf edges");
    }

    tribuf->edges_size = newsize;
  }

  int i = (tribuf->totedge - 1) * 2;

  tribuf->edges[i] = v1;
  tribuf->edges[i + 1] = v2;
}

void pbvh_bmesh_check_other_verts(PBVHNode *node)
{
  if (!(node->flag & PBVH_UpdateOtherVerts)) {
    return;
  }

  node->flag &= ~PBVH_UpdateOtherVerts;

  if (node->bm_other_verts) {
    BLI_table_gset_free(node->bm_other_verts, nullptr);
  }

  node->bm_other_verts = BLI_table_gset_new("bm_other_verts");
  BMFace *f;

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      if (!BLI_table_gset_haskey(node->bm_unique_verts, l->v)) {
        BLI_table_gset_add(node->bm_other_verts, l->v);
      }
    } while ((l = l->next) != f->l_first);
  }
  TGSET_ITER_END;
}

static void pbvh_init_tribuf(PBVHNode *node, PBVHTriBuf *tribuf)
{
  tribuf->tottri = 0;
  tribuf->tris_size = 0;
  tribuf->verts_size = 0;
  tribuf->mat_nr = 0;
  tribuf->tottri = 0;
  tribuf->totvert = 0;
  tribuf->totloop = 0;
  tribuf->totedge = 0;

  tribuf->edges = nullptr;
  tribuf->verts = nullptr;
  tribuf->tris = nullptr;
  tribuf->loops = nullptr;

  BLI_smallhash_init_ex(&tribuf->vertmap, node->bm_unique_verts->length);
}

static uintptr_t tri_loopkey(BMLoop *l, int mat_nr, int cd_fset, int cd_uvs[], int totuv)
{
  uintptr_t key = (uintptr_t)mat_nr;

  key ^= (uintptr_t)l->v;

  if (cd_fset >= 0) {
    // key ^= (uintptr_t)BLI_hash_int(BM_ELEM_CD_GET_INT(l->f, cd_fset));
    key ^= (uintptr_t)BM_ELEM_CD_GET_INT(l->f, cd_fset);
  }

  for (int i = 0; i < totuv; i++) {
    float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uvs[i]);
    float snap = 4196.0f;

    uintptr_t x = (uintptr_t)(luv[0] * snap);
    uintptr_t y = (uintptr_t)(luv[1] * snap);

    uintptr_t key2 = y * snap + x;
    key ^= key2;
  }

  return key;
}
/* In order to perform operations on the original node coordinates
 * (currently just raycast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden. */
bool BKE_pbvh_bmesh_check_tris(PBVH *pbvh, PBVHNode *node)
{
  BMesh *bm = pbvh->header.bm;

  if (!(node->flag & PBVH_UpdateTris) && node->tribuf) {
    return false;
  }

  int totuv = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
  int *cd_uvs = (int *)BLI_array_alloca(cd_uvs, totuv);

  for (int i = 0; i < totuv; i++) {
    int idx = CustomData_get_layer_index_n(&bm->ldata, CD_PROP_FLOAT2, i);
    cd_uvs[i] = bm->ldata.layers[idx].offset;
  }

  node->flag |= PBVH_UpdateOtherVerts;

  int mat_map[MAXMAT];

  for (int i = 0; i < MAXMAT; i++) {
    mat_map[i] = -1;
  }

  if (node->tribuf || node->tri_buffers) {
    BKE_pbvh_bmesh_free_tris(pbvh, node);
  }

  node->tribuf = MEM_cnew<PBVHTriBuf>("node->tribuf");
  pbvh_init_tribuf(node, node->tribuf);

  Vector<BMLoop *, 128> loops;
  Vector<blender::uint3, 128> loops_idx;
  Vector<PBVHTriBuf> tribufs;

  node->flag &= ~PBVH_UpdateTris;

  const int edgeflag = BM_ELEM_TAG_ALT;

  BMFace *f;

  float min[3], max[3];

  INIT_MINMAX(min, max);

  TGSET_ITER (f, node->bm_faces) {
    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    /* Clear edgeflag for building edge indices later. */
    BMLoop *l = f->l_first;
    do {
      l->e->head.hflag &= ~edgeflag;
    } while ((l = l->next) != f->l_first);

    const int mat_nr = f->mat_nr;

    if (mat_map[mat_nr] == -1) {
      PBVHTriBuf _tribuf = {0};

      mat_map[mat_nr] = tribufs.size();

      pbvh_init_tribuf(node, &_tribuf);
      _tribuf.mat_nr = mat_nr;
      tribufs.append(_tribuf);
    }

#ifdef DYNTOPO_DYNAMIC_TESS
    int tottri = (f->len - 2);

    loops.resize(f->len);
    loops_idx.resize(tottri);

    BM_face_calc_tessellation(f, true, loops.data(), (uint(*)[3])loops_idx.data());

    for (int i = 0; i < tottri; i++) {
      PBVHTri *tri = pbvh_tribuf_add_tri(node->tribuf);
      PBVHTriBuf *mat_tribuf = &tribufs[mat_map[mat_nr]];
      PBVHTri *mat_tri = pbvh_tribuf_add_tri(mat_tribuf);

      tri->eflag = mat_tri->eflag = 0;

      for (int j = 0; j < 3; j++) {
        // BMLoop *l0 = loops[loops_idx[i][(j + 2) % 3]];
        BMLoop *l = loops[loops_idx[i][j]];
        BMLoop *l2 = loops[loops_idx[i][(j + 1) % 3]];

        void **val = nullptr;
        BMEdge *e = BM_edge_exists(l->v, l2->v);

        if (e) {
          tri->eflag |= 1 << j;
          mat_tri->eflag |= 1 << j;
        }

        uintptr_t loopkey = tri_loopkey(l, mat_nr, pbvh->cd_faceset_offset, cd_uvs, totuv);

        if (!BLI_smallhash_ensure_p(&node->tribuf->vertmap, loopkey, &val)) {
          PBVHVertRef sv = {(intptr_t)l->v};

          minmax_v3v3_v3(min, max, l->v->co);

          *val = POINTER_FROM_INT(node->tribuf->totvert);
          pbvh_tribuf_add_vert(node->tribuf, sv, l);
        }

        tri->v[j] = (intptr_t)val[0];
        tri->l[j] = (intptr_t)l;

        val = nullptr;
        if (!BLI_smallhash_ensure_p(&mat_tribuf->vertmap, loopkey, &val)) {
          PBVHVertRef sv = {(intptr_t)l->v};

          minmax_v3v3_v3(min, max, l->v->co);

          *val = POINTER_FROM_INT(mat_tribuf->totvert);
          pbvh_tribuf_add_vert(mat_tribuf, sv, l);
        }

        mat_tri->v[j] = (intptr_t)val[0];
        mat_tri->l[j] = (intptr_t)l;
      }

      copy_v3_v3(tri->no, f->no);
      copy_v3_v3(mat_tri->no, f->no);
      tri->f.i = (intptr_t)f;
      mat_tri->f.i = (intptr_t)f;
    }
#else
    PBVHTri *tri = pbvh_tribuf_add_tri(node->tribuf);
    PBVHTriBuf *mat_tribuf = tribufs + mat_map[mat_nr];
    PBVHTri *mat_tri = pbvh_tribuf_add_tri(mat_tribuf);

    BMLoop *l = f->l_first;
    int j = 0;

    do {
      void **val = nullptr;

      if (!BLI_ghash_ensure_p(vmap, l->v, &val)) {
        PBVHVertRef sv = {(intptr_t)l->v};

        minmax_v3v3_v3(min, max, l->v->co);

        *val = (void *)node->tribuf->totvert;
        pbvh_tribuf_add_vert(node->tribuf, sv);
      }

      tri->v[j] = (intptr_t)val[0];
      tri->l[j] = (intptr_t)l;

      val = nullptr;
      if (!BLI_ghash_ensure_p(mat_vmaps[mat_nr], l->v, &val)) {
        PBVHVertRef sv = {(intptr_t)l->v};

        minmax_v3v3_v3(min, max, l->v->co);

        *val = (void *)mat_tribuf->totvert;
        pbvh_tribuf_add_vert(mat_tribuf, sv);
      }

      mat_tri->v[j] = (intptr_t)val[0];
      mat_tri->l[j] = (intptr_t)l;

      j++;

      if (j >= 3) {
        break;
      }

      l = l->next;
    } while (l != f->l_first);

    copy_v3_v3(tri->no, f->no);
    tri->f.i = (intptr_t)f;
#endif
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    int mat_nr = f->mat_nr;
    PBVHTriBuf *mat_tribuf = &tribufs[mat_map[mat_nr]];

    BMLoop *l = f->l_first;
    do {
      if (l->e->head.hflag & edgeflag) {
        continue;
      }

      l->e->head.hflag |= edgeflag;

      int v1 = POINTER_AS_INT(BLI_smallhash_lookup(&node->tribuf->vertmap, (uintptr_t)l->e->v1));
      int v2 = POINTER_AS_INT(BLI_smallhash_lookup(&node->tribuf->vertmap, (uintptr_t)l->e->v2));

      pbvh_tribuf_add_edge(node->tribuf, v1, v2);

      v1 = POINTER_AS_INT(BLI_smallhash_lookup(&mat_tribuf->vertmap, (uintptr_t)l->e->v1));
      v2 = POINTER_AS_INT(BLI_smallhash_lookup(&mat_tribuf->vertmap, (uintptr_t)l->e->v2));

      pbvh_tribuf_add_edge(mat_tribuf, v1, v2);
    } while ((l = l->next) != f->l_first);
  }
  TGSET_ITER_END

  bm->elem_index_dirty |= BM_VERT;

  node->tri_buffers = c_array_from_vector<PBVHTriBuf>(tribufs);
  node->tot_tri_buffers = tribufs.size();

  if (node->tribuf->totvert) {
    copy_v3_v3(node->tribuf->min, min);
    copy_v3_v3(node->tribuf->max, max);
  }
  else {
    zero_v3(node->tribuf->min);
    zero_v3(node->tribuf->max);
  }

  return true;
}

static int pbvh_count_subtree_verts(PBVH *pbvh, PBVHNode *n)
{
  if (n->flag & PBVH_Leaf) {
    n->subtree_tottri = BLI_table_gset_len(
        n->bm_faces);  // n->tm_unique_verts->length + n->tm_other_verts->length;
    return n->subtree_tottri;
  }

  int ni = n->children_offset;

  int ret = pbvh_count_subtree_verts(pbvh, pbvh->nodes + ni);
  ret += pbvh_count_subtree_verts(pbvh, pbvh->nodes + ni + 1);

  n->subtree_tottri = ret;

  return ret;
}

void BKE_pbvh_bmesh_flag_all_disk_sort(PBVH *pbvh)
{
  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_DISK_SORT);
  }
}

void BKE_pbvh_bmesh_update_all_valence(PBVH *pbvh)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    BKE_pbvh_bmesh_update_valence(pbvh, BKE_pbvh_make_vref((intptr_t)v));
  }
}

void BKE_pbvh_bmesh_on_mesh_change(PBVH *pbvh)
{
  BMIter iter;
  BMVert *v;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (node->flag & PBVH_Leaf) {
      node->flag |= PBVH_UpdateTriAreas | PBVH_UpdateCurvatureDir;
    }
  }

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
    *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

    dyntopo_add_flag(pbvh,
                     v,
                     SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_TRIANGULATE |
                         SCULPTFLAG_NEED_VALENCE);
    BKE_pbvh_bmesh_update_valence(pbvh, BKE_pbvh_make_vref((intptr_t)v));
  }
}

bool BKE_pbvh_bmesh_mark_update_valence(PBVH *pbvh, PBVHVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;

  bool ret = (*BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag)) & SCULPTFLAG_NEED_VALENCE;
  dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);

  return ret;
}

bool BKE_pbvh_bmesh_check_valence(PBVH *pbvh, PBVHVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;

  if (*BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag) & SCULPTFLAG_NEED_VALENCE) {
    BKE_pbvh_bmesh_update_valence(pbvh, vertex);
    return true;
  }

  return false;
}

void BKE_pbvh_bmesh_update_valence(PBVH *pbvh, PBVHVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  BMEdge *e;

  uint8_t *flag = BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag);
  uint *valence = BM_ELEM_CD_PTR<uint *>(v, pbvh->cd_valence);

  *flag &= ~SCULPTFLAG_NEED_VALENCE;

  if (!v->e) {
    *valence = 0;
    return;
  }

  *valence = 0;

  e = v->e;

  if (!e) {
    return;
  }

  do {
    (*valence)++;

    e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;

    if (!e) {
      printf("bmesh error!\n");
      break;
    }
  } while (e != v->e);
}

static void pbvh_bmesh_join_subnodes(PBVH *pbvh, PBVHNode *node, PBVHNode *parent)
{
  if (!(node->flag & PBVH_Leaf)) {
    int ni = node->children_offset;

    if (ni > 0 && ni < pbvh->totnode - 1) {
      pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + ni, parent);
      pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + ni + 1, parent);
    }
    else {
      printf("node corruption: %d\n", ni);
      return;
    }
    if (node != parent) {
      node->flag |= PBVH_Delete; /* Mark for deletion. */
    }

    return;
  }

  if (node != parent) {
    node->flag |= PBVH_Delete; /* Mark for deletion. */
  }

  BMVert *v;

  TGSET_ITER (v, node->bm_unique_verts) {
    BLI_table_gset_add(parent->bm_unique_verts, v);

    int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
    *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

    BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }
  TGSET_ITER_END

  // printf("  subtotface: %d\n", BLI_table_gset_len(node->bm_faces));

  BMFace *f;
  TGSET_ITER (f, node->bm_faces) {
    BLI_table_gset_add(parent->bm_faces, f);
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
  TGSET_ITER_END
}

static void BKE_pbvh_bmesh_correct_tree(PBVH *pbvh, PBVHNode *node, PBVHNode * /*parent*/)
{
  const int size_lower = pbvh->leaf_limit - (pbvh->leaf_limit >> 1);

  if (node->flag & PBVH_Leaf) {
    return;
  }

  if (node->subtree_tottri < size_lower && node != pbvh->nodes) {
    node->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
    node->bm_other_verts = BLI_table_gset_new("bm_other_verts");
    node->bm_faces = BLI_table_gset_new("bm_faces");

    pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + node->children_offset, node);
    pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + node->children_offset + 1, node);

    node->children_offset = 0;
    node->flag |= PBVH_Leaf | PBVH_UpdateRedraw | PBVH_UpdateBB | PBVH_UpdateDrawBuffers |
                  PBVH_RebuildDrawBuffers | PBVH_UpdateOriginalBB | PBVH_UpdateMask |
                  PBVH_UpdateVisibility | PBVH_UpdateColor | PBVH_UpdateNormals | PBVH_UpdateTris;

    TableGSet *other = BLI_table_gset_new(__func__);
    BMVert *v;

    node->children_offset = 0;
    node->draw_batches = nullptr;

    // rebuild bm_other_verts
    BMFace *f;
    TGSET_ITER (f, node->bm_faces) {
      BMLoop *l = f->l_first;

      if (BM_elem_is_free((BMElem *)f, BM_FACE)) {
        printf("%s: corrupted face %p.\n", __func__, f);
        BLI_table_gset_remove(node->bm_faces, f, nullptr);
        continue;
      }

      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

      do {
        if (!BLI_table_gset_haskey(node->bm_unique_verts, l->v)) {
          BLI_table_gset_add(other, l->v);
        }
        l = l->next;
      } while (l != f->l_first);
    }
    TGSET_ITER_END

    BLI_table_gset_free(node->bm_other_verts, nullptr);
    node->bm_other_verts = other;

    BB_reset(&node->vb);

#if 1
    TGSET_ITER (v, node->bm_unique_verts) {
      BB_expand(&node->vb, v->co);
    }
    TGSET_ITER_END

    TGSET_ITER (v, node->bm_other_verts) {
      BB_expand(&node->vb, v->co);
    }
    TGSET_ITER_END
#endif

    // printf("totface: %d\n", BLI_table_gset_len(node->bm_faces));
    node->orig_vb = node->vb;

    return;
  }

  int ni = node->children_offset;

  for (int i = 0; i < 2; i++, ni++) {
    PBVHNode *child = pbvh->nodes + ni;
    BKE_pbvh_bmesh_correct_tree(pbvh, child, node);
  }
}

/* Deletes PBVH_Delete marked nodes. */
static void pbvh_bmesh_compact_tree(PBVH *bvh)
{
  // compact nodes
  int totnode = 0;
  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Delete)) {
      if (!(n->flag & PBVH_Leaf)) {
        PBVHNode *n1 = bvh->nodes + n->children_offset;
        PBVHNode *n2 = bvh->nodes + n->children_offset + 1;

        if ((n1->flag & PBVH_Delete) != (n2->flag & PBVH_Delete)) {
          printf("un-deleting an empty node\n");
          PBVHNode *n3 = n1->flag & PBVH_Delete ? n1 : n2;

          n3->flag = PBVH_Leaf | PBVH_UpdateTris;
          n3->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
          n3->bm_other_verts = BLI_table_gset_new("bm_other_verts");
          n3->bm_faces = BLI_table_gset_new("bm_faces");
          n3->tribuf = nullptr;
          n3->draw_batches = nullptr;
        }
        else if ((n1->flag & PBVH_Delete) && (n2->flag & PBVH_Delete)) {
          n->children_offset = 0;
          n->flag |= PBVH_Leaf | PBVH_UpdateTris;

          if (!n->bm_unique_verts) {
            // should not happen
            n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
            n->bm_other_verts = BLI_table_gset_new("bm_other_verts");
            n->bm_faces = BLI_table_gset_new("bm_faces");
            n->tribuf = nullptr;
            n->draw_batches = nullptr;
          }
        }
      }

      totnode++;
    }
  }

  int *map = MEM_cnew_array<int>(bvh->totnode, "bmesh map temp");

  // build idx map for child offsets
  int j = 0;
  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Delete)) {
      map[i] = j++;
    }
    else if (1) {
      if (n->layer_disp) {
        MEM_freeN(n->layer_disp);
        n->layer_disp = nullptr;
      }

      pbvh_free_draw_buffers(bvh, n);

      if (n->vert_indices) {
        MEM_freeN((void *)n->vert_indices);
        n->vert_indices = nullptr;
      }
      if (n->face_vert_indices) {
        MEM_freeN((void *)n->face_vert_indices);
        n->face_vert_indices = nullptr;
      }

      if (n->tribuf || n->tri_buffers) {
        BKE_pbvh_bmesh_free_tris(bvh, n);
      }

      if (n->bm_unique_verts) {
        BLI_table_gset_free(n->bm_unique_verts, nullptr);
        n->bm_unique_verts = nullptr;
      }

      if (n->bm_other_verts) {
        BLI_table_gset_free(n->bm_other_verts, nullptr);
        n->bm_other_verts = nullptr;
      }

      if (n->bm_faces) {
        BLI_table_gset_free(n->bm_faces, nullptr);
        n->bm_faces = nullptr;
      }

#ifdef PROXY_ADVANCED
      BKE_pbvh_free_proxyarray(bvh, n);
#endif
    }
  }

  // compact node array
  j = 0;
  for (int i = 0; i < bvh->totnode; i++) {
    if (!(bvh->nodes[i].flag & PBVH_Delete)) {
      if (bvh->nodes[i].children_offset >= bvh->totnode - 1) {
        printf("error %i %i\n", i, bvh->nodes[i].children_offset);
        continue;
      }

      int i1 = map[bvh->nodes[i].children_offset];
      int i2 = map[bvh->nodes[i].children_offset + 1];

      if (bvh->nodes[i].children_offset >= bvh->totnode) {
        printf("bad child node reference %d->%d, totnode: %d\n",
               i,
               bvh->nodes[i].children_offset,
               bvh->totnode);
        continue;
      }

      if (bvh->nodes[i].children_offset && i2 != i1 + 1) {
        printf("      pbvh corruption during node join %d %d\n", i1, i2);
      }

      bvh->nodes[j] = bvh->nodes[i];
      bvh->nodes[j].children_offset = i1;

      j++;
    }
  }

  if (j != totnode) {
    printf("pbvh error: %s", __func__);
  }

  if (bvh->totnode != j) {
    memset(bvh->nodes + j, 0, sizeof(*bvh->nodes) * (bvh->totnode - j));
    bvh->node_mem_count = j;
  }

  bvh->totnode = j;

  // set vert/face node indices again
  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    if (!n->bm_unique_verts) {
      printf("ERROR!\n");
      n->bm_unique_verts = BLI_table_gset_new("bleh");
      n->bm_other_verts = BLI_table_gset_new("bleh");
      n->bm_faces = BLI_table_gset_new("bleh");
    }

    BMVert *v;

    TGSET_ITER (v, n->bm_unique_verts) {
      BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    BMFace *f;

    TGSET_ITER (f, n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, bvh->cd_face_node_offset, i);
    }
    TGSET_ITER_END
  }

  Vector<BMVert *> scratch;

  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    scratch.clear();
    BMVert *v;

    TGSET_ITER (v, n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        scratch.append(v);
      }
      // BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    int slen = scratch.size();
    for (int j = 0; j < slen; j++) {
      BMVert *v = scratch[j];

      BLI_table_gset_remove(n->bm_other_verts, v, nullptr);
      BLI_table_gset_add(n->bm_unique_verts, v);
      BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
  }

  MEM_freeN(map);
}

/* Prunes leaf nodes that are too small or degenerate. */
static void pbvh_bmesh_balance_tree(PBVH *pbvh)
{
  float *overlaps = MEM_cnew_array<float>(pbvh->totnode, "overlaps");
  PBVHNode **parentmap = MEM_cnew_array<PBVHNode *>(pbvh->totnode, "parentmap");
  int *depthmap = MEM_cnew_array<int>(pbvh->totnode, "depthmap");

  Vector<PBVHNode *> stack;
  Vector<BMFace *> faces;
  Vector<PBVHNode *> substack;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if ((node->flag & PBVH_Leaf) || node->children_offset == 0) {
      continue;
    }

    if (node->children_offset < pbvh->totnode) {
      parentmap[node->children_offset] = node;
    }

    if (node->children_offset + 1 < pbvh->totnode) {
      parentmap[node->children_offset + 1] = node;
    }
  }

  const int cd_vert_node = pbvh->cd_vert_node_offset;
  const int cd_face_node = pbvh->cd_face_node_offset;

  bool modified = false;

  stack.append(pbvh->nodes);

  while (stack.size() > 0) {
    PBVHNode *node = stack.pop_last();
    BB clip;

    if (!(node->flag & PBVH_Leaf) && node->children_offset > 0) {
      PBVHNode *child1 = pbvh->nodes + node->children_offset;
      PBVHNode *child2 = pbvh->nodes + node->children_offset + 1;

      float volume = BB_volume(&child1->vb) + BB_volume(&child2->vb);

      /* dissolve nodes whose children overlap by more then a percentage
        of the total volume.  we use a simple huerstic to calculate the
        cutoff threshold.*/

      BB_intersect(&clip, &child1->vb, &child2->vb);
      float overlap = BB_volume(&clip);
      float factor;

      /* use higher threshold for the root node and its immediate children */
      switch (stack.size()) {
        case 0:
          factor = 0.5;
          break;
        case 1:
        case 2:
          factor = 0.2;
          break;
        default:
          factor = 0.2;
          break;
      }

#if 0
      for (int k = 0; k < stack.size(); k++) {
        printf(" ");
      }

      printf("factor: %.3f\n", factor);
#endif

      bool bad = overlap > volume * factor;

      bad |= child1->bm_faces && !BLI_table_gset_len(child1->bm_faces);
      bad |= child2->bm_faces && !BLI_table_gset_len(child2->bm_faces);

      if (bad) {
        modified = true;

        substack.clear();
        substack.append(child1);
        substack.append(child2);

        while (substack.size() > 0) {
          PBVHNode *node2 = substack.pop_last();

          node2->flag |= PBVH_Delete;

          if (node2->flag & PBVH_Leaf) {
            BMFace *f;
            BMVert *v;

            TGSET_ITER (f, node2->bm_faces) {
              if (BM_ELEM_CD_GET_INT(f, cd_face_node) == -1) {
                // eek!
                continue;
              }

              BM_ELEM_CD_SET_INT(f, cd_face_node, DYNTOPO_NODE_NONE);
              faces.append(f);
            }
            TGSET_ITER_END;

            TGSET_ITER (v, node2->bm_unique_verts) {
              int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
              *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

              BM_ELEM_CD_SET_INT(v, cd_vert_node, DYNTOPO_NODE_NONE);
            }
            TGSET_ITER_END;
          }
          else if (node2->children_offset > 0 && node2->children_offset < pbvh->totnode) {
            substack.append(pbvh->nodes + node2->children_offset);

            if (node2->children_offset + 1 < pbvh->totnode) {
              substack.append(pbvh->nodes + node2->children_offset + 1);
            }
          }
        }
      }

      if (node->children_offset < pbvh->totnode) {
        stack.append(child1);
      }

      if (node->children_offset + 1 < pbvh->totnode) {
        stack.append(child2);
      }
    }
  }

  if (modified) {
    pbvh_bmesh_compact_tree(pbvh);

    printf("joined nodes; %d faces\n", (int)faces.size());

    for (int i : IndexRange(faces.size())) {
      if (BM_elem_is_free((BMElem *)faces[i], BM_FACE)) {
        printf("corrupted face in pbvh tree; faces[i]: %p\n", faces[i]);
        continue;
      }

      if (BM_ELEM_CD_GET_INT(faces[i], cd_face_node) != DYNTOPO_NODE_NONE) {
        // printf("duplicate faces in pbvh_bmesh_balance_tree!\n");
        continue;
      }

      bke_pbvh_insert_face(pbvh, faces[i]);
    }
  }

  MEM_SAFE_FREE(parentmap);
  MEM_SAFE_FREE(overlaps);
  MEM_SAFE_FREE(depthmap);
}

/* Fix any orphaned empty leaves that survived other stages of culling.*/
static void pbvh_fix_orphan_leaves(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf) || (node->flag & PBVH_Delete) ||
        BLI_table_gset_len(node->bm_faces) != 0)
    {
      continue;
    }

    /* Find parent node. */
    PBVHNode *parent = nullptr;

    for (int j = 0; j < pbvh->totnode; j++) {
      if (pbvh->nodes[j].children_offset == i || pbvh->nodes[j].children_offset + 1 == i) {
        parent = pbvh->nodes + j;
        break;
      }
    }

    if (!parent) {
      printf("%s: node corruption, could not find parent node!\n", __func__);
      continue;
    }

    PBVHNode *other = pbvh->nodes + (parent->children_offset == i ? i + 1 : i - 1);

    if (other->flag & PBVH_Delete) {
      printf("%s: error!\n", __func__);
      continue;
    }

    while (!(other->flag & PBVH_Leaf)) {
      PBVHNode *a = pbvh->nodes + other->children_offset;
      PBVHNode *b = pbvh->nodes + other->children_offset + 1;

      if (!(a->flag & PBVH_Delete) && (a->flag & PBVH_Leaf) && BLI_table_gset_len(a->bm_faces) > 1)
      {
        other = a;
      }
      else if (!(b->flag & PBVH_Delete) && (b->flag & PBVH_Leaf) &&
               BLI_table_gset_len(b->bm_faces) > 1) {
        other = b;
      }
      else {
        other = nullptr;
        break;
      }
    }

    if (other == nullptr || BLI_table_gset_len(other->bm_faces) < 1) {
      printf("%s: other was nullptr\n", __func__);
      continue;
    }

    /* Steal a single face from other */
    BMFace *f;
    PBVHNodeFlags updateflag = PBVH_UpdateOtherVerts | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                               PBVH_UpdateTris | PBVH_UpdateTriAreas | PBVH_RebuildDrawBuffers |
                               PBVH_RebuildNodeVerts | PBVH_RebuildPixels | PBVH_UpdateNormals |
                               PBVH_UpdateCurvatureDir | PBVH_UpdateRedraw | PBVH_UpdateVisibility;

    TGSET_ITER (f, other->bm_faces) {
      BLI_table_gset_remove(other->bm_faces, static_cast<void *>(f), nullptr);
      BLI_table_gset_add(node->bm_faces, static_cast<void *>(f));
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);

      BMVert *v = f->l_first->v;
      int node_i = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
      if (node_i != DYNTOPO_NODE_NONE) {
        PBVHNode *node = pbvh->nodes + node_i;
        if (BLI_table_gset_haskey(node->bm_unique_verts, static_cast<void *>(v))) {
          BLI_table_gset_remove(node->bm_unique_verts, static_cast<void *>(v), nullptr);
        }
      }

      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
      BLI_table_gset_add(node->bm_unique_verts, v);

      node->flag |= updateflag;
      other->flag |= updateflag;

      printf("%s: Patched empty leaf node.\n", __func__);
      break;
    }
    TGSET_ITER_END;
  }
}

static void pbvh_bmesh_join_nodes(PBVH *pbvh)
{
  if (pbvh->totnode < 2) {
    return;
  }

  pbvh_count_subtree_verts(pbvh, pbvh->nodes);
  BKE_pbvh_bmesh_correct_tree(pbvh, pbvh->nodes, nullptr);
  pbvh_fix_orphan_leaves(pbvh);

  /* Compact nodes. */
  int totnode = 0;
  int *map = MEM_cnew_array<int>(pbvh->totnode, "bmesh map temp");

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < pbvh->totnode; j++) {
      if (i == j || !pbvh->nodes[i].draw_batches) {
        continue;
      }

      if (pbvh->nodes[i].draw_batches == pbvh->nodes[j].draw_batches) {
        printf("%s: error %d %d\n", __func__, i, j);

        pbvh->nodes[j].draw_batches = nullptr;
      }
    }
  }

  /* Build index map for child offsets. */
  int j = 0;
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *n = pbvh->nodes + i;

    if (!(n->flag & PBVH_Delete)) {
      map[i] = j++;
      totnode++;
    }
    else {
      if (n->layer_disp) {
        MEM_freeN(n->layer_disp);
        n->layer_disp = nullptr;
      }

      pbvh_free_draw_buffers(pbvh, n);

      if (n->vert_indices) {
        MEM_freeN((void *)n->vert_indices);
        n->vert_indices = nullptr;
      }
      if (n->face_vert_indices) {
        MEM_freeN((void *)n->face_vert_indices);
        n->face_vert_indices = nullptr;
      }

      if (n->tribuf || n->tri_buffers) {
        BKE_pbvh_bmesh_free_tris(pbvh, n);
      }

      if (n->bm_unique_verts) {
        BLI_table_gset_free(n->bm_unique_verts, nullptr);
        n->bm_unique_verts = nullptr;
      }

      if (n->bm_other_verts) {
        BLI_table_gset_free(n->bm_other_verts, nullptr);
        n->bm_other_verts = nullptr;
      }

      if (n->bm_faces) {
        BLI_table_gset_free(n->bm_faces, nullptr);
        n->bm_faces = nullptr;
      }

#ifdef PROXY_ADVANCED
      BKE_pbvh_free_proxyarray(pbvh, n);
#endif
    }
  }

  // compact node array
  j = 0;
  for (int i = 0; i < pbvh->totnode; i++) {
    if (!(pbvh->nodes[i].flag & PBVH_Delete)) {
      if (pbvh->nodes[i].children_offset >= pbvh->totnode - 1) {
        printf("%s: error %i %i\n", __func__, i, pbvh->nodes[i].children_offset);
        continue;
      }

      int i1 = map[pbvh->nodes[i].children_offset];
      int i2 = map[pbvh->nodes[i].children_offset + 1];

      if (pbvh->nodes[i].children_offset >= pbvh->totnode) {
        printf("%s: Bad child node reference %d->%d, totnode: %d\n",
               __func__,
               i,
               pbvh->nodes[i].children_offset,
               pbvh->totnode);
        continue;
      }

      if (pbvh->nodes[i].children_offset && i2 != i1 + 1) {
        printf("      pbvh corruption during node join %d %d\n", i1, i2);
      }

      pbvh->nodes[j] = pbvh->nodes[i];
      pbvh->nodes[j].children_offset = i1;

      j++;
    }
  }

  if (j != totnode) {
    printf("%s: pbvh error.\n", __func__);
  }

  if (pbvh->totnode != j) {
    memset(pbvh->nodes + j, 0, sizeof(*pbvh->nodes) * (pbvh->totnode - j));
    pbvh->node_mem_count = j;
  }

  pbvh->totnode = j;

  // set vert/face node indices again
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *n = pbvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    if (!n->bm_unique_verts) {
      printf("%s: ERROR!\n", __func__);
      n->bm_unique_verts = BLI_table_gset_new("bleh");
      n->bm_other_verts = BLI_table_gset_new("bleh");
      n->bm_faces = BLI_table_gset_new("bleh");
    }

    BMVert *v;

    TGSET_ITER (v, n->bm_unique_verts) {
      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    BMFace *f;

    TGSET_ITER (f, n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);
    }
    TGSET_ITER_END
  }

  Vector<BMVert *> scratch;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *n = pbvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    scratch.clear();
    BMVert *v;

    TGSET_ITER (v, n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        scratch.append(v);
      }
      // BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    for (int j : IndexRange(scratch.size())) {
      BMVert *v = scratch[j];

      BLI_table_gset_remove(n->bm_other_verts, v, nullptr);
      BLI_table_gset_add(n->bm_unique_verts, v);
      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
    }
  }

  MEM_freeN(map);
}

namespace blender::bke::dyntopo {
void after_stroke(PBVH *pbvh, bool force_balance)
{
  int totnode = pbvh->totnode;

  BKE_pbvh_update_bounds(pbvh, (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw));

  pbvh_bmesh_check_nodes(pbvh);
  pbvh_bmesh_join_nodes(pbvh);
  pbvh_bmesh_check_nodes(pbvh);

  BKE_pbvh_update_bounds(pbvh, (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw));

  if (force_balance || pbvh->balance_counter++ == 10) {
    pbvh_bmesh_balance_tree(pbvh);
    pbvh_bmesh_check_nodes(pbvh);
    pbvh->balance_counter = 0;

    totnode = pbvh->totnode;

    for (int i = 0; i < totnode; i++) {
      PBVHNode *n = pbvh->nodes + i;

      if (totnode != pbvh->totnode) {
#ifdef PROXY_ADVANCED
        BKE_pbvh_free_proxyarray(pbvh, n);
#endif
      }

      if (n->flag & PBVH_Leaf) {
        /* Recursively split nodes that have gotten too many
         * elements */
        pbvh_bmesh_node_limit_ensure(pbvh, i);
      }
    }
  }

  pbvh_print_mem_size(pbvh);
}
}  // namespace blender::bke::dyntopo

void BKE_pbvh_node_mark_topology_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateTopology;
}

TableGSet *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node)
{
  return node->bm_unique_verts;
}

TableGSet *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node)
{
  pbvh_bmesh_check_other_verts(node);
  return node->bm_other_verts;
}

struct TableGSet *BKE_pbvh_bmesh_node_faces(PBVHNode *node)
{
  return node->bm_faces;
}

/****************************** Debugging *****************************/

void BKE_pbvh_update_offsets(PBVH *pbvh,
                             const int cd_vert_node_offset,
                             const int cd_face_node_offset,
                             const int cd_face_areas,
                             const int cd_boundary_flag,
                             const int cd_flag,
                             const int cd_valence,
                             const int cd_origco,
                             const int cd_origno,
                             const int cd_curvature_dir)
{
  pbvh->cd_face_node_offset = cd_face_node_offset;
  pbvh->cd_vert_node_offset = cd_vert_node_offset;
  pbvh->cd_face_area = cd_face_areas;
  pbvh->cd_vert_mask_offset = CustomData_get_offset(&pbvh->header.bm->vdata, CD_PAINT_MASK);
  pbvh->cd_faceset_offset = CustomData_get_offset_named(
      &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  pbvh->totuv = CustomData_number_of_layers(&pbvh->header.bm->ldata, CD_PROP_FLOAT2);
  pbvh->cd_boundary_flag = cd_boundary_flag;
  pbvh->cd_curvature_dir = cd_curvature_dir;

  if (pbvh->bm_idmap) {
    BM_idmap_check_attributes(pbvh->bm_idmap);
  }

  pbvh->cd_flag = cd_flag;
  pbvh->cd_valence = cd_valence;
  pbvh->cd_origco = cd_origco;
  pbvh->cd_origno = cd_origno;
}

#define MAX_RE_CHILD 3
struct ReVertNode {
  int totvert, totchild;
  struct ReVertNode *parent;
  struct ReVertNode *children[MAX_RE_CHILD];
  BMVert *verts[];
};

BMesh *BKE_pbvh_reorder_bmesh(PBVH *pbvh)
{
  /*try to compute size of verts per node*/
  int vsize = sizeof(BMVert);
  vsize += pbvh->header.bm->vdata.totsize;

  // perhaps aim for l2 cache?
  const int limit = 1024;
  int leaf_limit = MAX2(limit / vsize, 4);

  BLI_mempool *pool = BLI_mempool_create(sizeof(ReVertNode) + sizeof(void *) * vsize, 0, 8192, 0);
  ReVertNode **vnodemap = (ReVertNode **)MEM_calloc_arrayN(
      pbvh->header.bm->totvert, sizeof(void *), "vnodemap");

  printf("leaf_limit: %d\n", leaf_limit);

  BMIter iter;
  BMVert *v;
  const char flag = BM_ELEM_TAG_ALT;
  int i = 0;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    v->head.hflag &= ~flag;
    v->head.index = i++;
  }

  Vector<BMVert *> stack;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    if (v->head.hflag & flag) {
      continue;
    }

    ReVertNode *node = (ReVertNode *)BLI_mempool_calloc(pool);

    stack.clear();
    stack.append(v);

    v->head.hflag |= flag;

    vnodemap[v->head.index] = node;
    node->verts[node->totvert++] = v;

    while (stack.size() > 0) {
      BMVert *v2 = stack.pop_last();
      BMEdge *e;

      if (node->totvert >= leaf_limit) {
        break;
      }

      if (!v2->e) {
        continue;
      }

      int len = node->totvert;

      e = v2->e;
      do {
        BMVert *v3 = BM_edge_other_vert(e, v2);

        if (!BM_elem_flag_test(v3, flag) && len < leaf_limit) {
          v3->head.hflag |= flag;

          vnodemap[v3->head.index] = node;
          node->verts[node->totvert++] = v3;

          len++;

          stack.append(v3);
        }

        e = e->v1 == v2 ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v2->e);
    }
  }

  const int steps = 4;
  Vector<ReVertNode *> roots;

  for (int step = 0; step < steps; step++) {
    const bool last_step = step == steps - 1;

    BM_ITER_MESH_INDEX (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH, i) {
      BMEdge *e = v->e;

      if (!e) {
        continue;
      }

      ReVertNode *node = vnodemap[v->head.index];
      if (node->parent) {
        continue;
      }

      ReVertNode *parent = (ReVertNode *)BLI_mempool_calloc(pool);
      parent->children[0] = node;
      parent->totchild = 1;

      do {
        BMVert *v2 = BM_edge_other_vert(e, v);

        ReVertNode *node2 = vnodemap[v2->head.index];

        bool ok = node != node2 && !node2->parent;
        ok = ok && parent->totchild < MAX_RE_CHILD;

        for (int j = 0; j < parent->totchild; j++) {
          if (parent->children[j] == node2) {
            ok = false;
            break;
          }
        }

        if (ok) {
          parent->children[parent->totchild++] = node2;
          node2->parent = parent;
          break;
        }

        e = e->v1 == v ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v->e);

      if (last_step) {
        roots.append(parent);
      }

      for (int j = 0; j < parent->totchild; j++) {
        parent->children[j]->parent = parent;
      }
    }

    BM_ITER_MESH_INDEX (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH, i) {
      while (vnodemap[i]->parent) {
        vnodemap[i] = vnodemap[i]->parent;
      }
    }
  }

  BLI_mempool_iter loopiter;
  BLI_mempool_iternew(pbvh->header.bm->lpool, &loopiter);
  BMLoop *l = (BMLoop *)BLI_mempool_iterstep(&loopiter);
  BMEdge *e;
  BMFace *f;

  for (i = 0; l; l = (BMLoop *)BLI_mempool_iterstep(&loopiter), i++) {
    l->head.hflag &= ~flag;
  }
  BM_ITER_MESH (e, &iter, pbvh->header.bm, BM_EDGES_OF_MESH) {
    e->head.hflag &= ~flag;
  }

  BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
    f->head.hflag &= ~flag;
  }

  int totroot = roots.size();
  Vector<ReVertNode *> nstack;
  int vorder = 0, eorder = 0, lorder = 0, forder = 0;

  for (i = 0; i < totroot; i++) {
    nstack.clear();

    ReVertNode *node = roots[i];
    nstack.append(node);

    while (nstack.size() > 0) {
      ReVertNode *node2 = nstack.pop_last();

      if (node2->totchild == 0) {
        for (int j = 0; j < node2->totvert; j++) {
          v = node2->verts[j];

#if 0
          const int cd_vcol = CustomData_get_offset(&pbvh->header.bm->vdata,CD_PROP_COLOR);

          if (cd_vcol >= 0) {
            MPropCol *col = BM_ELEM_CD_PTR<MPropCol*>(node2->verts[j],cd_vcol);

            float r = 0.0f,g = 0.0f,b = 0.0f;

            ReVertNode *parent = node2->parent;
            for (int j = 0; parent->parent && j < 2; j++) {
              parent = parent->parent;
            }

            unsigned int p = (unsigned int)node2->parent;
            p = p % 65535;

            unsigned int p2 = (unsigned int)parent;
            p2 = p2 % 65535;

            r = ((float)vorder) * 0.01;
            g = ((float)p2) / 65535.0f;
            b = ((float)p2) / 65535.0f;

            r = cosf(r * 17.2343) * 0.5 + 0.5;
            g = cosf(g * 11.2343) * 0.5 + 0.5;
            b = cosf(b * 19.2343) * 0.5 + 0.5;

            col->color[0] = r;
            col->color[1] = g;
            col->color[2] = b;
            col->color[3] = 1.0f;
          }
#endif
          v->head.index = vorder++;

          BMEdge *e = v->e;
          if (!e) {
            continue;
          }

          do {
            if (!(e->head.hflag & flag)) {
              e->head.hflag |= flag;
              e->head.index = eorder++;
            }

            if (e->l) {
              BMLoop *l = e->l;

              do {
                if (!(l->head.hflag & flag)) {
                  l->head.hflag |= flag;
                  l->head.index = lorder++;
                }

                if (!(l->f->head.hflag & flag)) {
                  l->f->head.hflag |= flag;
                  l->f->head.index = forder++;
                }

                l = l->radial_next;
              } while (l != e->l);
            }
            e = e->v1 == v ? e->v1_disk_link.next : e->v2_disk_link.next;
          } while (e != v->e);
        }
      }
      else {
        for (int j = 0; j < node2->totchild; j++) {
          nstack.append(node2->children[j]);
        }
      }
    }
  }

  uint *vidx, *eidx, *lidx, *fidx;

  vidx = MEM_cnew_array<uint>(pbvh->header.bm->totvert, "vorder");
  eidx = MEM_cnew_array<uint>(pbvh->header.bm->totedge, "eorder");
  lidx = MEM_cnew_array<uint>(pbvh->header.bm->totloop, "lorder");
  fidx = MEM_cnew_array<uint>(pbvh->header.bm->totface, "forder");

  printf("v %d %d\n", vorder, pbvh->header.bm->totvert);
  printf("e %d %d\n", eorder, pbvh->header.bm->totedge);
  printf("l %d %d\n", lorder, pbvh->header.bm->totloop);
  printf("f %d %d\n", forder, pbvh->header.bm->totface);

  BM_ITER_MESH_INDEX (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH, i) {
    vidx[i] = (uint)v->head.index;
  }

  BM_ITER_MESH_INDEX (e, &iter, pbvh->header.bm, BM_EDGES_OF_MESH, i) {
    eidx[i] = (uint)e->head.index;
  }
  BM_ITER_MESH_INDEX (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH, i) {
    fidx[i] = (uint)f->head.index;
  }

  BLI_mempool_iternew(pbvh->header.bm->lpool, &loopiter);
  l = (BMLoop *)BLI_mempool_iterstep(&loopiter);

  for (i = 0; l; l = (BMLoop *)BLI_mempool_iterstep(&loopiter), i++) {
    // handle orphaned loops
    if (!(l->head.hflag & flag)) {
      printf("warning in %s: orphaned loop!\n", __func__);
      l->head.index = lorder++;
    }

    lidx[i] = (uint)l->head.index;
  }

  printf("roots: %d\n", (int)roots.size());

  BM_mesh_remap(pbvh->header.bm, vidx, eidx, lidx, fidx);

  MEM_SAFE_FREE(vidx);
  MEM_SAFE_FREE(eidx);
  MEM_SAFE_FREE(lidx);
  MEM_SAFE_FREE(fidx);

  BLI_mempool_destroy(pool);
  MEM_SAFE_FREE(vnodemap);

  return pbvh->header.bm;
}

BMesh *BKE_pbvh_reorder_bmesh2(PBVH *pbvh)
{
  if (BKE_pbvh_type(pbvh) != PBVH_BMESH || pbvh->totnode == 0) {
    return pbvh->header.bm;
  }

  // try to group memory allocations by node
  struct TempNodeData {
    Vector<BMVert *> verts;
    Vector<BMEdge *> edges;
    Vector<BMFace *> faces;
  };

  Vector<TempNodeData> nodedata;
  nodedata.resize(pbvh->totnode);

  BMIter iter;
  int types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};

#define VISIT_TAG BM_ELEM_TAG

  BM_mesh_elem_index_ensure(pbvh->header.bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_table_ensure(pbvh->header.bm, BM_VERT | BM_EDGE | BM_FACE);

  for (int i = 0; i < 3; i++) {
    BMHeader *elem;

    BM_ITER_MESH (elem, &iter, pbvh->header.bm, types[i]) {
      elem->hflag &= ~VISIT_TAG;
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    Vector<BMVert *> &verts = nodedata[i].verts;
    Vector<BMEdge *> &edges = nodedata[i].edges;
    Vector<BMFace *> &faces = nodedata[i].faces;

    BMVert *v;
    BMFace *f;

    TGSET_ITER (v, node->bm_unique_verts) {
      if (v->head.hflag & VISIT_TAG) {
        continue;
      }

      v->head.hflag |= VISIT_TAG;
      verts.append(v);

      BMEdge *e = v->e;
      do {
        if (!(e->head.hflag & VISIT_TAG)) {
          e->head.hflag |= VISIT_TAG;
          edges.append(e);
        }
        e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v->e);
    }
    TGSET_ITER_END;

    TGSET_ITER (f, node->bm_faces) {
      if (f->head.hflag & VISIT_TAG) {
        continue;
      }

      faces.append(f);
      f->head.hflag |= VISIT_TAG;
    }
    TGSET_ITER_END;
  }

  BMAllocTemplate templ = {pbvh->header.bm->totvert,
                           pbvh->header.bm->totedge,
                           pbvh->header.bm->totloop,
                           pbvh->header.bm->totface};
  struct BMeshCreateParams params = {0};

  BMesh *bm2 = BM_mesh_create(&templ, &params);

  CustomData_copy_all_layout(&pbvh->header.bm->vdata, &bm2->vdata);
  CustomData_copy_all_layout(&pbvh->header.bm->edata, &bm2->edata);
  CustomData_copy_all_layout(&pbvh->header.bm->ldata, &bm2->ldata);
  CustomData_copy_all_layout(&pbvh->header.bm->pdata, &bm2->pdata);

  CustomData_bmesh_init_pool(&bm2->vdata, pbvh->header.bm->totvert, BM_VERT);
  CustomData_bmesh_init_pool(&bm2->edata, pbvh->header.bm->totedge, BM_EDGE);
  CustomData_bmesh_init_pool(&bm2->ldata, pbvh->header.bm->totloop, BM_LOOP);
  CustomData_bmesh_init_pool(&bm2->pdata, pbvh->header.bm->totface, BM_FACE);

  Vector<BMVert *> verts;
  Vector<BMEdge *> edges;
  Vector<BMFace *> faces;

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].verts.size(); j++) {
      BMVert *v1 = nodedata[i].verts[j];
      BMVert *v2 = BM_vert_create(bm2, v1->co, nullptr, BM_CREATE_NOP);
      BM_elem_attrs_copy_ex(pbvh->header.bm, bm2, v1, v2, 0, 0L);

      v2->head.index = v1->head.index = verts.size();
      verts.append(v2);
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].edges.size(); j++) {
      BMEdge *e1 = nodedata[i].edges[j];
      BMEdge *e2 = BM_edge_create(
          bm2, verts[e1->v1->head.index], verts[e1->v2->head.index], nullptr, BM_CREATE_NOP);
      BM_elem_attrs_copy_ex(pbvh->header.bm, bm2, e1, e2, 0, 0L);

      e2->head.index = e1->head.index = edges.size();
      edges.append(e2);
    }
  }

  Vector<BMVert *> fvs;
  Vector<BMEdge *> fes;

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].faces.size(); j++) {
      BMFace *f1 = nodedata[i].faces[j];

      fvs.clear();
      fes.clear();

      int totloop = 0;
      BMLoop *l1 = f1->l_first;
      do {
        fvs.append(verts[l1->v->head.index]);
        fes.append(edges[l1->e->head.index]);
        l1 = l1->next;
        totloop++;
      } while (l1 != f1->l_first);

      BMFace *f2 = BM_face_create(bm2, fvs.data(), fes.data(), totloop, nullptr, BM_CREATE_NOP);
      f1->head.index = f2->head.index = faces.size();
      faces.append(f2);

      // CustomData_bmesh_copy_data(&pbvh->header.bm->pdata, &bm2->pdata, f1->head.data,
      // &f2->head.data);
      BM_elem_attrs_copy_ex(pbvh->header.bm, bm2, f1, f2, 0, 0L);

      BMLoop *l2 = f2->l_first;
      do {
        BM_elem_attrs_copy_ex(pbvh->header.bm, bm2, l1, l2, 0, 0L);

        l1 = l1->next;
        l2 = l2->next;
      } while (l2 != f2->l_first);
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    int totunique = node->bm_unique_verts->length;
    int totother = node->bm_other_verts->length;
    int totface = node->bm_faces->length;

    TableGSet *bm_faces = BLI_table_gset_new_ex("bm_faces", totface);
    TableGSet *bm_other_verts = BLI_table_gset_new_ex("bm_other_verts", totunique);
    TableGSet *bm_unique_verts = BLI_table_gset_new_ex("bm_unique_verts", totother);

    BMVert *v;
    BMFace *f;

    TGSET_ITER (v, node->bm_unique_verts) {
      BLI_table_gset_insert(bm_unique_verts, verts[v->head.index]);
    }
    TGSET_ITER_END;
    TGSET_ITER (v, node->bm_other_verts) {
      BLI_table_gset_insert(bm_other_verts, verts[v->head.index]);
    }
    TGSET_ITER_END;
    TGSET_ITER (f, node->bm_faces) {
      BLI_table_gset_insert(bm_faces, faces[f->head.index]);
    }
    TGSET_ITER_END;

    BLI_table_gset_free(node->bm_faces, nullptr);
    BLI_table_gset_free(node->bm_other_verts, nullptr);
    BLI_table_gset_free(node->bm_unique_verts, nullptr);

    node->bm_faces = bm_faces;
    node->bm_other_verts = bm_other_verts;
    node->bm_unique_verts = bm_unique_verts;

    node->flag |= PBVH_UpdateTris | PBVH_UpdateRedraw;
  }

  BM_mesh_free(pbvh->header.bm);
  pbvh->header.bm = bm2;

  return bm2;
}

struct SortElem {
  BMElem *elem;
  int index;
  int cd_node_off;
};

static int sort_verts_faces(const void *va, const void *vb)
{
  SortElem *a = (SortElem *)va;
  SortElem *b = (SortElem *)vb;
  int ni1 = BM_ELEM_CD_GET_INT(a->elem, a->cd_node_off);
  int ni2 = BM_ELEM_CD_GET_INT(b->elem, b->cd_node_off);

  return ni1 - ni2;
}

static int sort_edges(const void *va, const void *vb)
{
  SortElem *a = (SortElem *)va;
  SortElem *b = (SortElem *)vb;

  BMEdge *e1 = (BMEdge *)a->elem;
  BMEdge *e2 = (BMEdge *)b->elem;

  int ni1 = BM_ELEM_CD_GET_INT(e1->v1, a->cd_node_off);
  int ni2 = BM_ELEM_CD_GET_INT(e1->v2, a->cd_node_off);
  int ni3 = BM_ELEM_CD_GET_INT(e2->v1, b->cd_node_off);
  int ni4 = BM_ELEM_CD_GET_INT(e2->v2, b->cd_node_off);

  return (ni1 + ni2) - (ni3 + ni4);
}

BMesh *BKE_pbvh_reorder_bmesh1(PBVH *pbvh)
{
  BMesh *bm = pbvh->header.bm;

  Vector<Vector<int>> save_other_vs;
  Vector<Vector<int>> save_unique_vs;
  Vector<Vector<int>> save_fs;

  save_other_vs.resize(pbvh->totnode);
  save_unique_vs.resize(pbvh->totnode);
  save_fs.resize(pbvh->totnode);

  SortElem *verts = MEM_cnew_array<SortElem>(bm->totvert, __func__);
  SortElem *edges = MEM_cnew_array<SortElem>(bm->totedge, __func__);
  SortElem *faces = MEM_cnew_array<SortElem>(bm->totface, __func__);

  BMIter iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  int i = 0;

  BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
    verts[i].elem = (BMElem *)v;
    verts[i].cd_node_off = pbvh->cd_vert_node_offset;
    verts[i].index = i;
    v->head.index = i;
  }
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    edges[i].elem = (BMElem *)e;
    edges[i].cd_node_off = pbvh->cd_vert_node_offset;
    edges[i].index = i;
    e->head.index = i;
  }
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    faces[i].elem = (BMElem *)f;
    faces[i].cd_node_off = pbvh->cd_face_node_offset;
    faces[i].index = i;
    f->head.index = i;
  }

  for (i = 0; i < pbvh->totnode; i++) {
    Vector<int> other_vs;
    Vector<int> unique_vs;
    Vector<int> fs;

    PBVHNode *node = pbvh->nodes + i;
    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BMVert *v;
    BMFace *f;

    TGSET_ITER (v, node->bm_unique_verts) {
      unique_vs.append(v->head.index);
    }
    TGSET_ITER_END;
    TGSET_ITER (v, node->bm_other_verts) {
      other_vs.append(v->head.index);
    }
    TGSET_ITER_END;
    TGSET_ITER (f, node->bm_faces) {
      fs.append(f->head.index);
    }
    TGSET_ITER_END;

    save_unique_vs[i] = unique_vs;
    save_other_vs[i] = other_vs;
    save_fs[i] = fs;
  }

  qsort(verts, bm->totvert, sizeof(SortElem), sort_verts_faces);
  qsort(edges, bm->totedge, sizeof(SortElem), sort_edges);
  qsort(faces, bm->totface, sizeof(SortElem), sort_verts_faces);

  uint *vs = MEM_cnew_array<uint>(bm->totvert, __func__);
  uint *es = MEM_cnew_array<uint>(bm->totedge, __func__);
  uint *fs = MEM_cnew_array<uint>(bm->totface, __func__);

  for (i = 0; i < bm->totvert; i++) {
    vs[i] = (uint)verts[i].index;
    verts[i].elem->head.index = verts[i].index;
  }
  for (i = 0; i < bm->totedge; i++) {
    es[i] = (uint)edges[i].index;
    edges[i].elem->head.index = edges[i].index;
  }
  for (i = 0; i < bm->totface; i++) {
    fs[i] = (uint)faces[i].index;
    faces[i].elem->head.index = faces[i].index;
  }

  BM_mesh_remap(bm, vs, es, nullptr, fs);

  // create new mappings
  BMVert **mapvs = MEM_cnew_array<BMVert *>(bm->totvert, __func__);
  BMEdge **mapes = MEM_cnew_array<BMEdge *>(bm->totedge, __func__);
  BMFace **mapfs = MEM_cnew_array<BMFace *>(bm->totface, __func__);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    mapvs[v->head.index] = v;
  }
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    mapes[e->head.index] = e;
  }
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    mapfs[f->head.index] = f;
  }

  // rebuild bm_unique_verts bm_other_verts and bm_faces in pbvh nodes
  for (i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    int tot_unique_vs = BLI_table_gset_len(node->bm_unique_verts);
    int tot_other_vs = BLI_table_gset_len(node->bm_other_verts);
    int tot_fs = BLI_table_gset_len(node->bm_faces);

    BLI_table_gset_free(node->bm_unique_verts, nullptr);
    BLI_table_gset_free(node->bm_other_verts, nullptr);
    BLI_table_gset_free(node->bm_faces, nullptr);

    node->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
    node->bm_other_verts = BLI_table_gset_new("bm_other_verts");
    node->bm_faces = BLI_table_gset_new("bm_faces");

    Vector<int> &unique_vs = save_unique_vs[i];
    Vector<int> &other_vs = save_other_vs[i];
    Vector<int> &fs = save_fs[i];

    for (int j = 0; j < tot_unique_vs; j++) {
      BLI_table_gset_add(node->bm_unique_verts, mapvs[unique_vs[j]]);
    }
    for (int j = 0; j < tot_other_vs; j++) {
      BLI_table_gset_add(node->bm_other_verts, mapvs[other_vs[j]]);
    }

    for (int j = 0; j < tot_fs; j++) {
      BLI_table_gset_add(node->bm_faces, mapfs[fs[j]]);
    }

    node->flag |= PBVH_UpdateTris;
  }

  MEM_SAFE_FREE(mapvs);
  MEM_SAFE_FREE(mapes);
  MEM_SAFE_FREE(mapfs);

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  MEM_SAFE_FREE(vs);
  MEM_SAFE_FREE(es);
  MEM_SAFE_FREE(fs);

  MEM_SAFE_FREE(verts);
  MEM_SAFE_FREE(edges);
  MEM_SAFE_FREE(faces);

  return pbvh->header.bm;
}

/* restore bmesh references from previously indices saved by BKE_pbvh_bmesh_save_indices */
void BKE_pbvh_bmesh_from_saved_indices(PBVH *pbvh)
{
  BM_mesh_elem_table_ensure(pbvh->header.bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_index_ensure(pbvh->header.bm, BM_VERT | BM_EDGE | BM_FACE);

  Vector<BMLoop *> ltable;

  BMFace *f;
  BMIter iter;
  int i = 0;

  BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;

    do {
      l->head.index = i++;
      ltable.append(l);
    } while ((l = l->next) != f->l_first);
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BLI_table_gset_free(node->bm_unique_verts, nullptr);
    BLI_table_gset_free(node->bm_faces, nullptr);

    if (node->bm_other_verts) {
      BLI_table_gset_free(node->bm_other_verts, nullptr);
    }

    node->bm_other_verts = BLI_table_gset_new("bm_other_verts");
    node->flag |= PBVH_UpdateOtherVerts;

    node->bm_faces = BLI_table_gset_new("bm_faces");
    node->bm_unique_verts = BLI_table_gset_new("bm_verts");

    int j = 0;
    int *data = node->prim_indices;

    while (data[j] != -1 && j < node->totprim) {
      BMFace *f = pbvh->header.bm->ftable[data[j]];
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);

      BLI_table_gset_insert(node->bm_faces, f);
      j++;
    }

    j++;

    while (j < node->totprim) {
      if (data[j] < 0 || data[j] >= pbvh->header.bm->totvert) {
        printf("%s: bad vertex at index %d!\n", __func__, data[j]);
        continue;
      }
      BMVert *v = pbvh->header.bm->vtable[data[j]];
      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);

      BLI_table_gset_insert(node->bm_unique_verts, v);
      j++;
    }

    MEM_SAFE_FREE(node->prim_indices);

    // don't try to load invalid triangulation
    if (node->flag & PBVH_UpdateTris) {
      continue;
    }

    for (j = 0; j < node->tot_tri_buffers + 1; j++) {
      PBVHTriBuf *tribuf = j == node->tot_tri_buffers ? node->tribuf : node->tri_buffers + j;

      if (!tribuf) {
        break;
      }

      for (int k = 0; k < tribuf->totvert; k++) {
        tribuf->verts[k].i = (intptr_t)pbvh->header.bm->vtable[tribuf->verts[k].i];
      }

      for (int k = 0; k < tribuf->totloop; k++) {
        tribuf->loops[k] = (uintptr_t)ltable[tribuf->loops[k]];
      }

      for (int k = 0; k < tribuf->tottri; k++) {
        PBVHTri *tri = tribuf->tris + k;

        for (int l = 0; l < 3; l++) {
          tri->l[l] = (uintptr_t)ltable[tri->l[l]];
        }

        tri->f.i = (intptr_t)pbvh->header.bm->ftable[tri->f.i];
      }
    }

    node->prim_indices = nullptr;
    node->totprim = 0;
  }
}

static void pbvh_bmesh_fetch_cdrefs(PBVH *pbvh)
{
  BMesh *bm = pbvh->header.bm;

  int idx = CustomData_get_named_layer_index(
      &bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_vertex));
  pbvh->cd_vert_node_offset = bm->vdata.layers[idx].offset;

  idx = CustomData_get_named_layer_index(
      &bm->pdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_face));
  pbvh->cd_face_node_offset = bm->pdata.layers[idx].offset;

  idx = CustomData_get_named_layer_index(
      &bm->pdata, CD_PROP_FLOAT2, SCULPT_ATTRIBUTE_NAME(face_areas));
  pbvh->cd_face_area = bm->pdata.layers[idx].offset;

  pbvh->cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  pbvh->cd_faceset_offset = CustomData_get_offset_named(
      &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
}

void BKE_pbvh_bmesh_set_toolflags(PBVH *pbvh, bool use_toolflags)
{
  if (use_toolflags == pbvh->header.bm->use_toolflags) {
    return;
  }

  BM_mesh_toolflags_set(pbvh->header.bm, use_toolflags);

  /* Customdata layout might've changed. */
  pbvh_bmesh_fetch_cdrefs(pbvh);
}

float BKE_pbvh_bmesh_detail_size_avg_get(PBVH *pbvh)
{
  return (pbvh->bm_max_edge_len + pbvh->bm_min_edge_len) * 0.5f;
}

namespace blender::bke::pbvh {

void update_sharp_boundary_bmesh(BMVert *v, int cd_boundary_flag, const float sharp_angle_limit)
{
  int flag = BM_ELEM_CD_GET_INT(v, cd_boundary_flag);
  flag &= ~(SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE | SCULPT_BOUNDARY_SHARP_ANGLE |
            SCULPT_CORNER_SHARP_ANGLE);

  if (!v->e) {
    return;
  }

  int sharp_num = 0;

  BMEdge *e = v->e;
  do {
    if (!e->l || e->l == e->l->radial_next) {
      continue;
    }

    if (BKE_pbvh_test_sharp_faces_bmesh(e->l->f, e->l->radial_next->f, sharp_angle_limit)) {
      flag |= SCULPT_BOUNDARY_SHARP_ANGLE;
      sharp_num++;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (sharp_num > 2) {
    flag |= SCULPT_CORNER_SHARP_ANGLE;
  }

  BM_ELEM_CD_SET_INT(v, cd_boundary_flag, flag);
}
}  // namespace blender::bke::pbvh
