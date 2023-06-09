/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
using blender::bke::dyntopo::DyntopoSet;

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

    for (BMFace *f : *node->bm_faces) {
      if (!f || f->head.htype != BM_FACE) {
        _debugprint("Corrupted (freed?) face in node->bm_faces\n");
        continue;
      }

      if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) != i) {
        _debugprint("Face in more then one node\n");
      }
    }
  }
}

ATTR_NO_OPT void pbvh_bmesh_check_nodes(PBVH *pbvh)
{
  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

    if (ni >= 0 && (!v->e || !v->e->l)) {
      //_debugprint("wire vert had node reference: %p (type %d)\n", v, v->head.htype);
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

    if (!node->bm_unique_verts->contains(v)) {
      _debugprint("vert not in node->bm_unique_verts\n");
    }

    if (node->bm_other_verts->contains(v)) {
      _debugprint("vert in node->bm_other_verts");
    }

    if (pbvh->cd_valence != -1) {
      BKE_pbvh_bmesh_check_valence(pbvh, (PBVHVertRef){.i = (intptr_t)v});
      int valence = BM_ELEM_CD_GET_INT(v, pbvh->cd_valence);

      if (BM_vert_edge_count(v) != valence) {
        _debugprint("cached vertex valence mismatch; old: %d, should be: %d\n",
                    valence,
                    BM_vert_edge_count(v));
      }
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

    for (BMVert *v : *node->bm_unique_verts) {
      if (BM_elem_is_free((BMElem *)v, BM_VERT)) {
        printf("bm_unique_verts has freed vertex.\n");
        continue;
      }

      int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

      if (ni != i) {
        if (ni >= 0 && ni < pbvh->totnode) {
          PBVHNode *node2 = pbvh->nodes + ni;
          _debugprint("v node offset is wrong, %d\n",
                      !node2->bm_unique_verts ? 0 : node2->bm_unique_verts->contains(v));
        }
        else {
          _debugprint("v node offset is wrong\n");
        }
      }

      if (!v || v->head.htype != BM_VERT) {
        _debugprint("corruption in pbvh! bm_unique_verts\n");
      }
      else if (node->bm_other_verts->contains(v)) {
        _debugprint("v in both unique and other verts\n");
      }
    }

    for (BMFace *f : *node->bm_faces) {
      if (!f || f->head.htype != BM_FACE) {
        _debugprint("corruption in pbvh! bm_faces\n");
        continue;
      }

      int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
      if (pbvh->nodes + ni != node) {
        _debugprint("face in multiple nodes!\n");
      }
    }

    for (BMVert *v : *node->bm_other_verts) {
      BMIter iter;
      BMFace *f = nullptr;

      if (BM_elem_is_free((BMElem *)v, BM_VERT)) {
        printf("bm_other_verts has freed vertex.\n");
        continue;
      }

      int ni = int(node - pbvh->nodes);

      bool ok = false;
      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) == ni) {
          if (!node->bm_faces->contains(f)) {
            _debugprint("Node does not contain f, but f has a node index to it\n");
            continue;
          }

          ok = true;
          break;
        }
      }

      if (!ok) {
        int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
        _debugprint(
            "v is in node.bm_other_verts but none of its faces are in node.bm_faces. owning node "
            "(not this one): %d\n",
            ni);
      }

      if (!v || v->head.htype != BM_VERT) {
        _debugprint("corruption in pbvh! bm_other_verts\n");
      }
      else if (node->bm_unique_verts->contains(v)) {
        _debugprint("v in both unique and other verts\n");
      }
    }
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
  dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);

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

  node->bm_unique_verts->add(v);
  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, node_index);

  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris;

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

  node->bm_faces->add(f);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, node_index);

  /* mark node for update */
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris |
                PBVH_UpdateCurvatureDir | PBVH_UpdateTriAreas;
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
        node->bm_unique_verts->add(l->v);
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, node_index);

        node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris;
      }

      pbvh_boundary_update_bmesh(pbvh, l->v);
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_VALENCE);

      l = l->next;
    } while (l != f->l_first);
  }
  else {
    BMLoop *l = f->l_first;
    do {
      pbvh_boundary_update_bmesh(pbvh, l->v);
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_VALENCE);
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
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);

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

        if (ok && (ni == DYNTOPO_NODE_NONE || node->bm_faces->size() < pbvh->leaf_limit)) {
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

void pbvh_bmesh_vert_remove(PBVH *pbvh, BMVert *v)
{
  /* never match for first time */
  const int updateflag = PBVH_UpdateDrawBuffers | PBVH_UpdateBB | PBVH_UpdateTris |
                         PBVH_UpdateNormals;

  int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
  if (ni != DYNTOPO_NODE_NONE) {
    PBVHNode *node = pbvh->nodes + ni;
    node->bm_unique_verts->remove(v);
    node->flag |= (PBVHNodeFlags)updateflag;
  }

  BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

  if (!v->e) {
    return;
  }

  const int tag = 2;

  BMEdge *e = v->e;
  do {
    BMLoop *l = e->l;
    if (!l) {
      continue;
    }

    do {
      l->f->head.api_flag |= tag;
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  e = v->e;
  do {
    BMLoop *l = e->l;
    if (!l) {
      continue;
    }

    do {
      if (!(l->f->head.api_flag & tag)) {
        continue;
      }

      l->f->head.api_flag &= ~tag;

      int ni2 = BM_ELEM_CD_GET_INT(l->f, pbvh->cd_face_node_offset);
      if (ni2 != DYNTOPO_NODE_NONE) {
        PBVHNode *node = pbvh->nodes + ni2;

        if (ni2 >= 0 && ni2 < pbvh->totnode && pbvh->nodes[ni2].flag & PBVH_Leaf) {
          node->flag |= PBVHNodeFlags(updateflag);
          node->bm_other_verts->remove(v);
        }
      }
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

void pbvh_bmesh_face_remove(
    PBVH *pbvh, BMFace *f, bool log_face, bool check_verts, bool ensure_ownership_transfer)
{
  PBVHNode *f_node = pbvh_bmesh_node_from_face(pbvh, f);

  if (!f_node || !(f_node->flag & PBVH_Leaf)) {
    printf(
        "%s: pbvh corruption. %d\n", __func__, BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset));
    return;
  }

  bm_logstack_push();

  /* Check if any of this face's vertices need to be removed
   * from the node */
  if (check_verts) {
    int ni = int(f_node - pbvh->nodes);

    BMLoop *l = f->l_first;
    do {
      bool owns_vert = BM_ELEM_CD_GET_INT(l->v, pbvh->cd_vert_node_offset) == ni;
      bool ok = false;
      int new_ni = DYNTOPO_NODE_NONE;

      BMIter iter;
      BMLoop *l2;
      BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
        int ni2 = BM_ELEM_CD_GET_INT(l2->f, pbvh->cd_face_node_offset);
        if (l2->f != f && ni2 == ni) {
          ok = true;
        }

        if (ni2 != DYNTOPO_NODE_NONE && ni2 != ni) {
          if (ni2 < 0 || ni2 >= pbvh->totnode || !(pbvh->nodes[ni2].bm_other_verts)) {
            printf("error! invalid node index %d!\n", ni2);
          }
          else {
            new_ni = ni2;
          }
        }
      }

      if (!ok) {
        if (owns_vert) {
          f_node->bm_unique_verts->remove(l->v);

          if (new_ni != DYNTOPO_NODE_NONE) {
            PBVHNode *new_node = &pbvh->nodes[new_ni];

            new_node->bm_other_verts->remove(l->v);
            new_node->bm_unique_verts->add(l->v);
            BM_ELEM_CD_SET_INT(l->v, pbvh->cd_vert_node_offset, new_ni);
          }
          else {
            BM_ELEM_CD_SET_INT(l->v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
          }
        }
        else {
          f_node->bm_other_verts->remove(l->v);
        }
      }
    } while ((l = l->next) != f->l_first);
  }

  /* Remove face from node and top level */
  f_node->bm_faces->remove(f);
  BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

  /* Log removed face */
  if (log_face) {
    BM_log_face_removed(pbvh->header.bm, pbvh->bm_log, f);
  }

  /* mark node for update */
  f_node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateNormals | PBVH_UpdateTris |
                  PBVH_UpdateTriAreas | PBVH_UpdateCurvatureDir;

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
    n->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
  }
  n->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");

  BB_reset(&n->vb);
  BB_reset(&n->orig_vb);

  for (BMFace *f : *n->bm_faces) {
    /* Update ownership of faces */
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    do {
      BMVert *v = l_iter->v;

      int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
      *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

      if (!n->bm_unique_verts->contains(v)) {
        if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
          n->bm_other_verts->add(v);
        }
        else {
          n->bm_unique_verts->add(v);
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

  if (n->depth >= PBVH_STACK_FIXED_DEPTH || n->bm_faces->size() <= pbvh->leaf_limit) {
    /* Node limit not exceeded */
    pbvh_bmesh_node_finalize(pbvh, node_index, cd_vert_node_offset, cd_face_node_offset, add_orco);
    return;
  }

  /* Calculate bounding box around primitive centroids */
  BB cb;
  BB_reset(&cb);

  for (BMFace *f : *n->bm_faces) {
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    BB_expand(&cb, bbc->bcentroid);
  }

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

  c1->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces", int64_t(n->bm_faces->size() >> 1));
  c2->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces", int64_t(n->bm_faces->size() >> 1));

  c1->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
  c2->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");

  c1->bm_other_verts = c2->bm_other_verts = nullptr;

  /* Partition the parent node's faces between the two children */
  for (BMFace *f : *n->bm_faces) {
    const BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    if (bbc->bcentroid[axis] < mid) {
      c1->bm_faces->add(f);
    }
    else {
      c2->bm_faces->add(f);
    }
  }

  /* Clear this node */

  /* Assign verts to c1 and c2.  Note that the previous
     method of simply marking them as untaken and rebuilding
     unique verts later doesn't work, as it assumes that dyntopo
     never assigns verts to nodes that don't contain their
     faces.*/
  if (n->bm_unique_verts) {
    for (BMVert *v : *n->bm_unique_verts) {
      if (v->co[axis] < mid) {
        BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, (c1 - pbvh->nodes));
        c1->bm_unique_verts->add(v);
      }
      else {
        BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, (c2 - pbvh->nodes));
        c2->bm_unique_verts->add(v);
      }
    }

    MEM_delete(n->bm_unique_verts);
  }

  if (n->bm_faces) {
    /* Unclaim faces */
    for (BMFace *f : *n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
    }

    MEM_delete(n->bm_faces);
  }

  if (n->bm_other_verts) {
    MEM_delete(n->bm_other_verts);
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
  DyntopoSet<BMFace> *bm_faces = pbvh->nodes[node_index].bm_faces;
  const int bm_faces_size = bm_faces->size();

  if (bm_faces_size <= pbvh->leaf_limit || pbvh->nodes[node_index].depth >= PBVH_STACK_FIXED_DEPTH)
  {
    /* Node limit not exceeded */
    return false;
  }

  /* Trigger draw manager cache invalidation. */
  pbvh->draw_cache_invalid = true;

  /* For each BMFace, store the AABB and AABB centroid */
  BBC *bbc_array = MEM_cnew_array<BBC>(bm_faces_size, "BBC");

  int i = 0;
  for (BMFace *f : *bm_faces) {

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
    i++;
  }

  /* Likely this is already dirty. */
  pbvh->header.bm->elem_index_dirty |= BM_FACE;

  pbvh_bmesh_node_split(pbvh, bbc_array, node_index, false, 0);

  MEM_freeN(bbc_array);
  pbvh_bmesh_check_nodes(pbvh);

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

  node->bm_faces->add(f);

  PBVHNodeFlags updateflag = PBVH_UpdateTris | PBVH_UpdateBB | PBVH_UpdateDrawBuffers |
                             PBVH_UpdateCurvatureDir | PBVH_UpdateColor | PBVH_UpdateMask |
                             PBVH_UpdateNormals | PBVH_UpdateOriginalBB | PBVH_UpdateVisibility |
                             PBVH_UpdateRedraw | PBVH_RebuildDrawBuffers | PBVH_UpdateTriAreas;

  node->flag |= updateflag;

  // ensure verts are in pbvh
  BMLoop *l = f->l_first;
  do {
    int ni2 = BM_ELEM_CD_GET_INT(l->v, pbvh->cd_vert_node_offset);

    if (ni2 != DYNTOPO_NODE_NONE &&
        (ni2 < 0 || ni2 >= pbvh->totnode || !(pbvh->nodes[ni2].flag & PBVH_Leaf)))
    {
      printf("%s: pbvh corruption\n", __func__);
      ni2 = DYNTOPO_NODE_NONE;
    }

    BB_expand(&node->vb, l->v->co);
    BB_expand(&node->orig_vb, BM_ELEM_CD_PTR<float *>(l->v, pbvh->cd_origco));

    if (ni2 == DYNTOPO_NODE_NONE) {
      node->bm_unique_verts->add(l->v);
      BM_ELEM_CD_SET_INT(l->v, pbvh->cd_vert_node_offset, ni);
      continue;
    }

    PBVHNode *node2 = pbvh->nodes + ni2;

    if (ni2 != ni) {
      node->bm_other_verts->add(l->v);

      BB_expand(&node2->vb, l->v->co);
      BB_expand(&node2->orig_vb, BM_ELEM_CD_PTR<float *>(l->v, pbvh->cd_origco));

      node2->flag |= updateflag;
    }
  } while ((l = l->next) != f->l_first);
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

  int usize = node->bm_unique_verts->size();
  int osize = node->bm_other_verts->size();

  DyntopoSet<BMVert> *old_unique_verts = node->bm_unique_verts;
  DyntopoSet<BMVert> *old_other_verts = node->bm_other_verts;

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

  for (BMVert *v : *old_unique_verts) {
    check_vert(v);
  }

  for (BMVert *v : *old_other_verts) {
    check_vert(v);
  }

  node->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
  node->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");

  bool update = false;

  for (BMFace *f : *node->bm_faces) {
    BMLoop *l = f->l_first;
    do {
      int ni2 = BM_ELEM_CD_GET_INT(l->v, cd_vert_node);

      if (ni2 == DYNTOPO_NODE_NONE) {
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, ni);
        ni2 = ni;
        update = true;
      }

      if (ni2 == ni) {
        node->bm_unique_verts->add(l->v);
        BM_ELEM_CD_SET_INT(l->v, cd_vert_node, ni);
      }
      else {
        node->bm_other_verts->add(l->v);
      }
    } while ((l = l->next) != f->l_first);
  }

  for (BMVert *v : *old_unique_verts) {
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
          PBVHNode *node2 = pbvh->nodes + ni2;

          node2->bm_unique_verts->add(v);
          node2->bm_other_verts->remove(v);

          BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, ni2);

          ok = true;
          break;
        }
      }

      if (!ok) {
        printf("pbvh error: orphaned vert node reference\n");
      }
    }
  }

  if (usize != node->bm_unique_verts->size()) {
    update = true;
#if 0
    printf("possible pbvh error: bm_unique_verts might have had bad data. old: %d, new: %d\n",
      usize,
      node->bm_unique_verts->size());
#endif
  }

  if (osize != node->bm_other_verts->size()) {
    update = true;
#if 0
    printf("possible pbvh error: bm_other_verts might have had bad data. old: %d, new: %d\n",
      osize,
      node->bm_other_verts->size());
#endif
  }

  if (update) {
    node->flag |= PBVH_UpdateNormals | PBVH_UpdateDrawBuffers | PBVH_RebuildDrawBuffers |
                  PBVH_UpdateBB;
    node->flag |= PBVH_UpdateOriginalBB | PBVH_UpdateRedraw | PBVH_UpdateColor | PBVH_UpdateTris |
                  PBVH_UpdateVisibility;
  }

  MEM_delete(old_unique_verts);
  MEM_delete(old_other_verts);
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

  for (PBVHTri &tri : node->tribuf->tris) {
    BMVert *verts[3] = {
        (BMVert *)node->tribuf->verts[tri.v[0]].i,
        (BMVert *)node->tribuf->verts[tri.v[1]].i,
        (BMVert *)node->tribuf->verts[tri.v[2]].i,
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
        *r_active_face = tri.f;
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
  for (PBVHTri &tri : node->tribuf->tris) {
    BMVert *v1 = (BMVert *)node->tribuf->verts[tri.v[0]].i;
    BMVert *v2 = (BMVert *)node->tribuf->verts[tri.v[1]].i;
    BMVert *v3 = (BMVert *)node->tribuf->verts[tri.v[2]].i;
    BMFace *f = (BMFace *)tri.f.i;

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

  for (PBVHTri &tri : tribuf->tris) {
    BMFace *f = (BMFace *)tri.f.i;

    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    BMVert *v1 = (BMVert *)tribuf->verts[tri.v[0]].i;
    BMVert *v2 = (BMVert *)tribuf->verts[tri.v[1]].i;
    BMVert *v3 = (BMVert *)tribuf->verts[tri.v[2]].i;

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
  (!(v)->e || BM_ELEM_CD_GET_INT((v), cd_vert_node_offset) != node_nr || \
   ((*BM_ELEM_CD_PTR<uint8_t *>((v), data->cd_flag)) & SCULPTFLAG_PBVH_BOUNDARY))

  for (BMVert *v : *node->bm_unique_verts) {
    PBVH_CHECK_NAN(v->no);

    if (NORMAL_VERT_BAD(v)) {
      data->border_verts.append(v);
    }

    zero_v3(v->no);
  }

  for (BMVert *v : *node->bm_other_verts) {
    PBVH_CHECK_NAN(v->no);

    if (NORMAL_VERT_BAD(v)) {
      data->border_verts.append(v);
    }

    zero_v3(v->no);
  }

  for (BMFace *f : *node->bm_faces) {
    BM_face_normal_update(f);

    PBVH_CHECK_NAN(f->no);

    BMLoop *l = f->l_first;
    do {
      PBVH_CHECK_NAN(l->v->no);

      if (!NORMAL_VERT_BAD(l->v)) {
        add_v3_v3(l->v->no, f->no);
      }
    } while ((l = l->next) != f->l_first);
  }

  for (BMVert *v : *node->bm_unique_verts) {
    PBVH_CHECK_NAN(v->no);

    if (!NORMAL_VERT_BAD(v)) {
      normalize_v3(v->no);
    }
  }

  node->flag &= ~PBVH_UpdateNormals;
}

void pbvh_bmesh_normals_update(PBVH *pbvh, Span<PBVHNode *> nodes)
{
  TaskParallelSettings settings;
  Vector<UpdateNormalsTaskData> datas;
  datas.resize(nodes.size());

  for (int i : nodes.index_range()) {
    datas[i].node = nodes[i];
    datas[i].node_nr = nodes[i] - pbvh->nodes;
    datas[i].cd_flag = pbvh->cd_flag;
    datas[i].cd_vert_node_offset = pbvh->cd_vert_node_offset;
    datas[i].cd_face_node_offset = pbvh->cd_face_node_offset;

    BKE_pbvh_bmesh_check_tris(pbvh, nodes[i]);
  }

  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), datas.data(), pbvh_update_normals_task_cb, &settings);

  for (UpdateNormalsTaskData &data : datas) {
    for (BMVert *v : data.border_verts) {
      if (BM_elem_is_free((BMElem *)v, BM_VERT)) {
        printf("%s: error, v was freed!\n", __func__);
        continue;
      }

      if (!v->e || !v->e->l) {
        continue;
      }

      zero_v3(v->no);

      BMEdge *e = v->e;
      do {
        BMLoop *l = e->l;
        if (!l) {
          continue;
        }

        do {
          add_v3_v3(v->no, l->f->no);
        } while ((l = l->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      normalize_v3(v->no);
    }
  }
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

      /* Found a face that should be part of another node, look for a face to substitute with. */
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

        /* Increase both counts. */
        num_child1++;
        num_child2++;
      }
      else {
        /* Not finding candidate means second half of array part is full of
         * second node parts, just increase the number of child nodes for it.
         */
        num_child2++;
      }
    }
    else {
      num_child1++;
    }
  }

  /* Ensure at least one child in each node. */
  if (num_child2 == 0) {
    num_child2++;
    num_child1--;
  }
  else if (num_child1 == 0) {
    num_child1++;
    num_child2--;
  }

  /* At this point, faces should have been split along the array range sequentially,
   * each sequential part belonging to one node only.
   */
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

  n->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces", node->totface);

  /* Create vert hash sets */
  n->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts", node->totface * 3);
  n->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts", node->totface * 3);

  BB_reset(&n->vb);

  const int end = node->start + node->totface;

  for (int i = node->start; i < end; i++) {
    BMFace *f = nodeinfo[i];
    BBC *bbc = &bbc_array[BM_elem_index_get(f)];

    /* Update ownership of faces */

    n->bm_faces->add(f);
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      BMVert *v = l_iter->v;

      int old = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
      ;
      int32_t *ptr = static_cast<int32_t *>(
          POINTER_OFFSET(v->head.data, pbvh->cd_vert_node_offset));

      if (BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
        if (old == DYNTOPO_NODE_NONE &&
            atomic_cas_int32(ptr, DYNTOPO_NODE_NONE, node_index) == DYNTOPO_NODE_NONE)
        {
          n->bm_unique_verts->add(v);
        }
        else {
          n->bm_other_verts->add(v);
        }
      }
      else if (old != node->node_index) {
        n->bm_other_verts->add(v);
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

static bool test_colinear_tri(BMFace *f)
{
  BMLoop *l = f->l_first;

  float area_limit = 0.00001f;
  area_limit = len_squared_v3v3(l->v->co, l->next->v->co) * 0.001;

  return area_tri_v3(l->v->co, l->next->v->co, l->prev->v->co) <= area_limit;
}

namespace blender::bke::pbvh {

float test_sharp_faces_bmesh(BMFace *f1, BMFace *f2, float limit)
{
  float angle = saacos(dot_v3v3(f1->no, f2->no));

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

static void update_edge_boundary_bmesh_uv(BMEdge *e, int cd_edge_boundary, const CustomData *ldata)
{
  int base = ldata->typemap[CD_PROP_FLOAT2];

  *BM_ELEM_CD_PTR<int *>(e, cd_edge_boundary) &= ~(SCULPT_BOUNDARY_UPDATE_UV | SCULPT_BOUNDARY_UV);

  for (int i = base; i < ldata->totlayer; i++) {
    const CustomDataLayer &layer = ldata->layers[i];

    if (layer.type != CD_PROP_FLOAT2) {
      break;
    }
    if (layer.flag & (CD_FLAG_TEMPORARY | CD_FLAG_ELEM_NOINTERP)) {
      continue;
    }

    BMLoop *l = e->l;

    int cd_uv = layer.offset;
    float limit = blender::bke::sculpt::calc_uv_snap_limit(l, cd_uv);
    limit *= limit;

    float2 a1 = BM_ELEM_CD_PTR<float *>((l->v == e->v1 ? l : l->next), cd_uv);
    float2 a2 = BM_ELEM_CD_PTR<float *>((l->v == e->v2 ? l : l->next), cd_uv);

    do {
      float *b1 = BM_ELEM_CD_PTR<float *>((l->v == e->v1 ? l : l->next), cd_uv);
      float *b2 = BM_ELEM_CD_PTR<float *>((l->v == e->v2 ? l : l->next), cd_uv);

      if (len_squared_v2v2(a1, b1) > limit || len_squared_v2v2(a2, b2) > limit) {
        *BM_ELEM_CD_PTR<int *>(e, cd_edge_boundary) |= SCULPT_BOUNDARY_UV;
        return;
      }
    } while ((l = l->radial_next) != e->l);
  }
}

void update_edge_boundary_bmesh(BMEdge *e,
                                int cd_faceset_offset,
                                int cd_edge_boundary,
                                const int cd_flag,
                                const int cd_valence,
                                const CustomData *ldata,
                                float sharp_angle_limit)
{
  int oldflag = BM_ELEM_CD_GET_INT(e, cd_edge_boundary);

  if (e->l && (oldflag & SCULPT_BOUNDARY_UPDATE_UV) && ldata->typemap[CD_PROP_FLOAT2] != -1) {
    update_edge_boundary_bmesh_uv(e, cd_edge_boundary, ldata);
    oldflag = BM_ELEM_CD_GET_INT(e, cd_edge_boundary);
  }

  if (oldflag & SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE) {
    if (e->l && e->l != e->l->radial_next &&
        test_sharp_faces_bmesh(e->l->f, e->l->radial_next->f, sharp_angle_limit))
    {
      *BM_ELEM_CD_PTR<int *>(e, cd_edge_boundary) |= SCULPT_BOUNDARY_SHARP_ANGLE;
    }
    else {
      *BM_ELEM_CD_PTR<int *>(e, cd_edge_boundary) &= ~SCULPT_BOUNDARY_SHARP_ANGLE;
    }
    oldflag = BM_ELEM_CD_GET_INT(e, cd_edge_boundary);
  }

  if (!(oldflag & SCULPT_BOUNDARY_NEEDS_UPDATE)) {
    return;
  }

  int newflag = oldflag & (SCULPT_BOUNDARY_UV | SCULPT_BOUNDARY_SHARP_ANGLE);

  if (!e->l || e->l == e->l->radial_next) {
    newflag |= SCULPT_BOUNDARY_MESH;
  }

  if (e->l && e->l != e->l->radial_next && cd_faceset_offset != -1) {
    int fset1 = BM_ELEM_CD_GET_INT(e->l->f, cd_faceset_offset);
    int fset2 = BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset);

    newflag |= fset1 != fset2 ? SCULPT_BOUNDARY_FACE_SET : 0;
  }

  newflag |= !BM_elem_flag_test(e, BM_ELEM_SMOOTH) ? SCULPT_BOUNDARY_SHARP_MARK : 0;
  newflag |= BM_elem_flag_test(e, BM_ELEM_SEAM) ? SCULPT_BOUNDARY_SEAM : 0;

  *BM_ELEM_CD_PTR<int *>(e, cd_edge_boundary) = newflag;
}
}  // namespace blender::bke::pbvh

static void pbvh_bmesh_update_uv_boundary_intern(BMVert *v, int cd_boundary, int cd_uv)
{
  int *boundflag = BM_ELEM_CD_PTR<int *>(v, cd_boundary);

  if (!v->e) {
    return;
  }

  BMEdge *e = v->e;
  do {
    BMLoop *l = e->l;
    if (!l) {
      continue;
    }

    float snap_limit = blender::bke::sculpt::calc_uv_snap_limit(l, cd_uv);
    snap_limit *= snap_limit;

    float2 uv1a = *BM_ELEM_CD_PTR<float2 *>(l->v == v ? l : l->next, cd_uv);
    float2 uv1b = *BM_ELEM_CD_PTR<float2 *>(l->v == v ? l->next : l, cd_uv);

    do {
      float2 uv2a = *BM_ELEM_CD_PTR<float2 *>(l->v == v ? l : l->next, cd_uv);
      float2 uv2b = *BM_ELEM_CD_PTR<float2 *>(l->v == v ? l->next : l, cd_uv);

      if (len_squared_v2v2(uv1a, uv2a) > snap_limit || len_squared_v2v2(uv1b, uv2b) > snap_limit) {
        *boundflag |= SCULPT_BOUNDARY_UV;
      }

      /* Corners are calculated from the number of distinct charts. */
      BMLoop *l2 = l->v == v ? l : l->next;
      if (blender::bke::sculpt::loop_is_corner(l2, cd_uv)) {
        *boundflag |= SCULPT_CORNER_UV;
        *boundflag |= SCULPT_BOUNDARY_UV;
      }

    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

static void pbvh_bmesh_update_uv_boundary(BMVert *v, int cd_boundary, const CustomData *ldata)
{
  int base_uv = ldata->typemap[CD_PROP_FLOAT2];
  int newflag = 0;

  *BM_ELEM_CD_PTR<int *>(v, cd_boundary) &= ~(SCULPT_BOUNDARY_UPDATE_UV | SCULPT_BOUNDARY_UV |
                                              SCULPT_CORNER_UV);

  if (base_uv == -1) {
    return;
  }

  for (int i = base_uv; i < ldata->totlayer; i++) {
    if (ldata->layers[i].type != CD_PROP_FLOAT2) {
      break;
    }
    if (ldata->layers[i].flag & (CD_FLAG_TEMPORARY | CD_FLAG_ELEM_NOINTERP)) {
      continue;
    }

    pbvh_bmesh_update_uv_boundary_intern(v, cd_boundary, ldata->layers[i].offset);
  }
}

namespace blender::bke::pbvh {
void update_vert_boundary_bmesh(int cd_faceset_offset,
                                int cd_vert_node_offset,
                                int cd_face_node_offset,
                                int /*cd_vcol*/,
                                int cd_boundary_flag,
                                const int cd_flag,
                                const int cd_valence,
                                BMVert *v,
                                const CustomData *ldata,
                                float sharp_angle_limit)
{
  int newflag = *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag);
  newflag &= ~(SCULPTFLAG_VERT_FSET_HIDDEN | SCULPTFLAG_PBVH_BOUNDARY);

  if (BM_ELEM_CD_GET_INT(v, cd_boundary_flag) & SCULPT_BOUNDARY_UPDATE_UV) {
    pbvh_bmesh_update_uv_boundary(v, cd_boundary_flag, ldata);
  }

  int boundflag = BM_ELEM_CD_GET_INT(v, cd_boundary_flag) &
                  (SCULPT_BOUNDARY_UV | SCULPT_CORNER_UV);

  BMEdge *e = v->e;
  if (!e) {
    boundflag |= SCULPT_BOUNDARY_MESH;

    BM_ELEM_CD_SET_INT(v, cd_valence, 0);
    BM_ELEM_CD_SET_INT(v, cd_boundary_flag, boundflag);
    *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag) = newflag;

    return;
  }

  int val = 0;

  int ni = BM_ELEM_CD_GET_INT(v, cd_vert_node_offset);

  int sharpcount = 0;
  int seamcount = 0;
  int quadcount = 0;

  Vector<int, 16> fsets;

  int sharp_angle_num = 0;
  do {
    BMVert *v2 = v == e->v1 ? e->v2 : e->v1;

    if (BM_ELEM_CD_GET_INT(v2, cd_vert_node_offset) != ni) {
      newflag |= SCULPTFLAG_PBVH_BOUNDARY;
    }

    if (e->l && e->l != e->l->radial_next) {
      if (blender::bke::pbvh::test_sharp_faces_bmesh(
              e->l->f, e->l->radial_next->f, sharp_angle_limit)) {
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

    if (!(e->head.hflag & BM_ELEM_SMOOTH)) {
      boundflag |= SCULPT_BOUNDARY_SHARP_MARK;
      sharpcount++;

      if (sharpcount > 2) {
        boundflag |= SCULPT_CORNER_SHARP_MARK;
      }
    }

    if (e->l) {
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
        int fset = BM_ELEM_CD_GET_INT(e->l->f, cd_faceset_offset);
        if (!fsets.contains(fset)) {
          fsets.append(fset);
        }
      }

      /* Also check e->l->radial_next, in case we are not manifold
       * which can mess up the loop order
       */
      if (e->l->radial_next != e->l) {
        if (cd_faceset_offset != -1) {
          int fset = BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset);

          if (!fsets.contains(fset)) {
            fsets.append(fset);
          }
        }

        if (e->l->radial_next->f->len > 3) {
          newflag |= SCULPTFLAG_NEED_TRIANGULATE;
        }
      }
    }

    if (e->l && e->l->radial_next == e->l) {
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

  BM_ELEM_CD_SET_INT(v, cd_boundary_flag, boundflag);
  BM_ELEM_CD_SET_INT(v, cd_valence, val);
  *(BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag)) = newflag;
}

void sharp_limit_set(PBVH *pbvh, float limit)
{
  pbvh->sharp_angle_limit = limit;
}

}  // namespace blender::bke::pbvh

/*Used by symmetrize to update boundary flags*/
void BKE_pbvh_recalc_bmesh_boundary(PBVH *pbvh)
{
  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    blender::bke::pbvh::update_vert_boundary_bmesh(pbvh->cd_faceset_offset,
                                                   pbvh->cd_vert_node_offset,
                                                   pbvh->cd_face_node_offset,
                                                   pbvh->cd_vcol_offset,
                                                   pbvh->cd_boundary_flag,
                                                   pbvh->cd_flag,
                                                   pbvh->cd_valence,
                                                   v,
                                                   &pbvh->header.bm->ldata,
                                                   pbvh->sharp_angle_limit);
  }
}

void BKE_pbvh_set_idmap(PBVH *pbvh, BMIdMap *idmap)
{
  pbvh->bm_idmap = idmap;
}

#if 0
PBVH *global_debug_pbvh = nullptr;

extern "C" void debug_pbvh_on_vert_kill(BMVert *v)
{
  PBVH *pbvh = global_debug_pbvh;

  if (!pbvh) {
    return;
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode &node = pbvh->nodes[i];

    if (!(node.flag & PBVH_Leaf)) {
      continue;
    }

    for (BMVert *v2 : *node.bm_unique_verts) {
      if (v2 == v) {
        printf("Error! Vertex %p is still in bm_unique_verts\n", v);
      }
    }
    for (BMVert *v2 : *node.bm_other_verts) {
      if (v2 == v) {
        printf("Error! Vertex still in bm_other_verts\n");
      }
    }
  }
}
#endif

/* Build a PBVH from a BMesh */
void BKE_pbvh_build_bmesh(PBVH *pbvh,
                          Mesh *me,
                          BMesh *bm,
                          BMLog *log,
                          BMIdMap *idmap,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset,
                          const int cd_face_areas,
                          const int cd_boundary_flag,
                          const int cd_edge_boundary,
                          const int /*cd_flag_offset*/,
                          const int /*cd_valence_offset*/,
                          const int cd_origco,
                          const int cd_origno)
{
  pbvh->bm_idmap = idmap;

  pbvh->cd_face_area = cd_face_areas;
  pbvh->cd_vert_node_offset = cd_vert_node_offset;
  pbvh->cd_face_node_offset = cd_face_node_offset;
  pbvh->cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  pbvh->cd_boundary_flag = cd_boundary_flag;
  pbvh->cd_edge_boundary = cd_edge_boundary;
  pbvh->cd_origco = cd_origco;
  pbvh->cd_origno = cd_origno;

  pbvh->cd_flag = CustomData_get_offset_named(
      &pbvh->header.bm->vdata, CD_PROP_INT8, ".sculpt_flags");
  pbvh->cd_valence = CustomData_get_offset_named(
      &pbvh->header.bm->vdata, CD_PROP_INT8, ".sculpt_valence");

  pbvh->mesh = me;

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

    /* Check for currupted faceset. */
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
    for (BMFace *f : *node->bm_faces) {
      float *areabuf = BM_ELEM_CD_PTR<float *>(f, cd_face_area);
      areabuf[area_dst_i] = areabuf[area_src_i];
    }
  }

  pbvh_bmesh_check_nodes(pbvh);
}

void BKE_pbvh_set_bm_log(PBVH *pbvh, BMLog *log)
{
  pbvh->bm_log = log;
  BM_log_set_idmap(log, pbvh->bm_idmap);
}

namespace blender::bke::dyntopo {
bool remesh_topology_nodes(blender::bke::dyntopo::BrushTester *brush_tester,
                           Object *ob,
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
                             ob,
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

PBVHTriBuf *BKE_pbvh_bmesh_get_tris(PBVH *pbvh, PBVHNode *node)
{
  BKE_pbvh_bmesh_check_tris(pbvh, node);

  return node->tribuf;
}

void BKE_pbvh_bmesh_free_tris(PBVH * /*pbvh*/, PBVHNode *node)
{
  if (node->tribuf) {
    MEM_delete<PBVHTriBuf>(node->tribuf);
    node->tribuf = nullptr;
  }

  if (node->tri_buffers) {
    MEM_delete<blender::Vector<PBVHTriBuf>>(node->tri_buffers);
    node->tri_buffers = nullptr;
  }
}

BLI_INLINE void pbvh_tribuf_add_vert(PBVHTriBuf *tribuf, PBVHVertRef vertex, BMLoop *l)
{
  tribuf->verts.append(vertex);
  tribuf->loops.append((uintptr_t)l);
}

BLI_INLINE void pbvh_tribuf_add_edge(PBVHTriBuf *tribuf, int v1, int v2)
{
  tribuf->edges.append(v1);
  tribuf->edges.append(v2);
}

void pbvh_bmesh_check_other_verts(PBVHNode *node)
{
  if (!(node->flag & PBVH_UpdateOtherVerts)) {
    return;
  }

  node->flag &= ~PBVH_UpdateOtherVerts;

  if (node->bm_other_verts) {
    MEM_delete(node->bm_other_verts);
  }

  node->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");

  for (BMFace *f : *node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      if (!node->bm_unique_verts->contains(l->v)) {
        node->bm_other_verts->add(l->v);
      }
    } while ((l = l->next) != f->l_first);
  }
}

static void pbvh_init_tribuf(PBVHNode * /*node*/, PBVHTriBuf *tribuf)
{
  tribuf->mat_nr = 0;

  tribuf->edges.clear();
  tribuf->verts.clear();
  tribuf->tris.clear();
  tribuf->loops.clear();
}

static uintptr_t tri_loopkey(BMLoop *l, int mat_nr, int cd_fset, int cd_uvs[], int totuv)
{
  uintptr_t key = ((uintptr_t)l->v) << 12ULL;
  int i = 0;

  key ^= (uintptr_t)BLI_hash_int(mat_nr + i++);

  if (cd_fset >= 0) {
    // key ^= (uintptr_t)BLI_hash_int(BM_ELEM_CD_GET_INT(l->f, cd_fset));
    key ^= (uintptr_t)BLI_hash_int(BM_ELEM_CD_GET_INT(l->f, cd_fset) + i++);
  }

  for (int i = 0; i < totuv; i++) {
    float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uvs[i]);
    float snap = 4196.0f;

    uintptr_t x = (uintptr_t)(luv[0] * snap);
    uintptr_t y = (uintptr_t)(luv[1] * snap);

    uintptr_t key2 = y * snap + x;
    key ^= BLI_hash_int(key2 + i++);
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

  node->flag |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;

  int totuv = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
  int *cd_uvs = (int *)BLI_array_alloca(cd_uvs, totuv);

  for (int i = 0; i < totuv; i++) {
    int idx = CustomData_get_layer_index_n(&bm->ldata, CD_PROP_FLOAT2, i);
    cd_uvs[i] = bm->ldata.layers[idx].offset;
  }

  int mat_map[MAXMAT];

  for (int i = 0; i < MAXMAT; i++) {
    mat_map[i] = -1;
  }

  if (node->tribuf || node->tri_buffers) {
    BKE_pbvh_bmesh_free_tris(pbvh, node);
  }

  node->tribuf = MEM_new<PBVHTriBuf>("node->tribuf");
  pbvh_init_tribuf(node, node->tribuf);

  Vector<BMLoop *, 128> loops;
  Vector<blender::uint3, 128> loops_idx;
  Vector<PBVHTriBuf> *tribufs = MEM_new<Vector<PBVHTriBuf>>("PBVHTriBuf tribufs");

  node->flag &= ~PBVH_UpdateTris;

  const int edgeflag = BM_ELEM_TAG_ALT;

  float min[3], max[3];

  INIT_MINMAX(min, max);

  for (BMFace *f : *node->bm_faces) {
    if (pbvh_poly_hidden(pbvh, f)) {
      continue;
    }

    /* Set edgeflag for building edge indices later. */
    BMLoop *l = f->l_first;
    do {
      l->e->head.hflag |= edgeflag;
    } while ((l = l->next) != f->l_first);

    const int mat_nr = f->mat_nr;

    if (mat_map[mat_nr] == -1) {
      PBVHTriBuf _tribuf = {};

      mat_map[mat_nr] = tribufs->size();

      pbvh_init_tribuf(node, &_tribuf);
      _tribuf.mat_nr = mat_nr;
      tribufs->append(_tribuf);
    }

    int tottri = (f->len - 2);

    loops.resize(f->len);
    loops_idx.resize(tottri);

    BM_face_calc_tessellation(f, true, loops.data(), (uint(*)[3])loops_idx.data());

    auto add_tri_verts =
        [cd_uvs, totuv, pbvh, &min, &max](
            PBVHTriBuf *tribuf, PBVHTri &tri, BMLoop *l, BMLoop *l2, int mat_nr, int j) {
          int tri_v;

          if ((l->f->head.hflag & BM_ELEM_SMOOTH)) {
            void *loopkey = reinterpret_cast<void *>(
                tri_loopkey(l, mat_nr, pbvh->cd_faceset_offset, cd_uvs, totuv));

            tri_v = tribuf->vertmap.lookup_or_add(loopkey, tribuf->verts.size());
          }
          else { /* Flat shaded faces. */
            tri_v = tribuf->verts.size();
          }

          /* Newly added to the set? */
          if (tri_v == tribuf->verts.size()) {
            PBVHVertRef sv = {(intptr_t)l->v};
            minmax_v3v3_v3(min, max, l->v->co);
            pbvh_tribuf_add_vert(tribuf, sv, l);
          }

          tri.v[j] = (intptr_t)tri_v;
          tri.l[j] = (intptr_t)l;
        };

    /* Build index buffers. */
    for (int i = 0; i < tottri; i++) {
      PBVHTriBuf *mat_tribuf = &(*tribufs)[mat_map[mat_nr]];

      node->tribuf->tris.resize(node->tribuf->tris.size() + 1);
      mat_tribuf->tris.resize(mat_tribuf->tris.size() + 1);

      PBVHTri &tri = node->tribuf->tris.last();
      PBVHTri &mat_tri = mat_tribuf->tris.last();

      tri.eflag = mat_tri.eflag = 0;

      for (int j = 0; j < 3; j++) {
        BMLoop *l1 = loops[loops_idx[i][j]];
        BMLoop *l2 = loops[loops_idx[i][(j + 1) % 3]];

        add_tri_verts(node->tribuf, tri, l1, l2, mat_nr, j);
        add_tri_verts(mat_tribuf, mat_tri, l1, l2, mat_nr, j);
      }

      for (int j = 0; j < 3; j++) {
        BMLoop *l1 = loops[loops_idx[i][j]];
        BMLoop *l2 = loops[loops_idx[i][(j + 1) % 3]];
        BMEdge *e = nullptr;

        if (e = BM_edge_exists(l1->v, l2->v)) {
          tri.eflag |= 1 << j;

          if (e->head.hflag & edgeflag) {
            e->head.hflag &= ~edgeflag;
            pbvh_tribuf_add_edge(node->tribuf, tri.v[j], tri.v[(j + 1) % 3]);
            pbvh_tribuf_add_edge(mat_tribuf, tri.v[j], tri.v[(j + 1) % 3]);
          }
        }
      }

      copy_v3_v3(tri.no, f->no);
      copy_v3_v3(mat_tri.no, f->no);
      tri.f.i = (intptr_t)f;
      mat_tri.f.i = (intptr_t)f;
    }
  }
  /*
                void *loopkey = reinterpret_cast<void *>(
                tri_loopkey(l, mat_nr, pbvh->cd_faceset_offset, cd_uvs, totuv));
   */

  bm->elem_index_dirty |= BM_VERT;

  node->tri_buffers = tribufs;

  if (node->tribuf->verts.size()) {
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
    n->subtree_tottri = n->bm_faces->size();
    return n->subtree_tottri;
  }

  int ni = n->children_offset;

  int ret = pbvh_count_subtree_verts(pbvh, pbvh->nodes + ni);
  ret += pbvh_count_subtree_verts(pbvh, pbvh->nodes + ni + 1);

  n->subtree_tottri = ret;

  return ret;
}

void BKE_pbvh_bmesh_update_all_valence(PBVH *pbvh)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
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

  for (BMVert *v : *node->bm_unique_verts) {
    parent->bm_unique_verts->add(v);

    int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
    *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

    BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }

  for (BMFace *f : *node->bm_faces) {
    parent->bm_faces->add(f);
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
}

static void BKE_pbvh_bmesh_correct_tree(PBVH *pbvh, PBVHNode *node, PBVHNode * /*parent*/)
{
  const int size_lower = pbvh->leaf_limit - (pbvh->leaf_limit >> 1);

  if (node->flag & PBVH_Leaf) {
    return;
  }

  if (node->subtree_tottri < size_lower && node != pbvh->nodes) {
    node->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
    node->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
    node->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");

    pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + node->children_offset, node);
    pbvh_bmesh_join_subnodes(pbvh, pbvh->nodes + node->children_offset + 1, node);

    node->children_offset = 0;
    node->flag |= PBVH_Leaf | PBVH_UpdateRedraw | PBVH_UpdateBB | PBVH_UpdateDrawBuffers |
                  PBVH_RebuildDrawBuffers | PBVH_UpdateOriginalBB | PBVH_UpdateMask |
                  PBVH_UpdateVisibility | PBVH_UpdateColor | PBVH_UpdateNormals | PBVH_UpdateTris;

    DyntopoSet<BMVert> *other = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");

    node->children_offset = 0;
    node->draw_batches = nullptr;

    /* Rebuild bm_other_verts. */
    for (BMFace *f : *node->bm_faces) {
      BMLoop *l = f->l_first;

      if (BM_elem_is_free((BMElem *)f, BM_FACE)) {
        printf("%s: corrupted face %p.\n", __func__, f);
        node->bm_faces->remove(f);
        continue;
      }

      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

      do {
        if (!node->bm_unique_verts->contains(l->v)) {
          other->add(l->v);
        }
        l = l->next;
      } while (l != f->l_first);
    }

    MEM_delete(node->bm_other_verts);
    node->bm_other_verts = other;

    BB_reset(&node->vb);

    for (BMVert *v : *node->bm_unique_verts) {
      BB_expand(&node->vb, v->co);
    }

    for (BMVert *v : *node->bm_other_verts) {
      BB_expand(&node->vb, v->co);
    }

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
          n3->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
          n3->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
          n3->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");
          n3->tribuf = nullptr;
          n3->draw_batches = nullptr;
        }
        else if ((n1->flag & PBVH_Delete) && (n2->flag & PBVH_Delete)) {
          n->children_offset = 0;
          n->flag |= PBVH_Leaf | PBVH_UpdateTris;

          if (!n->bm_unique_verts) {
            // should not happen
            n->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
            n->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
            n->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");
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
        MEM_delete(n->bm_unique_verts);
        n->bm_unique_verts = nullptr;
      }

      if (n->bm_other_verts) {
        MEM_delete(n->bm_other_verts);
        n->bm_other_verts = nullptr;
      }

      if (n->bm_faces) {
        MEM_delete(n->bm_faces);
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
      printf("%s: pbvh error\n", __func__);

      n->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
      n->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
      n->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");
    }

    for (BMVert *v : *n->bm_unique_verts) {
      BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }

    for (BMFace *f : *n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, bvh->cd_face_node_offset, i);
    }
  }

  Vector<BMVert *> scratch;

  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    scratch.clear();

    for (BMVert *v : *n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        scratch.append(v);
      }
    }

    int slen = scratch.size();
    for (int j = 0; j < slen; j++) {
      BMVert *v = scratch[j];

      n->bm_other_verts->remove(v);
      n->bm_unique_verts->add(v);
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

      factor = 0.2f;

      bool bad = overlap > volume * factor;

      bad |= child1->bm_faces && !child1->bm_faces->size();
      bad |= child2->bm_faces && !child2->bm_faces->size();

      if (bad) {
        modified = true;

        substack.clear();
        substack.append(child1);
        substack.append(child2);

        while (substack.size() > 0) {
          PBVHNode *node2 = substack.pop_last();

          node2->flag |= PBVH_Delete;

          if (node2->flag & PBVH_Leaf) {
            for (BMFace *f : *node2->bm_faces) {
              if (BM_ELEM_CD_GET_INT(f, cd_face_node) == -1) {
                // eek!
                continue;
              }

              BM_ELEM_CD_SET_INT(f, cd_face_node, DYNTOPO_NODE_NONE);
              faces.append(f);
            }

            for (BMVert *v : *node2->bm_unique_verts) {
              int *flags = BM_ELEM_CD_PTR<int *>(v, pbvh->cd_boundary_flag);
              *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;

              BM_ELEM_CD_SET_INT(v, cd_vert_node, DYNTOPO_NODE_NONE);
            }
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

    if (!(node->flag & PBVH_Leaf) || (node->flag & PBVH_Delete) || node->bm_faces->size() != 0) {
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

      if (!(a->flag & PBVH_Delete) && (a->flag & PBVH_Leaf) && a->bm_faces->size() > 1) {
        other = a;
      }
      else if (!(b->flag & PBVH_Delete) && (b->flag & PBVH_Leaf) && b->bm_faces->size() > 1) {
        other = b;
      }
      else {
        other = nullptr;
        break;
      }
    }

    if (other == nullptr || other->bm_faces->size() < 1) {
      printf("%s: other was nullptr\n", __func__);
      continue;
    }

    /* Steal a single face from other */
    PBVHNodeFlags updateflag = PBVH_UpdateOtherVerts | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                               PBVH_UpdateTris | PBVH_UpdateTriAreas | PBVH_RebuildDrawBuffers |
                               PBVH_RebuildNodeVerts | PBVH_RebuildPixels | PBVH_UpdateNormals |
                               PBVH_UpdateCurvatureDir | PBVH_UpdateRedraw | PBVH_UpdateVisibility;

    for (BMFace *f : *other->bm_faces) {
      other->bm_faces->remove(f);
      node->bm_faces->add(f);
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);

      BMVert *v = f->l_first->v;
      int node_i = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
      if (node_i != DYNTOPO_NODE_NONE) {
        PBVHNode *node2 = pbvh->nodes + node_i;
        if (node2->bm_unique_verts->contains(v)) {
          node2->bm_unique_verts->remove(v);
        }
      }

      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
      node->bm_unique_verts->add(v);

      node->flag |= updateflag;
      other->flag |= updateflag;

      printf("%s: Patched empty leaf node.\n", __func__);
      break;
    }
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
        MEM_delete(n->bm_unique_verts);
        n->bm_unique_verts = nullptr;
      }

      if (n->bm_other_verts) {
        MEM_delete(n->bm_other_verts);
        n->bm_other_verts = nullptr;
      }

      if (n->bm_faces) {
        MEM_delete(n->bm_faces);
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
      printf("%s: pbvh error.\n", __func__);
      n->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_unique_verts");
      n->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
      n->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");
    }

    for (BMVert *v : *n->bm_unique_verts) {
      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, i);
    }

    for (BMFace *f : *n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);
    }
  }

  Vector<BMVert *> scratch;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *n = pbvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    scratch.clear();

    for (BMVert *v : *n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        scratch.append(v);
      }
    }

    for (int j : IndexRange(scratch.size())) {
      BMVert *v = scratch[j];

      n->bm_other_verts->remove(v);
      n->bm_unique_verts->add(v);
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

DyntopoSet<BMVert> *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node)
{
  return node->bm_unique_verts;
}

DyntopoSet<BMVert> *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node)
{
  pbvh_bmesh_check_other_verts(node);
  return node->bm_other_verts;
}

DyntopoSet<BMFace> *BKE_pbvh_bmesh_node_faces(PBVHNode *node)
{
  return node->bm_faces;
}

/****************************** Debugging *****************************/

void BKE_pbvh_update_offsets(PBVH *pbvh,
                             const int cd_vert_node_offset,
                             const int cd_face_node_offset,
                             const int cd_face_areas,
                             const int cd_boundary_flag,
                             const int cd_edge_boundary,
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

  CustomData *ldata = &pbvh->header.bm->ldata;
  pbvh->totuv = 0;
  for (int i : IndexRange(ldata->totlayer)) {
    CustomDataLayer &layer = ldata->layers[i];
    if (layer.type == CD_PROP_FLOAT2 && !(layer.flag & CD_FLAG_TEMPORARY)) {
      pbvh->totuv++;
    }
  }

  pbvh->cd_boundary_flag = cd_boundary_flag;
  pbvh->cd_edge_boundary = cd_edge_boundary;

  pbvh->cd_curvature_dir = cd_curvature_dir;

  if (pbvh->bm_idmap) {
    BM_idmap_check_attributes(pbvh->bm_idmap);
  }

  pbvh->cd_flag = cd_flag;
  pbvh->cd_valence = cd_valence;
  pbvh->cd_origco = cd_origco;
  pbvh->cd_origno = cd_origno;
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

    // MEM_delete<
    MEM_delete(node->bm_unique_verts);
    MEM_delete(node->bm_faces);

    if (node->bm_other_verts) {
      MEM_delete(node->bm_other_verts);
    }

    node->bm_other_verts = MEM_new<DyntopoSet<BMVert>>("bm_other_verts");
    node->flag |= PBVH_UpdateOtherVerts;

    node->bm_faces = MEM_new<DyntopoSet<BMFace>>("bm_faces");
    node->bm_unique_verts = MEM_new<DyntopoSet<BMVert>>("bm_verts");

    int j = 0;
    int *data = node->prim_indices;

    while (data[j] != -1 && j < node->totprim) {
      BMFace *f = pbvh->header.bm->ftable[data[j]];
      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, i);

      node->bm_faces->add(f);
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

      node->bm_unique_verts->add(v);
      j++;
    }

    MEM_SAFE_FREE(node->prim_indices);

    // don't try to load invalid triangulation
    if (node->flag & PBVH_UpdateTris) {
      continue;
    }

    for (j = 0; j < node->tri_buffers->size() + 1; j++) {
      PBVHTriBuf *tribuf = j == node->tri_buffers->size() ? node->tribuf :
                                                            &((*node->tri_buffers)[j]);

      if (!tribuf) {
        break;
      }

      for (int k = 0; k < tribuf->verts.size(); k++) {
        tribuf->verts[k].i = (intptr_t)pbvh->header.bm->vtable[tribuf->verts[k].i];
      }

      for (int k = 0; k < tribuf->loops.size(); k++) {
        tribuf->loops[k] = (uintptr_t)ltable[tribuf->loops[k]];
      }

      for (PBVHTri &tri : tribuf->tris) {
        for (int l = 0; l < 3; l++) {
          tri.l[l] = (uintptr_t)ltable[tri.l[l]];
        }

        tri.f.i = (intptr_t)pbvh->header.bm->ftable[tri.f.i];
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

void update_sharp_vertex_bmesh(BMVert *v, int cd_boundary_flag, const float sharp_angle_limit)
{
  int flag = BM_ELEM_CD_GET_INT(v, cd_boundary_flag);
  flag &= ~(SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE | SCULPT_BOUNDARY_SHARP_ANGLE |
            SCULPT_CORNER_SHARP_ANGLE);

  if (!v->e) {
    BM_ELEM_CD_SET_INT(v, cd_boundary_flag, flag);
    return;
  }

  int sharp_num = 0;

  BMEdge *e = v->e;
  do {
    if (!e->l || e->l == e->l->radial_next) {
      continue;
    }

    if (blender::bke::pbvh::test_sharp_faces_bmesh(
            e->l->f, e->l->radial_next->f, sharp_angle_limit)) {
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
