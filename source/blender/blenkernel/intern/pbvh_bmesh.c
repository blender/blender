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
 * \ingroup bli
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

#include "BLI_array.h"
#include "BLI_buffer.h"
#include "BLI_ghash.h"
#include "BLI_heap_simple.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_sort_utils.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"
#include "atomic_ops.h"

#include "DNA_material_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_pbvh.h"

#include "GPU_buffers.h"

#include "bmesh.h"
#include "pbvh_intern.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>

static void _debugprint(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void pbvh_bmesh_check_nodes_simple(PBVH *pbvh)
{
#if 0
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
#endif
}

void pbvh_bmesh_check_nodes(PBVH *pbvh)
{
#if 0
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (node->flag & PBVH_Leaf) {
      pbvh_bmesh_check_other_verts(node);
    }
  }

  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

    if (ni >= 0 && !v->e || !v->e->l) {
      _debugprint("wire vert had node reference\n");
      BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
    }

    if (ni < -1 || ni >= pbvh->totnode) {
      _debugprint("vert node ref was invalid");
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

    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
    BKE_pbvh_bmesh_check_valence(pbvh, (SculptVertRef){.i = (intptr_t)v});

    if (BM_vert_edge_count(v) != mv->valence) {
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
#endif
}

void pbvh_bmesh_pbvh_bmesh_check_nodes(PBVH *pbvh)
{
  pbvh_bmesh_check_nodes(pbvh);
}

/** \} */

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

  /* Create vert hash sets */
  if (!n->bm_unique_verts) {
    n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  }
  n->bm_other_verts = BLI_table_gset_new("bm_other_verts");

  BB_reset(&n->vb);
  BMFace *f;

  TGSET_ITER (f, n->bm_faces) {
    /* Update ownership of faces */
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    do {
      BMVert *v = l_iter->v;
      MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
      mv->flag |= DYNVERT_NEED_BOUNDARY;

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
    } while ((l_iter = l_iter->next) != l_first);

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      has_visible = true;
    }
  }
  TGSET_ITER_END

  BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
             n->vb.bmin[2] <= n->vb.bmax[2]);

  n->orig_vb = n->vb;

  /* Build GPU buffers for new node and update vertex normals */
  BKE_pbvh_node_mark_rebuild_draw(n);

  BKE_pbvh_node_fully_hidden_set(n, !has_visible);
  n->flag |= PBVH_UpdateNormals | PBVH_UpdateTopology | PBVH_UpdateCurvatureDir | PBVH_UpdateTris;

  if (add_orco) {
    BKE_pbvh_bmesh_check_tris(pbvh, n);
  }
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

  if (depth > 6 || BLI_table_gset_len(n->bm_faces) <= pbvh->leaf_limit) {
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

  c1->flag |= PBVH_Leaf;
  c2->flag |= PBVH_Leaf;
  c1->bm_faces = BLI_table_gset_new_ex("bm_faces", BLI_table_gset_len(n->bm_faces) / 2);
  c2->bm_faces = BLI_table_gset_new_ex("bm_faces", BLI_table_gset_len(n->bm_faces) / 2);

  c1->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  c2->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");

  c1->bm_other_verts = c2->bm_other_verts = NULL;

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
  TableGSet *empty = NULL, *other;
  if (BLI_table_gset_len(c1->bm_faces) == 0) {
    empty = c1->bm_faces;
    other = c2->bm_faces;
  }
  else if (BLI_table_gset_len(c2->bm_faces) == 0) {
    empty = c2->bm_faces;
    other = c1->bm_faces;
  }

  if (empty) {
    void *key;
    TGSET_ITER (key, other) {
      BLI_table_gset_insert(empty, key);
      BLI_table_gset_remove(other, key, NULL);
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

    BLI_table_gset_free(n->bm_unique_verts, NULL);
  }

  if (n->bm_faces) {
    /* Unclaim faces */
    TGSET_ITER (f, n->bm_faces) {
      BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
    }
    TGSET_ITER_END

    BLI_table_gset_free(n->bm_faces, NULL);
  }

  if (n->bm_other_verts) {
    BLI_table_gset_free(n->bm_other_verts, NULL);
  }

  if (n->layer_disp) {
    MEM_freeN(n->layer_disp);
  }

  if (n->tribuf || n->tri_buffers) {
    BKE_pbvh_bmesh_free_tris(pbvh, n);
  }

  n->bm_faces = NULL;
  n->bm_unique_verts = NULL;
  n->bm_other_verts = NULL;
  n->layer_disp = NULL;

  pbvh_free_all_draw_buffers(n);

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

  // pbvh_bmesh_check_nodes(pbvh);

  if (bm_faces_size <= pbvh->leaf_limit) {
    /* Node limit not exceeded */
    return false;
  }

  /* For each BMFace, store the AABB and AABB centroid */
  BBC *bbc_array = MEM_mallocN(sizeof(BBC) * bm_faces_size, "BBC");

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
  pbvh->bm->elem_index_dirty |= BM_FACE;

  pbvh_bmesh_node_split(pbvh, bbc_array, node_index, false, 0);

  MEM_freeN(bbc_array);

  // pbvh_bmesh_check_nodes(pbvh);

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
    printf("major pbvh corruption error");
    return;
  }

  BLI_table_gset_add(node->bm_faces, f);

  int updateflag = PBVH_UpdateTris | PBVH_UpdateBB | PBVH_UpdateDrawBuffers |
                   PBVH_UpdateCurvatureDir | PBVH_UpdateOtherVerts;
  updateflag |= PBVH_UpdateColor | PBVH_UpdateMask | PBVH_UpdateNormals | PBVH_UpdateOriginalBB;
  updateflag |= PBVH_UpdateVisibility | PBVH_UpdateRedraw;

  node->flag |= updateflag;

  // ensure verts are in pbvh
  BMLoop *l = f->l_first;
  do {
    const int ni2 = BM_ELEM_CD_GET_INT(l->v, pbvh->cd_vert_node_offset);
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, l->v);

    BB_expand(&node->vb, l->v->co);
    BB_expand(&node->orig_vb, mv->origco);

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

      BB_expand(&node2->vb, l->v->co);
      BB_expand(&node2->orig_vb, mv->origco);
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

static void pbvh_bmesh_regen_node_verts(PBVH *pbvh, PBVHNode *node)
{
  node->flag &= ~PBVH_RebuildNodeVerts;

  int usize = BLI_table_gset_len(node->bm_unique_verts);
  int osize = BLI_table_gset_len(node->bm_other_verts);

  TableGSet *old_unique_verts = node->bm_unique_verts;

  BLI_table_gset_free(node->bm_other_verts, NULL);

  BMVert *v;
  TGSET_ITER (v, old_unique_verts) {
    BM_ELEM_CD_SET_INT(v, pbvh->cd_vert_node_offset, -1);
  }
  TGSET_ITER_END;

  node->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
  node->bm_other_verts = BLI_table_gset_new("bm_other_verts");

  const int cd_vert_node = pbvh->cd_vert_node_offset;
  const int ni = (int)(node - pbvh->nodes);

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
          BLI_table_gset_remove(node->bm_other_verts, v, NULL);

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

  BLI_table_gset_free(old_unique_verts, NULL);
}

void BKE_pbvh_bmesh_mark_node_regen(PBVH *pbvh, PBVHNode *node)
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

  return NULL;
}

void BKE_pbvh_bmesh_regen_node_verts(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf) || !(node->flag & PBVH_RebuildNodeVerts)) {
      continue;
    }

    pbvh_bmesh_regen_node_verts(pbvh, node);
  }
}

void BKE_pbvh_bmesh_update_origvert(
    PBVH *pbvh, BMVert *v, float **r_co, float **r_no, float **r_color, bool log_undo)
{
  MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);

  if (log_undo) {
    BM_log_vert_before_modified(pbvh->bm_log, v, pbvh->cd_vert_mask_offset, r_color != NULL);
  }

  if (r_co || r_no) {

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (r_co) {
      *r_co = mv->origco;
    }

    if (r_no) {
      *r_no = mv->origno;
    }
  }

  if (r_color && pbvh->cd_vcol_offset >= 0) {
    MPropCol *ml1 = BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_vcol_offset);

    copy_v4_v4(mv->origcolor, ml1->color);

    if (r_color) {
      *r_color = mv->origcolor;
    }
  }
  else if (r_color) {
    *r_color = NULL;
  }
}

/************************* Called from pbvh.c *************************/

bool BKE_pbvh_bmesh_check_origdata(PBVH *pbvh, BMVert *v, int stroke_id)
{
  // keep this up to date with surface_smooth_v_safe in dyntopo.c

  MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);

  if (mv->stroke_id != stroke_id) {
    float *dummy;

    BKE_pbvh_bmesh_update_origvert(pbvh, v, &dummy, &dummy, &dummy, false);
    mv->stroke_id = stroke_id;
    return true;
  }

  return false;
}

bool pbvh_bmesh_node_raycast(PBVH *pbvh,
                             PBVHNode *node,
                             const float ray_start[3],
                             const float ray_normal[3],
                             struct IsectRayPrecalc *isect_precalc,
                             int *hit_count,
                             float *depth,
                             float *back_depth,
                             bool use_original,
                             SculptVertRef *r_active_vertex_index,
                             SculptFaceRef *r_active_face_index,
                             float *r_face_normal,
                             int stroke_id)
{
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};
  float nearest_vertex_dist = 1e17;

  BKE_pbvh_bmesh_check_tris(pbvh, node);

  PBVHTriBuf *tribuf = node->tribuf;
  const int cd_dyn_vert = pbvh->cd_dyn_vert;

  for (int i = 0; i < node->tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;
    BMVert *v1 = (BMVert *)tribuf->verts[tri->v[0]].i;
    BMVert *v2 = (BMVert *)tribuf->verts[tri->v[1]].i;
    BMVert *v3 = (BMVert *)tribuf->verts[tri->v[2]].i;

    BMFace *f = (BMFace *)tri->f.i;

    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    float *co1, *co2, *co3;

    if (use_original) {
      BKE_pbvh_bmesh_check_origdata(pbvh, v1, stroke_id);
      BKE_pbvh_bmesh_check_origdata(pbvh, v2, stroke_id);
      BKE_pbvh_bmesh_check_origdata(pbvh, v3, stroke_id);

      co1 = BKE_PBVH_DYNVERT(cd_dyn_vert, v1)->origco;
      co2 = BKE_PBVH_DYNVERT(cd_dyn_vert, v2)->origco;
      co3 = BKE_PBVH_DYNVERT(cd_dyn_vert, v3)->origco;
    }
    else {
      co1 = v1->co;
      co2 = v2->co;
      co3 = v3->co;
    }

    bool hit2 = ray_face_intersection_depth_tri(
        ray_start, isect_precalc, co1, co2, co3, depth, back_depth, hit_count);

    // bool hit2 = ray_face_intersection_tri(ray_start, isect_precalc, co1, co2, co3, depth);

    if (hit2) {
      // ensure sculpt active vertex is set r_active_vertex_index

      for (int j = 0; j < 3; j++) {
        BMVert *v = (BMVert *)tribuf->verts[tri->v[j]].i;
        float *co = BKE_PBVH_DYNVERT(cd_dyn_vert, v)->origco;

        float dist = len_squared_v3v3(co, ray_start);
        if (dist < nearest_vertex_dist) {
          nearest_vertex_dist = dist;
          copy_v3_v3(nearest_vertex_co, co);

          hit = true;
          if (r_active_vertex_index) {
            *r_active_vertex_index = tribuf->verts[tri->v[j]];
          }

          if (r_active_face_index) {
            *r_active_face_index = tri->f;
          }

          if (r_face_normal) {
            float no[3];

            if (use_original) {
              copy_v3_v3(no, BKE_PBVH_DYNVERT(cd_dyn_vert, v1)->origno);
              add_v3_v3(no, BKE_PBVH_DYNVERT(cd_dyn_vert, v2)->origno);
              add_v3_v3(no, BKE_PBVH_DYNVERT(cd_dyn_vert, v3)->origno);
              normalize_v3(no);
            }
            else {
              copy_v3_v3(no, tri->no);
            }

            copy_v3_v3(r_face_normal, no);
          }
        }
      }

      hit = true;
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

    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
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

bool pbvh_bmesh_node_nearest_to_ray(PBVH *pbvh,
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
  const int cd_dyn_vert = pbvh->cd_dyn_vert;

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;
    BMFace *f = (BMFace *)tri->f.i;

    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMVert *v1 = (BMVert *)tribuf->verts[tri->v[0]].i;
    BMVert *v2 = (BMVert *)tribuf->verts[tri->v[1]].i;
    BMVert *v3 = (BMVert *)tribuf->verts[tri->v[2]].i;

    float *co1, *co2, *co3;

    if (use_original) {
      BKE_pbvh_bmesh_check_origdata(pbvh, v1, stroke_id);
      BKE_pbvh_bmesh_check_origdata(pbvh, v2, stroke_id);
      BKE_pbvh_bmesh_check_origdata(pbvh, v3, stroke_id);

      co1 = BKE_PBVH_DYNVERT(cd_dyn_vert, v1)->origco;
      co2 = BKE_PBVH_DYNVERT(cd_dyn_vert, v2)->origco;
      co3 = BKE_PBVH_DYNVERT(cd_dyn_vert, v3)->origco;
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

typedef struct UpdateNormalsTaskData {
  PBVHNode *node;
  BMVert **border_verts;
  int tot_border_verts;
  int cd_dyn_vert;
} UpdateNormalsTaskData;

static void pbvh_update_normals_task_cb(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMVert *v;
  BMFace *f;
  UpdateNormalsTaskData *data = ((UpdateNormalsTaskData *)userdata) + n;
  PBVHNode *node = data->node;

  BMVert **bordervs = NULL;
  BLI_array_declare(bordervs);

  node->flag |= PBVH_UpdateCurvatureDir;

  TGSET_ITER (v, node->bm_unique_verts) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(data->cd_dyn_vert, v);

    if (mv->flag & DYNVERT_PBVH_BOUNDARY) {
      BLI_array_append(bordervs, v);
    }
    else {
      zero_v3(v->no);
    }
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
    BM_face_normal_update(f);
    BMLoop *l = f->l_first;
    do {
      MDynTopoVert *mv = BKE_PBVH_DYNVERT(data->cd_dyn_vert, l->v);

      if (!(mv->flag & DYNVERT_PBVH_BOUNDARY)) {
        add_v3_v3(l->v->no, f->no);
      }
    } while ((l = l->next) != f->l_first);
  }
  TGSET_ITER_END

  TGSET_ITER (v, node->bm_unique_verts) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(data->cd_dyn_vert, v);

    if (!(mv->flag & DYNVERT_PBVH_BOUNDARY)) {
      // BLI_array_append(bordervs, v);
      normalize_v3(v->no);
    }
#if 0
    // BM_vert_normal_update(v);
    // optimized loop
    BMEdge *e = v->e;

    zero_v3(v->no);

    if (!e) {
      continue;
    }

    do {
      BMLoop *l = e->l;

      if (!l) {
        e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
        continue;
      }

      // no need to loop over radial list here,
      // it's unneeded for manifold meshes and non-manifold
      // won't give correct normals anyway by definition

      v->no[0] += l->f->no[0];
      v->no[1] += l->f->no[1];
      v->no[2] += l->f->no[2];

      e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
    } while (e != v->e);

    normalize_v3(v->no);
#endif
  }
  TGSET_ITER_END

  data->border_verts = bordervs;
  data->tot_border_verts = BLI_array_len(bordervs);

  node->flag &= ~PBVH_UpdateNormals;
}

void pbvh_bmesh_normals_update(BMesh *bm, PBVHNode **nodes, int totnode)
{
  TaskParallelSettings settings;
  UpdateNormalsTaskData *datas = MEM_calloc_arrayN(totnode, sizeof(*datas), "bmesh normal update");

  for (int i = 0; i < totnode; i++) {
    datas[i].node = nodes[i];
  }

  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, datas, pbvh_update_normals_task_cb, &settings);

  BLI_bitmap *visit = BLI_BITMAP_NEW(bm->totvert, "visit");
  BM_mesh_elem_index_ensure(bm, BM_VERT);

  for (int i = 0; i < totnode; i++) {
    UpdateNormalsTaskData *data = datas + i;

#if 0
    printf("%.2f%% : %d %d\n",
           100.0f * (float)data->tot_border_verts / (float)data->node->bm_unique_verts->length,
           data->tot_border_verts,
           data->node->bm_unique_verts->length);
#endif

    for (int j = 0; j < data->tot_border_verts; j++) {
      BMVert *v = data->border_verts[j];

      if (BLI_BITMAP_TEST(visit, v->head.index)) {
        continue;
      }

      BLI_BITMAP_ENABLE(visit, v->head.index);

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

    MEM_SAFE_FREE(data->border_verts);
  }

  MEM_SAFE_FREE(visit);
  MEM_SAFE_FREE(datas);

#if 0  // in theory we shouldn't need to update normals in bm_other_verts.
  for (int i=0; i<totnode; i++) {
    PBVHNode *node = nodes[i];

    TGSET_ITER (v, node->bm_other_verts) {
      BM_vert_normal_update(v);
    }
    TGSET_ITER_END
  }
#endif
}

static void pbvh_bmesh_normals_update_old(PBVHNode **nodes, int totnode)
{
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];

    if (node->flag & PBVH_UpdateNormals) {
      BMVert *v;
      BMFace *f;

      TGSET_ITER (f, node->bm_faces) {
        BM_face_normal_update(f);
      }
      TGSET_ITER_END

      TGSET_ITER (v, node->bm_unique_verts) {
        BM_vert_normal_update(v);
      }
      TGSET_ITER_END

      /* This should be unneeded normally */
      TGSET_ITER (v, node->bm_other_verts) {
        BM_vert_normal_update(v);
      }
      TGSET_ITER_END

      node->flag &= ~PBVH_UpdateNormals;
    }
  }
}

struct FastNodeBuildInfo {
  int totface; /* number of faces */
  int start;   /* start of faces in array */
  int depth;
  struct FastNodeBuildInfo *child1;
  struct FastNodeBuildInfo *child2;
};

/**
 * Recursively split the node if it exceeds the leaf_limit.
 * This function is multi-thread-able since each invocation applies
 * to a sub part of the arrays.
 */
static void pbvh_bmesh_node_limit_ensure_fast(
    PBVH *pbvh, BMFace **nodeinfo, BBC *bbc_array, struct FastNodeBuildInfo *node, MemArena *arena)
{
  struct FastNodeBuildInfo *child1, *child2;

  if (node->totface <= pbvh->leaf_limit || node->depth >= pbvh->depth_limit) {
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

  node->child1 = child1 = BLI_memarena_alloc(arena, sizeof(struct FastNodeBuildInfo));
  node->child2 = child2 = BLI_memarena_alloc(arena, sizeof(struct FastNodeBuildInfo));

  child1->totface = num_child1;
  child1->start = node->start;
  child1->depth = node->depth + 1;

  child2->totface = num_child2;
  child2->start = node->start + num_child1;
  child2->depth = node->depth + 2;

  child1->child1 = child1->child2 = child2->child1 = child2->child2 = NULL;

  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, child1, arena);
  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, child2, arena);
}

static void pbvh_bmesh_create_nodes_fast_recursive(
    PBVH *pbvh, BMFace **nodeinfo, BBC *bbc_array, struct FastNodeBuildInfo *node, int node_index)
{
  PBVHNode *n = pbvh->nodes + node_index;

  /* two cases, node does not have children or does have children */
  if (node->child1) {
    int children_offset = pbvh->totnode;

    n->children_offset = children_offset;
    pbvh_grow_nodes(pbvh, pbvh->totnode + 2);
    pbvh_bmesh_create_nodes_fast_recursive(
        pbvh, nodeinfo, bbc_array, node->child1, children_offset);
    pbvh_bmesh_create_nodes_fast_recursive(
        pbvh, nodeinfo, bbc_array, node->child2, children_offset + 1);

    n = &pbvh->nodes[node_index];

    /* Update bounding box */
    BB_reset(&n->vb);
    BB_expand_with_bb(&n->vb, &pbvh->nodes[n->children_offset].vb);
    BB_expand_with_bb(&n->vb, &pbvh->nodes[n->children_offset + 1].vb);
    n->orig_vb = n->vb;
  }
  else {
    /* node does not have children so it's a leaf node, populate with faces and tag accordingly
     * this is an expensive part but it's not so easily thread-able due to vertex node indices */
    const int cd_vert_node_offset = pbvh->cd_vert_node_offset;
    const int cd_face_node_offset = pbvh->cd_face_node_offset;

    bool has_visible = false;

    n->flag = PBVH_Leaf | PBVH_UpdateTris;
    n->bm_faces = BLI_table_gset_new_ex("bm_faces", node->totface);

    /* Create vert hash sets */
    n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
    n->bm_other_verts = BLI_table_gset_new("bm_other_verts");

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
      } while ((l_iter = l_iter->next) != l_first);

      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        has_visible = true;
      }

      BB_expand_with_bb(&n->vb, (BB *)bbc);
    }

    BLI_assert(n->vb.bmin[0] <= n->vb.bmax[0] && n->vb.bmin[1] <= n->vb.bmax[1] &&
               n->vb.bmin[2] <= n->vb.bmax[2]);

    n->orig_vb = n->vb;

    /* Build GPU buffers for new node and update vertex normals */
    BKE_pbvh_node_mark_rebuild_draw(n);

    BKE_pbvh_node_fully_hidden_set(n, !has_visible);
    n->flag |= PBVH_UpdateNormals | PBVH_UpdateCurvatureDir;
  }
}

/***************************** Public API *****************************/

static float sculpt_corner_angle(float *base, float *co1, float *co2)
{
  float t1[3], t2[3];
  sub_v3_v3v3(t1, co1, base);
  sub_v3_v3v3(t2, co2, base);

  normalize_v3(t1);
  normalize_v3(t2);

  float th = dot_v3v3(t1, t2);

  return saacos(th);
}

typedef struct FSetTemp {
  BMVert *v;
  int fset;
  bool boundary;
} FSetTemp;

int BKE_pbvh_do_fset_symmetry(int fset, const int symflag, const float *co)
{
  fset = abs(fset);

  // don't affect base face set
  if (fset == 1) {
    return 1;
  }

  // surely we don't need more then 8 million face sets?
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

ATTR_NO_OPT void bke_pbvh_update_vert_boundary(int cd_dyn_vert,
                                               int cd_faceset_offset,
                                               int cd_vert_node_offset,
                                               int cd_face_node_offset,
                                               BMVert *v,
                                               int bound_symmetry)
{
  MDynTopoVert *mv = BKE_PBVH_DYNVERT(cd_dyn_vert, v);

  BMEdge *e = v->e;
  mv->flag &= ~(DYNVERT_BOUNDARY | DYNVERT_FSET_BOUNDARY | DYNVERT_NEED_BOUNDARY |
                DYNVERT_NEED_TRIANGULATE | DYNVERT_FSET_CORNER | DYNVERT_CORNER |
                DYNVERT_NEED_VALENCE | DYNVERT_SEAM_BOUNDARY | DYNVERT_SHARP_BOUNDARY |
                DYNVERT_SEAM_CORNER | DYNVERT_SHARP_CORNER | DYNVERT_PBVH_BOUNDARY);

  if (!e) {
    mv->flag |= DYNVERT_BOUNDARY;
    mv->valence = 0;
    return;
  }

  int val = 0;

  int ni = BM_ELEM_CD_GET_INT(v, cd_vert_node_offset);

  int sharpcount = 0;
  int seamcount = 0;
  int quadcount = 0;

#if 0
  struct FaceSetRef {
    int fset;
    BMVert *v2;
    BMEdge *e;
  } *fsets = NULL;
#endif
  int *fsets = NULL;
  BLI_array_staticdeclare(fsets, 16);

  do {
    BMVert *v2 = v == e->v1 ? e->v2 : e->v1;

    if (BM_ELEM_CD_GET_INT(v2, cd_vert_node_offset) != ni) {
      mv->flag |= DYNVERT_PBVH_BOUNDARY;
    }

    if (e->head.hflag & BM_ELEM_SEAM) {
      mv->flag |= DYNVERT_SEAM_BOUNDARY;
      seamcount++;

      if (seamcount > 2) {
        mv->flag |= DYNVERT_SEAM_CORNER;
      }
    }

    if (!(e->head.hflag & BM_ELEM_SMOOTH)) {
      mv->flag |= DYNVERT_SHARP_BOUNDARY;
      sharpcount++;

      if (sharpcount > 2) {
        mv->flag |= DYNVERT_SHARP_CORNER;
      }
    }

    if (e->l) {
      if (BM_ELEM_CD_GET_INT(e->l->f, cd_face_node_offset) != ni) {
        mv->flag |= DYNVERT_PBVH_BOUNDARY;
      }

      if (e->l != e->l->radial_next) {
        if (e->l->f->len > 3) {
          quadcount++;
        }

        if (e->l->radial_next->f->len > 3) {
          quadcount++;
        }

        if (BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_face_node_offset) != ni) {
          mv->flag |= DYNVERT_PBVH_BOUNDARY;
        }
      }

      int fset = BKE_pbvh_do_fset_symmetry(
          BM_ELEM_CD_GET_INT(e->l->f, cd_faceset_offset), bound_symmetry, v2->co);

      if (e->l->f->len > 3) {
        mv->flag |= DYNVERT_NEED_TRIANGULATE;
      }

      bool ok = true;
      for (int i = 0; i < BLI_array_len(fsets); i++) {
        if (fsets[i] == fset) {
          ok = false;
        }
      }

      if (ok) {
        BLI_array_append(fsets, fset);
      }

      // also check e->l->radial_next, in case we are not manifold
      // which can mess up the loop order
      if (e->l->radial_next != e->l) {
        // fset = abs(BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset));
        int fset2 = BKE_pbvh_do_fset_symmetry(
            BM_ELEM_CD_GET_INT(e->l->radial_next->f, cd_faceset_offset), bound_symmetry, v2->co);

        bool ok2 = true;
        for (int i = 0; i < BLI_array_len(fsets); i++) {
          if (fsets[i] == fset2) {
            ok2 = false;
          }
        }

        if (ok2) {
          BLI_array_append(fsets, fset2);
        }

        if (e->l->radial_next->f->len > 3) {
          mv->flag |= DYNVERT_NEED_TRIANGULATE;
        }
      }
    }

    if (!e->l || e->l->radial_next == e->l) {
      mv->flag |= DYNVERT_BOUNDARY;
    }

    val++;
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (BLI_array_len(fsets) > 1) {
    mv->flag |= DYNVERT_FSET_BOUNDARY;
  }

  if (BLI_array_len(fsets) > 2) {
    mv->flag |= DYNVERT_FSET_CORNER;
  }

  if (sharpcount == 1) {
    mv->flag |= DYNVERT_SHARP_CORNER;
  }

  if (seamcount == 1) {
    mv->flag |= DYNVERT_SEAM_CORNER;
  }

  mv->valence = val;
  if ((mv->flag & DYNVERT_BOUNDARY) && quadcount >= 3) {
    mv->flag |= DYNVERT_CORNER;
  }

  BLI_array_free(fsets);
}

bool BKE_pbvh_check_vert_boundary(PBVH *pbvh, BMVert *v)
{
  return pbvh_check_vert_boundary(pbvh, v);
}

void BKE_pbvh_update_vert_boundary(int cd_dyn_vert,
                                   int cd_faceset_offset,
                                   int cd_vert_node_offset,
                                   int cd_face_node_offset,
                                   BMVert *v,
                                   int bound_symmetry)
{
  bke_pbvh_update_vert_boundary(
      cd_dyn_vert, cd_faceset_offset, cd_vert_node_offset, cd_face_node_offset, v, bound_symmetry);
}

/*Used by symmetrize to update boundary flags*/
void BKE_pbvh_recalc_bmesh_boundary(PBVH *pbvh)
{
  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    bke_pbvh_update_vert_boundary(pbvh->cd_dyn_vert,
                                  pbvh->cd_faceset_offset,
                                  pbvh->cd_vert_node_offset,
                                  pbvh->cd_face_node_offset,
                                  v,
                                  pbvh->boundary_symmetry);
  }
}

void BKE_pbvh_update_all_boundary_bmesh(PBVH *pbvh)
{
  // update boundary flags in a hopefully faster manner then v->e iteration
  // XXX or not, it's slower

  BMIter iter;
  BMEdge *e;
  BMVert *v;

  const int cd_fset = CustomData_get_offset(&pbvh->bm->pdata, CD_SCULPT_FACE_SETS);

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
    mv->flag &= ~(DYNVERT_BOUNDARY | DYNVERT_FSET_BOUNDARY | DYNVERT_NEED_BOUNDARY |
                  DYNVERT_VERT_FSET_HIDDEN);

    if (cd_fset >= 0 && v->e && v->e->l) {
      mv->flag |= DYNVERT_VERT_FSET_HIDDEN;
    }
  }

  BM_ITER_MESH (e, &iter, pbvh->bm, BM_EDGES_OF_MESH) {
    if (!e->l || e->l == e->l->radial_next) {
      MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v1);
      MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v2);

      mv1->flag |= DYNVERT_BOUNDARY;
      mv2->flag |= DYNVERT_BOUNDARY;
    }

    if (!e->l) {
      continue;
    }

    if (cd_fset < 0) {
      MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v1);
      MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v2);

      BMLoop *l = e->l;

      do {
        if (l->f->len > 3) {
          mv1->flag |= DYNVERT_NEED_TRIANGULATE;
          mv2->flag |= DYNVERT_NEED_TRIANGULATE;
          break;
        }

        l = l->radial_next;
      } while (l != e->l);
      continue;
    }

    MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v1);
    MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, e->v2);

    BMLoop *l = e->l;
    int lastfset = BM_ELEM_CD_GET_INT(l->f, cd_fset);
    do {
      int fset = BM_ELEM_CD_GET_INT(l->f, cd_fset);

      if (l->f->len > 3) {
        mv1->flag |= DYNVERT_NEED_TRIANGULATE;
        mv2->flag |= DYNVERT_NEED_TRIANGULATE;
      }

      if (fset > 0) {
        MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, l->v);
        mv->flag &= ~DYNVERT_VERT_FSET_HIDDEN;
      }

      if (lastfset != fset) {
        mv1->flag |= DYNVERT_FSET_BOUNDARY;
        mv2->flag |= DYNVERT_FSET_BOUNDARY;
      }

      lastfset = fset;

      l = l->radial_next;
    } while (l != e->l);
  }
}
/* Build a PBVH from a BMesh */
void BKE_pbvh_build_bmesh(PBVH *pbvh,
                          BMesh *bm,
                          bool smooth_shading,
                          BMLog *log,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset,
                          const int cd_dyn_vert,
                          const int cd_face_areas,
                          bool fast_draw)
{
  pbvh->cd_face_area = cd_face_areas;
  pbvh->cd_vert_node_offset = cd_vert_node_offset;
  pbvh->cd_face_node_offset = cd_face_node_offset;
  pbvh->cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  pbvh->cd_dyn_vert = cd_dyn_vert;

  smooth_shading |= fast_draw;

  pbvh->bm = bm;

  BKE_pbvh_bmesh_detail_size_set(pbvh, 0.75f, 0.4f);

  pbvh->type = PBVH_BMESH;
  pbvh->bm_log = log;
  pbvh->cd_vcol_offset = CustomData_get_offset(&bm->vdata, CD_PROP_COLOR);
  pbvh->cd_faceset_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);

  pbvh->depth_limit = 18;

  /* TODO: choose leaf limit better */
  pbvh->leaf_limit = 1000;

  BMIter iter;
  BMVert *v;

  // BKE_pbvh_update_all_boundary_bmesh(pbvh);

  int cd_vcol_offset = CustomData_get_offset(&bm->vdata, CD_PROP_COLOR);

  if (smooth_shading) {
    pbvh->flags |= PBVH_DYNTOPO_SMOOTH_SHADING;
  }

  if (fast_draw) {
    pbvh->flags |= PBVH_FAST_DRAW;
  }

  /* bounding box array of all faces, no need to recalculate every time */
  BBC *bbc_array = MEM_mallocN(sizeof(BBC) * bm->totface, "BBC");
  BMFace **nodeinfo = MEM_mallocN(sizeof(*nodeinfo) * bm->totface, "nodeinfo");
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "fast PBVH node storage");

  BMFace *f;
  int i;
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    BBC *bbc = &bbc_array[i];
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

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
  rootnode.totface = bm->totface;

  /* start recursion, assign faces to nodes accordingly */
  pbvh_bmesh_node_limit_ensure_fast(pbvh, nodeinfo, bbc_array, &rootnode, arena);

  /* We now have all faces assigned to a node,
   * next we need to assign those to the gsets of the nodes. */

  /* Start with all faces in the root node */
  pbvh->nodes = MEM_callocN(sizeof(PBVHNode), "PBVHNode");
  pbvh->totnode = 1;

  /* take root node and visit and populate children recursively */
  pbvh_bmesh_create_nodes_fast_recursive(pbvh, nodeinfo, bbc_array, &rootnode, 0);

  BLI_memarena_free(arena);
  MEM_freeN(bbc_array);
  MEM_freeN(nodeinfo);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(cd_dyn_vert, v);

    mv->flag = DYNVERT_NEED_DISK_SORT;

    bke_pbvh_update_vert_boundary(pbvh->cd_dyn_vert,
                                  pbvh->cd_faceset_offset,
                                  pbvh->cd_vert_node_offset,
                                  pbvh->cd_face_node_offset,
                                  v,
                                  pbvh->boundary_symmetry);
    BKE_pbvh_bmesh_update_valence(pbvh->cd_dyn_vert, (SculptVertRef){(intptr_t)v});

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (cd_vcol_offset >= 0) {
      MPropCol *c1 = BM_ELEM_CD_GET_VOID_P(v, cd_vcol_offset);
      copy_v4_v4(mv->origcolor, c1->color);
    }
    else {
      zero_v4(mv->origcolor);
    }
  }
}

void BKE_pbvh_set_bm_log(PBVH *pbvh, struct BMLog *log)
{
  pbvh->bm_log = log;
}

/*
static double last_update_time[128] = {
    0,
};
*/

bool BKE_pbvh_bmesh_update_topology_nodes(PBVH *pbvh,
                                          bool (*searchcb)(PBVHNode *node, void *data),
                                          void (*undopush)(PBVHNode *node, void *data),
                                          void *searchdata,
                                          PBVHTopologyUpdateMode mode,
                                          const float center[3],
                                          const float view_normal[3],
                                          float radius,
                                          const bool use_frontface,
                                          const bool use_projected,
                                          int sym_axis,
                                          bool updatePBVH,
                                          DyntopoMaskCB mask_cb,
                                          void *mask_cb_data)
{
  bool modified = false;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf) || !searchcb(node, searchdata)) {
      continue;
    }

    if (node->flag & PBVH_Leaf) {
      node->flag |= PBVH_UpdateCurvatureDir;
      undopush(node, searchdata);

      BKE_pbvh_node_mark_topology_update(pbvh->nodes + i);
    }
  }

  // double start = PIL_check_seconds_timer();

  modified = modified || BKE_pbvh_bmesh_update_topology(pbvh,
                                                        mode,
                                                        center,
                                                        view_normal,
                                                        radius,
                                                        use_frontface,
                                                        use_projected,
                                                        sym_axis,
                                                        updatePBVH,
                                                        mask_cb,
                                                        mask_cb_data,
                                                        0);

  // double end = PIL_check_seconds_timer();

  // printf("dyntopo time: %f\n", end - start);

  return modified;
}

static void pbvh_free_tribuf(PBVHTriBuf *tribuf)
{
  MEM_SAFE_FREE(tribuf->verts);
  MEM_SAFE_FREE(tribuf->tris);
  MEM_SAFE_FREE(tribuf->loops);
  MEM_SAFE_FREE(tribuf->edges);

  BLI_smallhash_release(&tribuf->vertmap);

  tribuf->verts = NULL;
  tribuf->tris = NULL;
  tribuf->loops = NULL;
  tribuf->edges = NULL;

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

void BKE_pbvh_bmesh_free_tris(PBVH *pbvh, PBVHNode *node)
{
  if (node->tribuf) {
    pbvh_free_tribuf(node->tribuf);
    MEM_freeN(node->tribuf);
    node->tribuf = NULL;
  }

  if (node->tri_buffers) {
    for (int i = 0; i < node->tot_tri_buffers; i++) {
      pbvh_free_tribuf(node->tri_buffers + i);
    }

    MEM_SAFE_FREE(node->tri_buffers);

    node->tri_buffers = NULL;
    node->tot_tri_buffers = 0;
  }
}

/*
generate triangle buffers with split uv islands.
currently unused (and untested).
*/
static bool pbvh_bmesh_split_tris(PBVH *pbvh, PBVHNode *node)
{
  BMFace *f;

  BM_mesh_elem_index_ensure(pbvh->bm, BM_VERT | BM_FACE);

  // split by uvs
  int layeri = CustomData_get_layer_index(&pbvh->bm->ldata, CD_MLOOPUV);
  if (layeri < 0) {
    return false;
  }

  int totlayer = 0;

  while (layeri < pbvh->bm->ldata.totlayer && pbvh->bm->ldata.layers[layeri].type == CD_MLOOPUV) {
    totlayer++;
    layeri++;
  }

  const int cd_uv = pbvh->bm->ldata.layers[layeri].offset;
  const int cd_size = CustomData_sizeof(CD_MLOOPUV);

  SculptVertRef *verts = NULL;
  PBVHTri *tris = NULL;
  intptr_t *loops = NULL;

  BLI_array_declare(verts);
  BLI_array_declare(tris);
  BLI_array_declare(loops);

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      l->head.index = -1;
      l = l->next;
    } while (l != f->l_first);
  }
  TGSET_ITER_END

  int vi = 0;

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    do {
      if (l->head.index >= 0) {
        continue;
      }

      l->head.index = vi++;
      BLI_array_append(loops, (intptr_t)l);

      SculptVertRef sv = {(intptr_t)l->v};
      BLI_array_append(verts, sv);

      BMIter iter;
      BMLoop *l2;

      BM_ITER_ELEM (l2, &iter, l, BM_LOOPS_OF_VERT) {
        bool ok = true;

        for (int i = 0; i < totlayer; i++) {
          MLoopUV *uv1 = BM_ELEM_CD_GET_VOID_P(l, cd_uv + cd_size * i);
          MLoopUV *uv2 = BM_ELEM_CD_GET_VOID_P(l2, cd_uv + cd_size * i);

          if (len_v3v3(uv1->uv, uv2->uv) > 0.001) {
            ok = false;
            break;
          }
        }

        if (ok) {
          l2->head.index = l->head.index;
        }
      }
    } while (l != f->l_first);
  }
  TGSET_ITER_END

  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l1 = f->l_first, *l2 = f->l_first->next, *l3 = f->l_first->prev;

    PBVHTri tri;
    tri.f.i = (intptr_t)f;

    tri.v[0] = l1->head.index;
    tri.v[1] = l2->head.index;
    tri.v[2] = l3->head.index;

    copy_v3_v3(tri.no, f->no);
    BLI_array_append(tris, tri);
  }
  TGSET_ITER_END

  if (node->tribuf) {
    pbvh_free_tribuf(node->tribuf);
  }
  else {
    node->tribuf = MEM_callocN(sizeof(*node->tribuf), "node->tribuf");
  }

  node->tribuf->verts = verts;
  node->tribuf->loops = loops;
  node->tribuf->tris = tris;

  node->tribuf->tottri = BLI_array_len(tris);
  node->tribuf->totvert = BLI_array_len(verts);
  node->tribuf->totloop = BLI_array_len(loops);

  return true;
}

BLI_INLINE PBVHTri *pbvh_tribuf_add_tri(PBVHTriBuf *tribuf)
{
  tribuf->tottri++;

  if (tribuf->tottri >= tribuf->tris_size) {
    size_t newsize = (size_t)32 + (size_t)tribuf->tris_size + (size_t)(tribuf->tris_size >> 1);

    if (!tribuf->tris) {
      tribuf->tris = MEM_mallocN(sizeof(*tribuf->tris) * newsize, "tribuf tris");
    }
    else {
      tribuf->tris = MEM_reallocN_id(tribuf->tris, sizeof(*tribuf->tris) * newsize, "tribuf tris");
    }

    tribuf->tris_size = newsize;
  }

  return tribuf->tris + tribuf->tottri - 1;
}

BLI_INLINE void pbvh_tribuf_add_vert(PBVHTriBuf *tribuf, SculptVertRef vertex)
{
  tribuf->totvert++;

  if (tribuf->totvert >= tribuf->verts_size) {
    size_t newsize = (size_t)32 + (size_t)(tribuf->verts_size << 1);

    if (!tribuf->verts) {
      tribuf->verts = MEM_mallocN(sizeof(*tribuf->verts) * newsize, "tribuf verts");
    }
    else {
      tribuf->verts = MEM_reallocN_id(
          tribuf->verts, sizeof(*tribuf->verts) * newsize, "tribuf verts");
    }

    tribuf->verts_size = newsize;
  }

  tribuf->verts[tribuf->totvert - 1] = vertex;
}

BLI_INLINE void pbvh_tribuf_add_edge(PBVHTriBuf *tribuf, int v1, int v2)
{
  tribuf->totedge++;

  if (tribuf->totedge >= tribuf->edges_size) {
    size_t newsize = (size_t)32 + (size_t)(tribuf->edges_size << 1);

    if (!tribuf->edges) {
      tribuf->edges = MEM_mallocN(sizeof(*tribuf->edges) * 2ULL * newsize, "tribuf edges");
    }
    else {
      tribuf->edges = MEM_reallocN_id(
          tribuf->edges, sizeof(*tribuf->edges) * 2ULL * newsize, "tribuf edges");
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
    BLI_table_gset_free(node->bm_other_verts, NULL);
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

  tribuf->edges = NULL;
  tribuf->verts = NULL;
  tribuf->tris = NULL;
  tribuf->loops = NULL;

  BLI_smallhash_init_ex(&tribuf->vertmap, node->bm_unique_verts->length);
}
/* In order to perform operations on the original node coordinates
 * (currently just raycast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden. */
bool BKE_pbvh_bmesh_check_tris(PBVH *pbvh, PBVHNode *node)
{
  BMesh *bm = pbvh->bm;

  if (!(node->flag & PBVH_UpdateTris) && node->tribuf) {
    return false;
  }

  node->flag |= PBVH_UpdateOtherVerts;

  int mat_map[MAXMAT];

  for (int i = 0; i < MAXMAT; i++) {
    mat_map[i] = -1;
  }

  if (node->tribuf || node->tri_buffers) {
    BKE_pbvh_bmesh_free_tris(pbvh, node);
  }

  node->tribuf = MEM_callocN(sizeof(*node->tribuf), "node->tribuf");
  pbvh_init_tribuf(node, node->tribuf);

  BMLoop **loops = NULL;
  uint(*loops_idx)[3] = NULL;

  BLI_array_staticdeclare(loops, 128);
  BLI_array_staticdeclare(loops_idx, 128);

  PBVHTriBuf *tribufs = NULL;  // material-specific tribuffers
  BLI_array_declare(tribufs);

  node->flag &= ~PBVH_UpdateTris;

  const int edgeflag = BM_ELEM_TAG_ALT;

  BMFace *f;

  float min[3], max[3];

  INIT_MINMAX(min, max);

  TGSET_ITER (f, node->bm_faces) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

#ifdef SCULPT_DIAGONAL_EDGE_MARKS
    int ecount = 0;
#endif

    // clear edgeflag for building edge indices later
    BMLoop *l = f->l_first;
    do {
#ifdef SCULPT_DIAGONAL_EDGE_MARKS
      BMEdge *e2 = l->v->e;
      do {
        if (e2->head.hflag & BM_ELEM_DRAW) {
          ecount++;
        }
      } while ((e2 = BM_DISK_EDGE_NEXT(e2, l->v)) != l->v->e);
#endif
      l->e->head.hflag &= ~edgeflag;
    } while ((l = l->next) != f->l_first);

    const int mat_nr = f->mat_nr;

    if (mat_map[mat_nr] == -1) {
      PBVHTriBuf _tribuf = {0};

      mat_map[mat_nr] = BLI_array_len(tribufs);

      pbvh_init_tribuf(node, &_tribuf);
      _tribuf.mat_nr = mat_nr;
      BLI_array_append(tribufs, _tribuf);
    }

#ifdef DYNTOPO_DYNAMIC_TESS
    const int tottri = (f->len - 2);

    BLI_array_clear(loops);
    BLI_array_clear(loops_idx);
    BLI_array_grow_items(loops, f->len);
    BLI_array_grow_items(loops_idx, tottri);

    BM_face_calc_tessellation(f, true, loops, loops_idx);

    for (int i = 0; i < tottri; i++) {
      PBVHTri *tri = pbvh_tribuf_add_tri(node->tribuf);
      PBVHTriBuf *mat_tribuf = tribufs + mat_map[mat_nr];
      PBVHTri *mat_tri = pbvh_tribuf_add_tri(mat_tribuf);

      tri->eflag = mat_tri->eflag = 0;

      for (int j = 0; j < 3; j++) {
        // BMLoop *l0 = loops[loops_idx[i][(j + 2) % 3]];
        BMLoop *l = loops[loops_idx[i][j]];
        BMLoop *l2 = loops[loops_idx[i][(j + 1) % 3]];

        void **val = NULL;
        BMEdge *e = BM_edge_exists(l->v, l2->v);

#  ifdef SCULPT_DIAGONAL_EDGE_MARKS
        if (e && (e->head.hflag & BM_ELEM_DRAW)) {
#  else
        if (e) {
#  endif
          tri->eflag |= 1 << j;
          mat_tri->eflag |= 1 << j;
        }

        if (!BLI_smallhash_ensure_p(&node->tribuf->vertmap, (uintptr_t)l->v, &val)) {
          SculptVertRef sv = {(intptr_t)l->v};

          minmax_v3v3_v3(min, max, l->v->co);

          *val = POINTER_FROM_INT(node->tribuf->totvert);
          pbvh_tribuf_add_vert(node->tribuf, sv);
        }

        tri->v[j] = (intptr_t)val[0];
        tri->l[j] = (intptr_t)l;

        val = NULL;
        if (!BLI_smallhash_ensure_p(&mat_tribuf->vertmap, (uintptr_t)l->v, &val)) {
          SculptVertRef sv = {(intptr_t)l->v};

          minmax_v3v3_v3(min, max, l->v->co);

          *val = POINTER_FROM_INT(mat_tribuf->totvert);
          pbvh_tribuf_add_vert(mat_tribuf, sv);
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
      void **val = NULL;

      if (!BLI_ghash_ensure_p(vmap, l->v, &val)) {
        SculptVertRef sv = {(intptr_t)l->v};

        minmax_v3v3_v3(min, max, l->v->co);

        *val = (void *)node->tribuf->totvert;
        pbvh_tribuf_add_vert(node->tribuf, sv);
      }

      tri->v[j] = (intptr_t)val[0];
      tri->l[j] = (intptr_t)l;

      val = NULL;
      if (!BLI_ghash_ensure_p(mat_vmaps[mat_nr], l->v, &val)) {
        SculptVertRef sv = {(intptr_t)l->v};

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
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    int mat_nr = f->mat_nr;
    PBVHTriBuf *mat_tribuf = tribufs + mat_map[mat_nr];

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

  BLI_array_free(loops);
  BLI_array_free(loops_idx);

  bm->elem_index_dirty |= BM_VERT;

  node->tri_buffers = tribufs;
  node->tot_tri_buffers = BLI_array_len(tribufs);

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

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
    mv->flag |= DYNVERT_NEED_DISK_SORT;
  }
}

void BKE_pbvh_bmesh_update_all_valence(PBVH *pbvh)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    BKE_pbvh_bmesh_update_valence(pbvh->cd_dyn_vert, (SculptVertRef){(intptr_t)v});
  }
}

void BKE_pbvh_bmesh_on_mesh_change(PBVH *pbvh)
{
  BMIter iter;
  BMVert *v;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (node->flag & PBVH_Leaf) {
      node->flag |= PBVH_UpdateTriAreas;
    }
  }

  const int cd_dyn_vert = pbvh->cd_dyn_vert;

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(cd_dyn_vert, v);

    mv->flag |= DYNVERT_NEED_BOUNDARY | DYNVERT_NEED_DISK_SORT | DYNVERT_NEED_TRIANGULATE;
    BKE_pbvh_bmesh_update_valence(pbvh->cd_dyn_vert, (SculptVertRef){.i = (intptr_t)v});
  }
}

bool BKE_pbvh_bmesh_mark_update_valence(PBVH *pbvh, SculptVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_dyn_vert);

  bool ret = mv->flag & DYNVERT_NEED_VALENCE;

  mv->flag |= DYNVERT_NEED_VALENCE;

  return ret;
}

bool BKE_pbvh_bmesh_check_valence(PBVH *pbvh, SculptVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_dyn_vert);

  if (mv->flag & DYNVERT_NEED_VALENCE) {
    BKE_pbvh_bmesh_update_valence(pbvh->cd_dyn_vert, vertex);
    return true;
  }

  return false;
}

void BKE_pbvh_bmesh_update_valence(int cd_dyn_vert, SculptVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  BMEdge *e;

  MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, cd_dyn_vert);

  mv->flag &= ~DYNVERT_NEED_VALENCE;

  if (!v->e) {
    mv->valence = 0;
    return;
  }

  mv->valence = 0;

  e = v->e;

  if (!e) {
    return;
  }

  do {
    mv->valence++;

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
      node->flag |= PBVH_Delete;  // mark for deletion
    }

    return;
  }

  if (node != parent) {
    node->flag |= PBVH_Delete;  // mark for deletion
  }

  BMVert *v;

  TGSET_ITER (v, node->bm_unique_verts) {
    BLI_table_gset_add(parent->bm_unique_verts, v);

    MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
    mv->flag |= DYNVERT_NEED_BOUNDARY;  // need to update DYNVERT_PBVH_BOUNDARY flags

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

static void BKE_pbvh_bmesh_correct_tree(PBVH *pbvh, PBVHNode *node, PBVHNode *parent)
{
  const int size_lower = pbvh->leaf_limit - (pbvh->leaf_limit >> 1);

  if (node->flag & PBVH_Leaf) {
    // pbvh_bmesh_node_limit_ensure(pbvh, (int)(node - pbvh->nodes));
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
                  PBVH_UpdateVisibility | PBVH_UpdateColor | PBVH_UpdateTopology |
                  PBVH_UpdateNormals | PBVH_UpdateTris;

    TableGSet *other = BLI_table_gset_new(__func__);
    BMVert *v;

    node->children_offset = 0;

    pbvh_free_all_draw_buffers(node);

    // rebuild bm_other_verts
    BMFace *f;
    TGSET_ITER (f, node->bm_faces) {
      BMLoop *l = f->l_first;

      BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);

      do {
        if (!BLI_table_gset_haskey(node->bm_unique_verts, l->v)) {
          BLI_table_gset_add(other, l->v);
        }
        l = l->next;
      } while (l != f->l_first);
    }
    TGSET_ITER_END

    BLI_table_gset_free(node->bm_other_verts, NULL);
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

// deletes PBVH_Delete marked nodes
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
          n3->tribuf = NULL;
        }
        else if ((n1->flag & PBVH_Delete) && (n2->flag & PBVH_Delete)) {
          n->children_offset = 0;
          n->flag |= PBVH_Leaf | PBVH_UpdateTris;

          if (!n->bm_unique_verts) {
            // should not happen
            n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
            n->bm_other_verts = BLI_table_gset_new("bm_other_verts");
            n->bm_faces = BLI_table_gset_new("bm_faces");
            n->tribuf = NULL;
          }
        }
      }

      totnode++;
    }
  }

  int *map = MEM_callocN(sizeof(int) * bvh->totnode, "bmesh map temp");

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
        n->layer_disp = NULL;
      }

      pbvh_free_all_draw_buffers(n);

      if (n->vert_indices) {
        MEM_freeN((void *)n->vert_indices);
        n->vert_indices = NULL;
      }
      if (n->face_vert_indices) {
        MEM_freeN((void *)n->face_vert_indices);
        n->face_vert_indices = NULL;
      }

      if (n->tribuf || n->tri_buffers) {
        BKE_pbvh_bmesh_free_tris(bvh, n);
      }

      if (n->bm_unique_verts) {
        BLI_table_gset_free(n->bm_unique_verts, NULL);
        n->bm_unique_verts = NULL;
      }

      if (n->bm_other_verts) {
        BLI_table_gset_free(n->bm_other_verts, NULL);
        n->bm_other_verts = NULL;
      }

      if (n->bm_faces) {
        BLI_table_gset_free(n->bm_faces, NULL);
        n->bm_faces = NULL;
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

  BMVert **scratch = NULL;
  BLI_array_declare(scratch);

  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    BLI_array_clear(scratch);
    BMVert *v;

    TGSET_ITER (v, n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        BLI_array_append(scratch, v);
      }
      // BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    int slen = BLI_array_len(scratch);
    for (int j = 0; j < slen; j++) {
      BMVert *v = scratch[j];

      BLI_table_gset_remove(n->bm_other_verts, v, NULL);
      BLI_table_gset_add(n->bm_unique_verts, v);
      BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
  }

  BLI_array_free(scratch);
  MEM_freeN(map);
}

static void recursive_delete_nodes(PBVH *pbvh, int ni)
{
  PBVHNode *node = pbvh->nodes + ni;

  node->flag |= PBVH_Delete;

  if (!(node->flag & PBVH_Leaf) && node->children_offset) {
    if (node->children_offset < pbvh->totnode) {
      recursive_delete_nodes(pbvh, node->children_offset);
    }

    if (node->children_offset + 1 < pbvh->totnode) {
      recursive_delete_nodes(pbvh, node->children_offset + 1);
    }
  }
}

// static float bbox_overlap()
/* works by detect overlay of leaf nodes, destroying them
  and then re-inserting them*/
static void pbvh_bmesh_balance_tree(PBVH *pbvh)
{
  PBVHNode **stack = NULL;
  float *overlaps = MEM_calloc_arrayN(pbvh->totnode, sizeof(float), "overlaps");
  PBVHNode **parentmap = MEM_calloc_arrayN(pbvh->totnode, sizeof(*parentmap), "parentmap");
  int *depthmap = MEM_calloc_arrayN(pbvh->totnode, sizeof(*depthmap), "depthmap");
  BLI_array_declare(stack);

  BMFace **faces = NULL;
  BLI_array_declare(faces);

  PBVHNode **substack = NULL;
  BLI_array_declare(substack);

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

#if 0
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;
    PBVHNode *parent = parentmap[i];
    int depth = 0;

    while (parent) {
      parent = parentmap[parent - pbvh->nodes];
      depth++;
    }

    depthmap[i] = depth;
  }
#endif

  const int cd_vert_node = pbvh->cd_vert_node_offset;
  const int cd_face_node = pbvh->cd_face_node_offset;

  bool modified = false;

  BLI_array_append(stack, pbvh->nodes);
  while (BLI_array_len(stack) > 0) {
    PBVHNode *node = BLI_array_pop(stack);
    BB clip;

    if (!(node->flag & PBVH_Leaf) && node->children_offset > 0) {
      PBVHNode *child1 = pbvh->nodes + node->children_offset;
      PBVHNode *child2 = pbvh->nodes + node->children_offset + 1;

      float volume = BB_volume(&child1->vb) + BB_volume(&child2->vb);

      BB_intersect(&clip, &child1->vb, &child2->vb);
      float overlap = BB_volume(&clip);

      // for (int i = 0; i < depthmap[node - pbvh->nodes]; i++) {
      // printf("-");
      //}

      // printf("volume: %.4f overlap: %.4f ratio: %.3f\n", volume, overlap, overlap / volume);

      if (overlap > volume * 0.1) {
        modified = true;
        // printf("  DELETE!\n");

        BLI_array_clear(substack);

        BLI_array_append(substack, child1);
        BLI_array_append(substack, child2);

        while (BLI_array_len(substack) > 0) {
          PBVHNode *node2 = BLI_array_pop(substack);

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
              BLI_array_append(faces, f);
            }
            TGSET_ITER_END;

            TGSET_ITER (v, node2->bm_unique_verts) {
              MDynTopoVert *mv = BKE_PBVH_DYNVERT(pbvh->cd_dyn_vert, v);
              mv->flag |= DYNVERT_NEED_BOUNDARY;

              BM_ELEM_CD_SET_INT(v, cd_vert_node, DYNTOPO_NODE_NONE);
            }
            TGSET_ITER_END;
          }
          else if (node2->children_offset > 0 && node2->children_offset < pbvh->totnode) {
            BLI_array_append(substack, pbvh->nodes + node2->children_offset);

            if (node2->children_offset + 1 < pbvh->totnode) {
              BLI_array_append(substack, pbvh->nodes + node2->children_offset + 1);
            }
          }
        }
      }

      if (node->children_offset < pbvh->totnode) {
        BLI_array_append(stack, child1);
      }

      if (node->children_offset + 1 < pbvh->totnode) {
        BLI_array_append(stack, child2);
      }
    }
  }

  if (modified) {
    pbvh_bmesh_compact_tree(pbvh);

    printf("joined nodes; %d faces\n", BLI_array_len(faces));

    for (int i = 0; i < BLI_array_len(faces); i++) {
      if (BM_ELEM_CD_GET_INT(faces[i], cd_face_node) != DYNTOPO_NODE_NONE) {
        // printf("duplicate faces in pbvh_bmesh_balance_tree!\n");
        continue;
      }

      bke_pbvh_insert_face(pbvh, faces[i]);
    }
  }

  BLI_array_free(faces);

  MEM_SAFE_FREE(parentmap);
  MEM_SAFE_FREE(overlaps);
  BLI_array_free(stack);
  BLI_array_free(substack);
}

static void pbvh_bmesh_join_nodes(PBVH *bvh)
{
  if (bvh->totnode < 2) {
    return;
  }

  pbvh_count_subtree_verts(bvh, bvh->nodes);
  BKE_pbvh_bmesh_correct_tree(bvh, bvh->nodes, NULL);

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
          n3->tribuf = NULL;
        }
        else if ((n1->flag & PBVH_Delete) && (n2->flag & PBVH_Delete)) {
          n->children_offset = 0;
          n->flag |= PBVH_Leaf | PBVH_UpdateTris;

          if (!n->bm_unique_verts) {
            // should not happen
            n->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
            n->bm_other_verts = BLI_table_gset_new("bm_other_verts");
            n->bm_faces = BLI_table_gset_new("bm_faces");
            n->tribuf = NULL;
          }
        }
      }

      totnode++;
    }
  }

  int *map = MEM_callocN(sizeof(int) * bvh->totnode, "bmesh map temp");

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
        n->layer_disp = NULL;
      }

      pbvh_free_all_draw_buffers(n);

      if (n->vert_indices) {
        MEM_freeN((void *)n->vert_indices);
        n->vert_indices = NULL;
      }
      if (n->face_vert_indices) {
        MEM_freeN((void *)n->face_vert_indices);
        n->face_vert_indices = NULL;
      }

      if (n->tribuf || n->tri_buffers) {
        BKE_pbvh_bmesh_free_tris(bvh, n);
      }

      if (n->bm_unique_verts) {
        BLI_table_gset_free(n->bm_unique_verts, NULL);
        n->bm_unique_verts = NULL;
      }

      if (n->bm_other_verts) {
        BLI_table_gset_free(n->bm_other_verts, NULL);
        n->bm_other_verts = NULL;
      }

      if (n->bm_faces) {
        BLI_table_gset_free(n->bm_faces, NULL);
        n->bm_faces = NULL;
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

  BMVert **scratch = NULL;
  BLI_array_declare(scratch);

  for (int i = 0; i < bvh->totnode; i++) {
    PBVHNode *n = bvh->nodes + i;

    if (!(n->flag & PBVH_Leaf)) {
      continue;
    }

    BLI_array_clear(scratch);
    BMVert *v;

    TGSET_ITER (v, n->bm_other_verts) {
      int ni = BM_ELEM_CD_GET_INT(v, bvh->cd_vert_node_offset);
      if (ni == DYNTOPO_NODE_NONE) {
        BLI_array_append(scratch, v);
      }
      // BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
    TGSET_ITER_END

    int slen = BLI_array_len(scratch);
    for (int j = 0; j < slen; j++) {
      BMVert *v = scratch[j];

      BLI_table_gset_remove(n->bm_other_verts, v, NULL);
      BLI_table_gset_add(n->bm_unique_verts, v);
      BM_ELEM_CD_SET_INT(v, bvh->cd_vert_node_offset, i);
    }
  }

  BLI_array_free(scratch);
  MEM_freeN(map);
}

void BKE_pbvh_bmesh_after_stroke(PBVH *pbvh, bool force_balance)
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
  }

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

void BKE_pbvh_bmesh_detail_size_set(PBVH *pbvh, float detail_size, float detail_range)
{
  pbvh->bm_max_edge_len = detail_size;
  pbvh->bm_min_edge_len = pbvh->bm_max_edge_len * detail_range;
  pbvh->bm_detail_range = detail_range;
}

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
                             const int cd_dyn_vert,
                             const int cd_face_areas)
{
  pbvh->cd_face_node_offset = cd_face_node_offset;
  pbvh->cd_vert_node_offset = cd_vert_node_offset;
  pbvh->cd_face_area = cd_face_areas;
  pbvh->cd_vert_mask_offset = CustomData_get_offset(&pbvh->bm->vdata, CD_PAINT_MASK);
  pbvh->cd_vcol_offset = CustomData_get_offset(&pbvh->bm->vdata, CD_PROP_COLOR);
  pbvh->cd_dyn_vert = cd_dyn_vert;
  pbvh->cd_faceset_offset = CustomData_get_offset(&pbvh->bm->pdata, CD_SCULPT_FACE_SETS);
}

static void scan_edge_split(BMesh *bm, BMEdge **edges, int totedge)
{
  BMFace **faces = NULL;
  BMEdge **newedges = NULL;
  BMVert **newverts = NULL;
  BMVert **fmap = NULL;  // newverts that maps to faces
  int *emap = NULL;

  BLI_array_declare(faces);
  BLI_array_declare(newedges);
  BLI_array_declare(newverts);
  BLI_array_declare(fmap);
  BLI_array_declare(emap);

  // remove e from radial list of e->v2
  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];

    BMDiskLink *prev;
    BMDiskLink *next;

    if (e->v2_disk_link.prev->v1 == e->v2) {
      prev = &e->v2_disk_link.prev->v1_disk_link;
    }
    else {
      prev = &e->v2_disk_link.prev->v2_disk_link;
    }

    if (e->v2_disk_link.next->v1 == e->v2) {
      next = &e->v2_disk_link.next->v1_disk_link;
    }
    else {
      next = &e->v2_disk_link.next->v2_disk_link;
    }

    prev->next = e->v2_disk_link.next;
    next->prev = e->v2_disk_link.prev;
  }

  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];

    BMVert *v2 = BLI_mempool_alloc(bm->vpool);
    memset(v2, 0, sizeof(*v2));
    v2->head.data = BLI_mempool_alloc(bm->vdata.pool);

    BLI_array_append(newverts, v2);

    BMEdge *e2 = BLI_mempool_alloc(bm->epool);
    BLI_array_append(newedges, e2);

    memset(e2, 0, sizeof(*e2));
    if (bm->edata.pool) {
      e2->head.data = BLI_mempool_alloc(bm->edata.pool);
    }

    BMLoop *l = e->l;

    if (!l) {
      continue;
    }

    do {
      BLI_array_append(faces, l->f);
      BMFace *f2 = BLI_mempool_alloc(bm->fpool);

      BLI_array_append(faces, l->f);
      BLI_array_append(fmap, v2);
      BLI_array_append(emap, i);

      BLI_array_append(faces, f2);
      BLI_array_append(fmap, v2);
      BLI_array_append(emap, i);

      memset(f2, 0, sizeof(*f2));
      f2->head.data = BLI_mempool_alloc(bm->ldata.pool);

      BMLoop *prev = NULL;
      BMLoop *l2 = NULL;

      for (int j = 0; j < 3; j++) {
        l2 = BLI_mempool_alloc(bm->lpool);
        memset(l2, 0, sizeof(*l2));
        l2->head.data = BLI_mempool_alloc(bm->ldata.pool);

        l2->prev = prev;

        if (prev) {
          prev->next = l2;
        }
        else {
          f2->l_first = l2;
        }
      }

      f2->l_first->prev = l2;
      l2->next = f2->l_first;

      BLI_array_append(faces, f2);
      l = l->radial_next;
    } while (l != e->l);
  }

  for (int i = 0; i < BLI_array_len(newedges); i++) {
    BMEdge *e1 = edges[i];
    BMEdge *e2 = newedges[i];
    BMVert *v = newverts[i];

    add_v3_v3v3(v->co, e1->v1->co, e1->v2->co);
    mul_v3_fl(v->co, 0.5f);

    e2->v1 = v;
    e2->v2 = e1->v2;
    e1->v2 = v;

    v->e = e1;

    e1->v2_disk_link.next = e1->v2_disk_link.prev = e2;
    e2->v1_disk_link.next = e2->v1_disk_link.prev = e1;
  }

  for (int i = 0; i < BLI_array_len(faces); i += 2) {
    BMFace *f1 = faces[i], *f2 = faces[i + 1];
    BMEdge *e1 = edges[emap[i]];
    BMEdge *e2 = newedges[emap[i]];
    BMVert *nv = fmap[i];

    // make sure first loop points to e1->v1
    BMLoop *l = f1->l_first;
    do {
      if (l->v == e1->v1) {
        break;
      }
      l = l->next;
    } while (l != f1->l_first);

    f1->l_first = l;

    BMLoop *l2 = f2->l_first;

    l2->f = l2->next->f = l2->prev->f = f2;
    l2->v = nv;
    l2->next->v = l->next->v;
    l2->prev->v = l->prev->v;
    l2->e = e2;
    l2->next->e = l->next->e;
    l2->prev->e = l->prev->e;

    l->next->v = nv;
    l->next->e = e2;
  }

  BLI_array_free(newedges);
  BLI_array_free(newverts);
  BLI_array_free(faces);
  BLI_array_free(fmap);
}

#define MAX_RE_CHILD 3
typedef struct ReVertNode {
  int totvert, totchild;
  struct ReVertNode *parent;
  struct ReVertNode *children[MAX_RE_CHILD];
  BMVert *verts[];
} ReVertNode;

BMesh *BKE_pbvh_reorder_bmesh(PBVH *pbvh)
{
  /*try to compute size of verts per node*/
  int vsize = sizeof(BMVert);
  vsize += pbvh->bm->vdata.totsize;

  // perhaps aim for l2 cache?
  const int limit = 1024;
  int leaf_limit = MAX2(limit / vsize, 4);

  BLI_mempool *pool = BLI_mempool_create(sizeof(ReVertNode) + sizeof(void *) * vsize, 0, 8192, 0);
  ReVertNode **vnodemap = MEM_calloc_arrayN(pbvh->bm->totvert, sizeof(void *), "vnodemap");

  printf("leaf_limit: %d\n", leaf_limit);

  BMIter iter;
  BMVert *v;
  const char flag = BM_ELEM_TAG_ALT;
  int i = 0;

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    v->head.hflag &= ~flag;
    v->head.index = i++;
  }

  BMVert **stack = NULL;
  BLI_array_declare(stack);

  BM_ITER_MESH (v, &iter, pbvh->bm, BM_VERTS_OF_MESH) {
    if (v->head.hflag & flag) {
      continue;
    }

    ReVertNode *node = BLI_mempool_calloc(pool);

    BLI_array_clear(stack);
    BLI_array_append(stack, v);

    v->head.hflag |= flag;

    vnodemap[v->head.index] = node;
    node->verts[node->totvert++] = v;

    while (BLI_array_len(stack) > 0) {
      BMVert *v2 = BLI_array_pop(stack);
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

          BLI_array_append(stack, v3);
        }

        e = e->v1 == v2 ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v2->e);
    }
  }

  const int steps = 4;
  ReVertNode **roots = NULL;
  BLI_array_declare(roots);

  for (int step = 0; step < steps; step++) {
    const bool last_step = step == steps - 1;

    BM_ITER_MESH_INDEX (v, &iter, pbvh->bm, BM_VERTS_OF_MESH, i) {
      BMEdge *e = v->e;

      if (!e) {
        continue;
      }

      ReVertNode *node = vnodemap[v->head.index];
      if (node->parent) {
        continue;
      }

      ReVertNode *parent = BLI_mempool_calloc(pool);
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
        BLI_array_append(roots, parent);
      }

      for (int j = 0; j < parent->totchild; j++) {
        parent->children[j]->parent = parent;
      }
    }

    BM_ITER_MESH_INDEX (v, &iter, pbvh->bm, BM_VERTS_OF_MESH, i) {
      while (vnodemap[i]->parent) {
        vnodemap[i] = vnodemap[i]->parent;
      }
    }
  }

  BLI_mempool_iter loopiter;
  BLI_mempool_iternew(pbvh->bm->lpool, &loopiter);
  BMLoop *l = BLI_mempool_iterstep(&loopiter);
  BMEdge *e;
  BMFace *f;

  for (i = 0; l; l = BLI_mempool_iterstep(&loopiter), i++) {
    l->head.hflag &= ~flag;
  }
  BM_ITER_MESH (e, &iter, pbvh->bm, BM_EDGES_OF_MESH) {
    e->head.hflag &= ~flag;
  }

  BM_ITER_MESH (f, &iter, pbvh->bm, BM_FACES_OF_MESH) {
    f->head.hflag &= ~flag;
  }

  int totroot = BLI_array_len(roots);
  ReVertNode **nstack = NULL;
  BLI_array_declare(nstack);
  int vorder = 0, eorder = 0, lorder = 0, forder = 0;

  for (i = 0; i < totroot; i++) {
    BLI_array_clear(nstack);

    ReVertNode *node = roots[i];
    BLI_array_append(nstack, node);

    while (BLI_array_len(nstack) > 0) {
      ReVertNode *node2 = BLI_array_pop(nstack);

      if (node2->totchild == 0) {
        for (int j = 0; j < node2->totvert; j++) {
          v = node2->verts[j];

#if 0
          const int cd_vcol = CustomData_get_offset(&pbvh->bm->vdata, CD_PROP_COLOR);

          if (cd_vcol >= 0) {
            MPropCol *col = BM_ELEM_CD_GET_VOID_P(node2->verts[j], cd_vcol);

            float r = 0.0f, g = 0.0f, b = 0.0f;

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
          BLI_array_append(nstack, node2->children[j]);
        }
      }
    }
  }

  uint *vidx, *eidx, *lidx, *fidx;

  vidx = MEM_malloc_arrayN(pbvh->bm->totvert, sizeof(*vidx), "vorder");
  eidx = MEM_malloc_arrayN(pbvh->bm->totedge, sizeof(*eidx), "eorder");
  lidx = MEM_malloc_arrayN(pbvh->bm->totloop, sizeof(*lidx), "lorder");
  fidx = MEM_malloc_arrayN(pbvh->bm->totface, sizeof(*fidx), "forder");

  printf("v %d %d\n", vorder, pbvh->bm->totvert);
  printf("e %d %d\n", eorder, pbvh->bm->totedge);
  printf("l %d %d\n", lorder, pbvh->bm->totloop);
  printf("f %d %d\n", forder, pbvh->bm->totface);

  BM_ITER_MESH_INDEX (v, &iter, pbvh->bm, BM_VERTS_OF_MESH, i) {
    vidx[i] = (uint)v->head.index;
  }

  BM_ITER_MESH_INDEX (e, &iter, pbvh->bm, BM_EDGES_OF_MESH, i) {
    eidx[i] = (uint)e->head.index;
  }
  BM_ITER_MESH_INDEX (f, &iter, pbvh->bm, BM_FACES_OF_MESH, i) {
    fidx[i] = (uint)f->head.index;
  }

  BLI_mempool_iternew(pbvh->bm->lpool, &loopiter);
  l = BLI_mempool_iterstep(&loopiter);

  for (i = 0; l; l = BLI_mempool_iterstep(&loopiter), i++) {
    // handle orphaned loops
    if (!(l->head.hflag & flag)) {
      printf("warning in %s: orphaned loop!\n", __func__);
      l->head.index = lorder++;
    }

    lidx[i] = (uint)l->head.index;
  }

  printf("roots: %d\n", BLI_array_len(roots));

  BM_mesh_remap(pbvh->bm, vidx, eidx, fidx, lidx);

  MEM_SAFE_FREE(vidx);
  MEM_SAFE_FREE(eidx);
  MEM_SAFE_FREE(lidx);
  MEM_SAFE_FREE(fidx);

  MEM_SAFE_FREE(nstack);
  MEM_SAFE_FREE(roots);
  BLI_mempool_destroy(pool);
  MEM_SAFE_FREE(stack);
  MEM_SAFE_FREE(vnodemap);

  return pbvh->bm;
}

BMesh *BKE_pbvh_reorder_bmesh2(PBVH *pbvh)
{
  if (BKE_pbvh_type(pbvh) != PBVH_BMESH || pbvh->totnode == 0) {
    return pbvh->bm;
  }

  // try to group memory allocations by node
  struct {
    BMEdge **edges;
    int totedge;
    BMVert **verts;
    int totvert;
    BMFace **faces;
    int totface;
  } *nodedata = MEM_callocN(sizeof(*nodedata) * pbvh->totnode, "nodedata");

  BMIter iter;
  int types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};

#define VISIT_TAG BM_ELEM_TAG

  BM_mesh_elem_index_ensure(pbvh->bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_table_ensure(pbvh->bm, BM_VERT | BM_EDGE | BM_FACE);

  for (int i = 0; i < 3; i++) {
    BMHeader *elem;

    BM_ITER_MESH (elem, &iter, pbvh->bm, types[i]) {
      elem->hflag &= ~VISIT_TAG;
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BMVert **verts = nodedata[i].verts;
    BMEdge **edges = nodedata[i].edges;
    BMFace **faces = nodedata[i].faces;

    BLI_array_declare(verts);
    BLI_array_declare(edges);
    BLI_array_declare(faces);

    BMVert *v;
    BMFace *f;

    TGSET_ITER (v, node->bm_unique_verts) {
      if (v->head.hflag & VISIT_TAG) {
        continue;
      }

      v->head.hflag |= VISIT_TAG;
      BLI_array_append(verts, v);

      BMEdge *e = v->e;
      do {
        if (!(e->head.hflag & VISIT_TAG)) {
          e->head.hflag |= VISIT_TAG;
          BLI_array_append(edges, e);
        }
        e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v->e);
    }
    TGSET_ITER_END;

    TGSET_ITER (f, node->bm_faces) {
      if (f->head.hflag & VISIT_TAG) {
        continue;
      }

      BLI_array_append(faces, f);
      f->head.hflag |= VISIT_TAG;
    }
    TGSET_ITER_END;

    nodedata[i].verts = verts;
    nodedata[i].edges = edges;
    nodedata[i].faces = faces;

    nodedata[i].totvert = BLI_array_len(verts);
    nodedata[i].totedge = BLI_array_len(edges);
    nodedata[i].totface = BLI_array_len(faces);
  }

  BMAllocTemplate templ = {
      pbvh->bm->totvert, pbvh->bm->totedge, pbvh->bm->totloop, pbvh->bm->totface};
  struct BMeshCreateParams params = {0};

  BMesh *bm2 = BM_mesh_create(&templ, &params);

  CustomData_copy_all_layout(&pbvh->bm->vdata, &bm2->vdata);
  CustomData_copy_all_layout(&pbvh->bm->edata, &bm2->edata);
  CustomData_copy_all_layout(&pbvh->bm->ldata, &bm2->ldata);
  CustomData_copy_all_layout(&pbvh->bm->pdata, &bm2->pdata);

  CustomData_bmesh_init_pool(&bm2->vdata, pbvh->bm->totvert, BM_VERT);
  CustomData_bmesh_init_pool(&bm2->edata, pbvh->bm->totedge, BM_EDGE);
  CustomData_bmesh_init_pool(&bm2->ldata, pbvh->bm->totloop, BM_LOOP);
  CustomData_bmesh_init_pool(&bm2->pdata, pbvh->bm->totface, BM_FACE);

  BMVert **verts = NULL;
  BMEdge **edges = NULL;
  BMFace **faces = NULL;
  BLI_array_declare(verts);
  BLI_array_declare(edges);
  BLI_array_declare(faces);

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].totvert; j++) {
      BMVert *v1 = nodedata[i].verts[j];
      BMVert *v2 = BM_vert_create(bm2, v1->co, NULL, BM_CREATE_NOP);
      BM_elem_attrs_copy_ex(pbvh->bm, bm2, v1, v2, 0, 0L);

      v2->head.index = v1->head.index = BLI_array_len(verts);
      BLI_array_append(verts, v2);
    }
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].totedge; j++) {
      BMEdge *e1 = nodedata[i].edges[j];
      BMEdge *e2 = BM_edge_create(
          bm2, verts[e1->v1->head.index], verts[e1->v2->head.index], NULL, BM_CREATE_NOP);
      BM_elem_attrs_copy_ex(pbvh->bm, bm2, e1, e2, 0, 0L);

      e2->head.index = e1->head.index = BLI_array_len(edges);
      BLI_array_append(edges, e2);
    }
  }

  BMVert **fvs = NULL;
  BMEdge **fes = NULL;
  BLI_array_declare(fvs);
  BLI_array_declare(fes);

  for (int i = 0; i < pbvh->totnode; i++) {
    for (int j = 0; j < nodedata[i].totface; j++) {
      BMFace *f1 = nodedata[i].faces[j];

      BLI_array_clear(fvs);
      BLI_array_clear(fes);

      int totloop = 0;
      BMLoop *l1 = f1->l_first;
      do {
        BLI_array_append(fvs, verts[l1->v->head.index]);
        BLI_array_append(fes, edges[l1->e->head.index]);
        l1 = l1->next;
        totloop++;
      } while (l1 != f1->l_first);

      BMFace *f2 = BM_face_create(bm2, fvs, fes, totloop, NULL, BM_CREATE_NOP);
      f1->head.index = f2->head.index = BLI_array_len(faces);
      BLI_array_append(faces, f2);

      // CustomData_bmesh_copy_data(&pbvh->bm->pdata, &bm2->pdata, f1->head.data,
      // &f2->head.data);
      BM_elem_attrs_copy_ex(pbvh->bm, bm2, f1, f2, 0, 0L);

      BMLoop *l2 = f2->l_first;
      do {
        BM_elem_attrs_copy_ex(pbvh->bm, bm2, l1, l2, 0, 0L);

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

    BLI_table_gset_free(node->bm_faces, NULL);
    BLI_table_gset_free(node->bm_other_verts, NULL);
    BLI_table_gset_free(node->bm_unique_verts, NULL);

    node->bm_faces = bm_faces;
    node->bm_other_verts = bm_other_verts;
    node->bm_unique_verts = bm_unique_verts;

    node->flag |= PBVH_UpdateTris | PBVH_UpdateRedraw;
  }

  MEM_SAFE_FREE(fvs);
  MEM_SAFE_FREE(fes);

  for (int i = 0; i < pbvh->totnode; i++) {
    MEM_SAFE_FREE(nodedata[i].verts);
    MEM_SAFE_FREE(nodedata[i].edges);
    MEM_SAFE_FREE(nodedata[i].faces);
  }

  MEM_SAFE_FREE(verts);
  MEM_SAFE_FREE(edges);
  MEM_SAFE_FREE(faces);

  MEM_freeN(nodedata);

  BM_mesh_free(pbvh->bm);
  pbvh->bm = bm2;

  return bm2;
}

typedef struct SortElem {
  BMElem *elem;
  int index;
  int cd_node_off;
} SortElem;

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
  BMesh *bm = pbvh->bm;

  int **save_other_vs = MEM_calloc_arrayN(pbvh->totnode, sizeof(int *), __func__);
  int **save_unique_vs = MEM_calloc_arrayN(pbvh->totnode, sizeof(int *), __func__);
  int **save_fs = MEM_calloc_arrayN(pbvh->totnode, sizeof(int *), __func__);

  SortElem *verts = MEM_malloc_arrayN(bm->totvert, sizeof(SortElem), __func__);
  SortElem *edges = MEM_malloc_arrayN(bm->totedge, sizeof(SortElem), __func__);
  SortElem *faces = MEM_malloc_arrayN(bm->totface, sizeof(SortElem), __func__);

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
    int *other_vs = NULL;
    int *unique_vs = NULL;
    int *fs = NULL;

    BLI_array_declare(other_vs);
    BLI_array_declare(unique_vs);
    BLI_array_declare(fs);

    PBVHNode *node = pbvh->nodes + i;
    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BMVert *v;
    BMFace *f;

    TGSET_ITER (v, node->bm_unique_verts) {
      BLI_array_append(unique_vs, v->head.index);
    }
    TGSET_ITER_END;
    TGSET_ITER (v, node->bm_other_verts) {
      BLI_array_append(other_vs, v->head.index);
    }
    TGSET_ITER_END;
    TGSET_ITER (f, node->bm_faces) {
      BLI_array_append(fs, f->head.index);
    }
    TGSET_ITER_END;

    save_unique_vs[i] = unique_vs;
    save_other_vs[i] = other_vs;
    save_fs[i] = fs;
  }

  qsort(verts, bm->totvert, sizeof(SortElem), sort_verts_faces);
  qsort(edges, bm->totedge, sizeof(SortElem), sort_edges);
  qsort(faces, bm->totface, sizeof(SortElem), sort_verts_faces);

  uint *vs = MEM_malloc_arrayN(bm->totvert, sizeof(int), __func__);
  uint *es = MEM_malloc_arrayN(bm->totedge, sizeof(int), __func__);
  uint *fs = MEM_malloc_arrayN(bm->totface, sizeof(int), __func__);

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

  BM_mesh_remap(bm, vs, es, fs, NULL);

  // create new mappings
  BMVert **mapvs = MEM_malloc_arrayN(bm->totvert, sizeof(BMVert *), __func__);
  BMEdge **mapes = MEM_malloc_arrayN(bm->totedge, sizeof(BMEdge *), __func__);
  BMFace **mapfs = MEM_malloc_arrayN(bm->totface, sizeof(BMFace *), __func__);

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

    BLI_table_gset_free(node->bm_unique_verts, NULL);
    BLI_table_gset_free(node->bm_other_verts, NULL);
    BLI_table_gset_free(node->bm_faces, NULL);

    node->bm_unique_verts = BLI_table_gset_new("bm_unique_verts");
    node->bm_other_verts = BLI_table_gset_new("bm_other_verts");
    node->bm_faces = BLI_table_gset_new("bm_faces");

    int *unique_vs = save_unique_vs[i];
    int *other_vs = save_other_vs[i];
    int *fs = save_fs[i];

    for (int j = 0; j < tot_unique_vs; j++) {
      BLI_table_gset_add(node->bm_unique_verts, mapvs[unique_vs[j]]);
    }
    for (int j = 0; j < tot_other_vs; j++) {
      BLI_table_gset_add(node->bm_other_verts, mapvs[other_vs[j]]);
    }

    for (int j = 0; j < tot_fs; j++) {
      BLI_table_gset_add(node->bm_faces, mapfs[fs[j]]);
    }

    MEM_SAFE_FREE(save_unique_vs[i]);
    MEM_SAFE_FREE(save_other_vs[i]);
    MEM_SAFE_FREE(save_fs[i]);

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

  MEM_SAFE_FREE(save_other_vs);
  MEM_SAFE_FREE(save_unique_vs);
  MEM_SAFE_FREE(save_fs);

  return pbvh->bm;
}

// only floats! and 8 byte aligned!
typedef struct CacheParams {
  float vchunk, echunk, lchunk, pchunk;
  int cluster_steps, cluster_size;
} CacheParams;

typedef struct CacheParamDef {
  char name[32];
  float defvalue, min, max;
} CacheParamDef;

CacheParamDef pbvh_bmesh_cache_param_def[] = {{"vchunk", 512.0f, 256.0f, 1024.0f * 12.0f},
                                              {"echunk", 512.0f, 256.0f, 1024.0f * 12.0f},
                                              {"lchunk", 512.0f, 256.0f, 1024.0f * 12.0f},
                                              {"pchunk", 512.0f, 256.0f, 1024.0f * 12.0f},
                                              {"cluster_steps", 512.0f, 1.0f, 256.0f},
                                              {"cluster_size", 512.0f, 1.0f, 8192.0f * 32.0f}};

int pbvh_bmesh_cache_test_totparams()
{
  return sizeof(pbvh_bmesh_cache_param_def) / sizeof(*pbvh_bmesh_cache_param_def);
}

void pbvh_bmesh_cache_test_default_params(CacheParams *params)
{
  float *fparams = (float *)params;
  int totparam = pbvh_bmesh_cache_test_totparams();

  for (int i = 0; i < totparam; i++) {
    fparams[i] = pbvh_bmesh_cache_param_def[i].defvalue;
  }
}

static void *hashco(float fx, float fy, float fz, float fdimen)
{
  double x = (double)fx;
  double y = (double)fy;
  double z = (double)fz;
  double dimen = (double)fdimen;

  return (void *)((intptr_t)(z * dimen * dimen * dimen + y * dimen * dimen + x * dimen));
}

typedef struct MeshTest {
  float (*v_co)[3];
  float (*v_no)[3];
  int *v_e;
  int *v_index;
  int *v_flag;

  int *e_v1;
  int *e_v2;
  int *e_v1_next;
  int *e_v2_next;
  int *e_l;
  int *e_flag;
  int *e_index;

  int *l_v;
  int *l_e;
  int *l_f;
  int *l_next;
  int *l_prev;
  int *l_radial_next;
  int *l_radial_prev;

  int *f_l;
  int *f_index;
  int *f_flag;

  int totvert, totedge, totloop, totface;
  MemArena *arena;
} MeshTest;

typedef struct ElemHeader {
  short type, hflag;
  int index;
  void *data;
} ElemHeader;

typedef struct MeshVert2 {
  ElemHeader head;
  float co[3];
  float no[3];
  int e;
} MeshVert2;

typedef struct MeshEdge2 {
  ElemHeader head;
  int v1, v2;
  int v1_next, v2_next;
  int l;
} MeshEdge2;

typedef struct MeshLoop2 {
  ElemHeader head;
  int v, e, f, next, prev;
  int radial_next, radial_prev;
} MeshLoop2;

typedef struct MeshFace2 {
  ElemHeader head;
  int l, len;
  float no[3];
} MeshFace2;

typedef struct MeshTest2 {
  MeshVert2 *verts;
  MeshEdge2 *edges;
  MeshLoop2 *loops;
  MeshFace2 *faces;

  int totvert, totedge, totloop, totface;
  MemArena *arena;
} MeshTest2;

static MeshTest2 *meshtest2_from_bm(BMesh *bm)
{
  MeshTest2 *m2 = MEM_callocN(sizeof(MeshTest2), "MeshTest2");
  m2->arena = BLI_memarena_new(1024 * 32, "MeshTest2 arena");

  m2->totvert = bm->totvert;
  m2->totedge = bm->totedge;
  m2->totloop = bm->totloop;
  m2->totface = bm->totface;

  BMVert *v;
  BMEdge *e;
  BMFace *f;
  BMIter iter;

  int lindex = 0;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;
    do {
      l->head.index = lindex++;
    } while ((l = l->next) != f->l_first);
  }

  m2->totloop = lindex;

  m2->verts = MEM_calloc_arrayN(bm->totvert, sizeof(MeshVert2), "MeshVert2s");
  m2->edges = MEM_calloc_arrayN(bm->totedge, sizeof(MeshEdge2), "MeshEdge2s");
  m2->loops = MEM_calloc_arrayN(m2->totloop, sizeof(MeshLoop2), "MeshLoop2s");
  m2->faces = MEM_calloc_arrayN(bm->totface, sizeof(MeshFace2), "MeshFace2s");

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    const int vi = v->head.index;

    copy_v3_v3(m2->verts[vi].co, v->co);
    copy_v3_v3(m2->verts[vi].no, v->no);

    m2->verts[vi].e = v->e ? v->e->head.index : -1;
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    const int ei = e->head.index;

    m2->edges[ei].v1 = e->v1->head.index;
    m2->edges[ei].v2 = e->v2->head.index;
    m2->edges[ei].l = e->l ? e->l->head.index : -1;

    m2->edges[ei].v1_next = e->v1_disk_link.next->head.index;
    m2->edges[ei].v2_next = e->v2_disk_link.next->head.index;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int fi = f->head.index;

    m2->faces[fi].len = f->len;
    copy_v3_v3(m2->faces[fi].no, f->no);

    BMLoop *l = f->l_first;
    do {
      int li = l->head.index;

      m2->loops[li].v = l->v->head.index;
      m2->loops[li].e = l->e->head.index;
      m2->loops[li].f = l->f->head.index;

      m2->loops[li].radial_next = l->radial_next->head.index;
      m2->loops[li].radial_prev = l->radial_prev->head.index;

      m2->loops[li].next = l->next->head.index;
      m2->loops[li].prev = l->prev->head.index;
    } while ((l = l->next) != f->l_first);
  }

  return m2;
}

static void free_meshtest2(MeshTest2 *m2)
{
  BLI_memarena_free(m2->arena);
  MEM_freeN(m2);
}

static MeshTest *meshtest_from_bm(BMesh *bm)
{
  MeshTest *m = MEM_callocN(sizeof(MeshTest), "MeshTest");
  m->arena = BLI_memarena_new(1024 * 32, "m->arena");

  m->v_co = BLI_memarena_alloc(m->arena, bm->totvert * sizeof(*m->v_co));
  m->v_no = BLI_memarena_alloc(m->arena, bm->totvert * sizeof(*m->v_no));
  m->v_e = BLI_memarena_alloc(m->arena, bm->totvert * sizeof(*m->v_e));
  m->v_flag = BLI_memarena_alloc(m->arena, bm->totvert * sizeof(*m->v_flag));
  m->v_index = BLI_memarena_alloc(m->arena, bm->totvert * sizeof(*m->v_index));

  m->e_v1 = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_v1_next = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_v2 = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_v2_next = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_l = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_index = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));
  m->e_flag = BLI_memarena_alloc(m->arena, bm->totedge * sizeof(*m->e_v1));

  m->l_v = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_e = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_f = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_next = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_prev = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_radial_next = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));
  m->l_radial_prev = BLI_memarena_alloc(m->arena, bm->totloop * sizeof(*m->l_e));

  m->f_l = BLI_memarena_alloc(m->arena, bm->totface * sizeof(*m->f_l));

  m->totvert = bm->totvert;
  m->totedge = bm->totedge;
  m->totface = bm->totface;
  m->totloop = bm->totloop;

  BMVert *v;
  BMEdge *e;
  BMFace *f;
  BMIter iter;

  int lindex = 0;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;
    do {
      l->head.index = lindex++;
    } while ((l = l->next) != f->l_first);
  }

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(m->v_co[v->head.index], v->co);
    copy_v3_v3(m->v_no[v->head.index], v->no);

    m->v_e[v->head.index] = v->e ? v->e->head.index : -1;
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    m->e_v1[e->head.index] = e->v1->head.index;
    m->e_v2[e->head.index] = e->v2->head.index;

    m->e_v1_next[e->head.index] = e->v1_disk_link.next->head.index;
    m->e_v2_next[e->head.index] = e->v2_disk_link.next->head.index;

    m->e_l[e->head.index] = e->l ? e->l->head.index : -1;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    m->f_l[f->head.index] = f->l_first->head.index;

    BMLoop *l = f->l_first;
    do {
      const int li = l->head.index;

      m->l_e[li] = l->e->head.index;
      m->l_v[li] = l->v->head.index;
      m->l_f[li] = f->head.index;
      m->l_next[li] = l->next->head.index;
      m->l_prev[li] = l->prev->head.index;
      m->l_radial_next[li] = l->radial_next->head.index;
      m->l_radial_prev[li] = l->radial_prev->head.index;
    } while ((l = l->next) != f->l_first);
  }

  return m;
}

static void free_meshtest(MeshTest *m)
{
  BLI_memarena_free(m->arena);
  MEM_freeN(m);
}

#define SMOOTH_TEST_STEPS 20

double pbvh_bmesh_smooth_test(BMesh *bm, PBVH *pbvh)
{
  double average = 0.0f;
  double average_tot = 0.0f;

  for (int iter = 0; iter < SMOOTH_TEST_STEPS; iter++) {
    RNG *rng = BLI_rng_new(0);

    double time1 = PIL_check_seconds_timer();

    for (int step = 0; step < 5; step++) {
      for (int i = 0; i < bm->totvert; i++) {
        int vi = BLI_rng_get_int(rng) % bm->totvert;
        BMVert *v = bm->vtable[vi];
        BMEdge *e = v->e;
        float co[3];

        zero_v3(co);
        int tot = 0.0;

        if (!e) {
          continue;
        }

        do {
          BMVert *v2 = BM_edge_other_vert(e, v);
          float co2[3];

          sub_v3_v3v3(co2, v2->co, v->co);
          madd_v3_v3fl(co2, v->no, -dot_v3v3(v->no, co2) * 0.9f);
          add_v3_v3(co, co2);

          tot++;

          e = e->v1 == v ? e->v1_disk_link.next : e->v2_disk_link.next;
        } while (e != v->e);

        if (tot == 0.0) {
          continue;
        }

        mul_v3_fl(co, 1.0f / (float)tot);
        madd_v3_v3fl(v->co, co, 0.5f);
      }
    }

    double time2 = PIL_check_seconds_timer();

    double time = time2 - time1;

    printf("  time: %.5f, %d of %d\n", time, iter, SMOOTH_TEST_STEPS);

    // skip first five
    if (iter >= 5) {
      average += time;
      average_tot += 1.0f;
    }

    BLI_rng_free(rng);
  }

  printf("time: %.5f\n", average / average_tot);
  return average / average_tot;
}

double pbvh_meshtest2_smooth_test(MeshTest2 *m2, PBVH *pbvh)
{
  double average = 0.0f;
  double average_tot = 0.0f;

  for (int iter = 0; iter < SMOOTH_TEST_STEPS; iter++) {
    RNG *rng = BLI_rng_new(0);

    double time1 = PIL_check_seconds_timer();

    for (int step = 0; step < 5; step++) {
      for (int i = 0; i < m2->totvert; i++) {
        int vi = BLI_rng_get_int(rng) % m2->totvert;
        MeshVert2 *v = m2->verts + vi;
        MeshEdge2 *e = v->e != -1 ? m2->edges + v->e : NULL;
        float co[3];

        zero_v3(co);
        int tot = 0.0;

        if (!e) {
          continue;
        }

        int enext = -1;

        do {
          MeshVert2 *v2 = vi == e->v1 ? m2->verts + e->v2 : m2->verts + e->v1;
          float co2[3];

          sub_v3_v3v3(co2, v2->co, v->co);
          madd_v3_v3fl(co2, v->no, -dot_v3v3(v->no, co2) * 0.9f);
          add_v3_v3(co, co2);

          tot++;

          enext = e->v1 == vi ? e->v1_next : e->v2_next;
          e = m2->edges + enext;
        } while (enext != v->e);

        if (tot == 0.0) {
          continue;
        }

        mul_v3_fl(co, 1.0f / (float)tot);
        madd_v3_v3fl(v->co, co, 0.5f);
      }
    }

    double time2 = PIL_check_seconds_timer();

    double time = time2 - time1;

    printf("  time: %.5f, %d of %d\n", time, iter, SMOOTH_TEST_STEPS);

    // skip first five
    if (iter >= 5) {
      average += time;
      average_tot += 1.0f;
    }

    BLI_rng_free(rng);
  }

  printf("time: %.5f\n", average / average_tot);
  return average / average_tot;
}

double pbvh_meshtest_smooth_test(MeshTest *m, PBVH *pbvh)
{
  double average = 0.0f;
  double average_tot = 0.0f;

  for (int iter = 0; iter < SMOOTH_TEST_STEPS; iter++) {
    RNG *rng = BLI_rng_new(0);

    double time1 = PIL_check_seconds_timer();

    for (int step = 0; step < 5; step++) {
      for (int i = 0; i < m->totvert; i++) {
        int vi = BLI_rng_get_int(rng) % m->totvert;
        // BMVert *v = bm->vtable[vi];
        const int startei = m->v_e[vi];
        int ei = startei;

        float co[3];

        zero_v3(co);
        int tot = 0.0;

        if (ei == -1) {
          continue;
        }

        const float *no = m->v_no[vi];
        const float *vco = m->v_co[vi];

        do {
          int ev1 = m->e_v1[ei];
          int ev2 = m->e_v2[ei];

          int v2i = ev1 == vi ? ev2 : ev1;

          float co2[3];

          sub_v3_v3v3(co2, m->v_co[v2i], vco);
          madd_v3_v3fl(co2, no, -dot_v3v3(no, co2) * 0.9f);
          add_v3_v3(co, co2);

          tot++;

          ei = ev1 == vi ? m->e_v1_next[ei] : m->e_v2_next[ei];
        } while (ei != startei);

        if (tot == 0.0) {
          continue;
        }

        mul_v3_fl(co, 1.0f / (float)tot);
        madd_v3_v3fl(m->v_co[vi], co, 0.5f);
      }
    }

    double time2 = PIL_check_seconds_timer();

    double time = time2 - time1;

    printf("  time: %.5f, %d of %d\n", time, iter + 1, SMOOTH_TEST_STEPS);

    // skip first five
    if (iter >= 5) {
      average += time;
      average_tot += 1.0f;
    }

    BLI_rng_free(rng);
  }

  printf("time: %.5f\n", average / average_tot);
  return average / average_tot;
}

/*
test results from blenderartists thread:

random, cluster,  percent,  data, data_perc, indices, ind_perc, mem (gb)
  [1.22,    1.04,   14.42,  0.73,     67,     0.94,    29,      0],
  [1.49,    1.46,   2.35,   1.10,     36,     1.17,    27,      0],
  [1.29,    1.13,     14,   0.75,  71.54,     0.89,    45.08 ,  0],
  [1.58,    1.40,   12.3,   1.09,   44.7,     1.11,    42.42,   16],
  [1.53,    1.36,   12.77,  1.08,  41.6,      1.07,    42.91,   0],
  [1.56,    1.39,   12.47,  1.09,  42.65,     1.10,    42.15,   16],
  [1.22,    1.06,   15.05,  0.75,   63.85,    0.82,    49.67,   32]

[random]           average: 1.41 variange: 0.15 median: 1.49
[cluster]          average: 1.26 variange: 0.17 median: 1.36
[cluster-percent]  average: 11.91 variange: 4.02 median: 12.77
[data]             average: 0.94 variange: 0.17 median: 1.08
[data-percent]     average: 52.48 variange: 13.37 median: 44.70
[indices]          average: 1.01 variange: 0.12 median: 1.07
[indices-percent]  average: 39.75 variange: 7.82 median: 42.42

So looks like the biggest gain is from replacing pointers with indices
(which lessens total memory bandwidth).  The pure data-oriented version
is a tad bit faster then the index-replacement one, but not by that much.
*/

void pbvh_bmesh_cache_test(CacheParams *params, BMesh **r_bm, PBVH **r_pbvh_out)
{
  // build mesh
  const int steps = 325;

  printf("== Starting Test ==\n");

  printf("building test mesh. . .\n");

  BMAllocTemplate templ = {0, 0, 0, 0};

  BMesh *bm = BM_mesh_create(
      &templ,
      &((struct BMeshCreateParams){.id_elem_mask = BM_VERT | BM_EDGE | BM_FACE,
                                   .id_map = true,
                                   .create_unique_ids = true,
                                   .temporary_ids = false,
                                   .no_reuse_ids = false}));

  // reinit pools
  BLI_mempool_destroy(bm->vpool);
  BLI_mempool_destroy(bm->epool);
  BLI_mempool_destroy(bm->lpool);
  BLI_mempool_destroy(bm->fpool);

  bm->vpool = BLI_mempool_create(sizeof(BMVert), 0, (int)params->vchunk, BLI_MEMPOOL_ALLOW_ITER);
  bm->epool = BLI_mempool_create(sizeof(BMEdge), 0, (int)params->echunk, BLI_MEMPOOL_ALLOW_ITER);
  bm->lpool = BLI_mempool_create(sizeof(BMLoop), 0, (int)params->lchunk, BLI_MEMPOOL_ALLOW_ITER);
  bm->fpool = BLI_mempool_create(sizeof(BMFace), 0, (int)params->pchunk, BLI_MEMPOOL_ALLOW_ITER);

  GHash *vhash = BLI_ghash_ptr_new("vhash");

  float df = 1.0f / (float)steps;

  int hashdimen = steps * 8;

  BMVert **grid = MEM_malloc_arrayN(steps * steps, sizeof(*grid), "bmvert grid");

  BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_INT32, "__dyntopo_vert_node");
  BM_data_layer_add_named(bm, &bm->pdata, CD_PROP_INT32, "__dyntopo_face_node");
  BM_data_layer_add(bm, &bm->pdata, CD_SCULPT_FACE_SETS);

  BM_data_layer_add(bm, &bm->vdata, CD_PAINT_MASK);
  BM_data_layer_add(bm, &bm->vdata, CD_DYNTOPO_VERT);
  BM_data_layer_add(bm, &bm->vdata, CD_PROP_COLOR);

  for (int side = 0; side < 6; side++) {
    int axis = side >= 3 ? side - 3 : side;
    float sign = side >= 3 ? -1.0f : 1.0f;

    printf("AXIS: %d\n", axis);

    float u = 0.0f;

    for (int i = 0; i < steps; i++, u += df) {
      float v = 0.0f;

      for (int j = 0; j < steps; j++, v += df) {
        float co[3];

        co[axis] = u;
        co[(axis + 1) % 3] = v;
        co[(axis + 2) % 3] = sign;

        // turn into sphere
        normalize_v3(co);

        void *key = hashco(co[0], co[1], co[2], hashdimen);

#if 0
        printf("%.3f %.3f %.3f, key: %p i: %d j: %d df: %f, u: %f v: %f\n",
               co[0],
               co[1],
               co[2],
               key,
               i,
               j,
               df,
               u,
               v);
#endif

        void **val = NULL;

        if (!BLI_ghash_ensure_p(vhash, key, &val)) {
          BMVert *v2 = BM_vert_create(bm, co, NULL, BM_CREATE_NOP);

          *val = (void *)v2;
        }

        BMVert *v2 = (BMVert *)*val;
        int idx = j * steps + i;

        grid[idx] = v2;
      }
    }

    for (int i = 0; i < steps - 1; i++) {
      for (int j = 0; j < steps - 1; j++) {
        int idx1 = j * steps + i;
        int idx2 = (j + 1) * steps + i;
        int idx3 = (j + 1) * steps + i + 1;
        int idx4 = j * steps + i + 1;

        BMVert *v1 = grid[idx1];
        BMVert *v2 = grid[idx2];
        BMVert *v3 = grid[idx3];
        BMVert *v4 = grid[idx4];

        if (v1 == v2 || v1 == v3 || v1 == v4 || v2 == v3 || v2 == v4 || v3 == v4) {
          printf("ERROR!\n");
          continue;
        }

        if (sign < 0) {
          BMVert *vs[4] = {v4, v3, v2, v1};
          BM_face_create_verts(bm, vs, 4, NULL, BM_CREATE_NOP, true);
        }
        else {
          BMVert *vs[4] = {v1, v2, v3, v4};
          BM_face_create_verts(bm, vs, 4, NULL, BM_CREATE_NOP, true);
        }
      }
    }
  }

  // randomize
  uint *rands[4];
  uint tots[4] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};

  RNG *rng = BLI_rng_new(0);

  for (uint i = 0; i < 4; i++) {
    rands[i] = MEM_malloc_arrayN(tots[i], sizeof(uint), "rands[i]");

    for (uint j = 0; j < tots[i]; j++) {
      rands[i][j] = j;
    }

    for (uint j = 0; j < tots[i] >> 1; j++) {
      int j2 = BLI_rng_get_int(rng) % tots[i];
      SWAP(uint, rands[i][j], rands[i][j2]);
    }
  }

  BM_mesh_remap(bm, rands[0], rands[1], rands[3], rands[2]);

  for (int i = 0; i < 4; i++) {
    MEM_SAFE_FREE(rands[i]);
  }

  BLI_rng_free(rng);
  BLI_ghash_free(vhash, NULL, NULL);
  MEM_SAFE_FREE(grid);

  printf("totvert: %d, totface: %d, tottri: %d\n", bm->totvert, bm->totface, bm->totface * 2);

  int cd_vert_node = CustomData_get_named_layer_index(
      &bm->vdata, CD_PROP_INT32, "__dyntopo_vert_node");
  int cd_face_node = CustomData_get_named_layer_index(
      &bm->pdata, CD_PROP_INT32, "__dyntopo_face_node");
  int cd_face_area = CustomData_get_named_layer_index(
      &bm->pdata, CD_PROP_FLOAT, "__dyntopo_face_areas");

  cd_vert_node = bm->vdata.layers[cd_vert_node].offset;
  cd_face_node = bm->pdata.layers[cd_face_node].offset;
  cd_face_area = bm->pdata.layers[cd_face_area].offset;

  const int cd_dyn_vert = CustomData_get_offset(&bm->vdata, CD_DYNTOPO_VERT);
  BMLog *bmlog = BM_log_create(bm, cd_dyn_vert);

  PBVH *pbvh = BKE_pbvh_new();

  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_FACE);
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  BKE_pbvh_build_bmesh(
      pbvh, bm, false, bmlog, cd_vert_node, cd_face_node, cd_dyn_vert, cd_face_area, false);

  int loop_size = sizeof(BMLoop) - sizeof(void *) * 4;

  size_t s1 = 0, s2 = 0, s3 = 0;
  s1 = sizeof(BMVert) * (size_t)bm->totvert + sizeof(BMEdge) * (size_t)bm->totedge +
       sizeof(BMLoop) * (size_t)bm->totloop + sizeof(BMFace) * (size_t)bm->totface;
  s2 = sizeof(MeshVert2) * (size_t)bm->totvert + sizeof(MeshEdge2) * (size_t)bm->totedge +
       sizeof(MeshLoop2) * (size_t)bm->totloop + sizeof(MeshFace2) * (size_t)bm->totface;
  s3 = (size_t)loop_size * (size_t)bm->totvert + sizeof(BMEdge) * (size_t)bm->totedge +
       sizeof(BMLoop) * (size_t)bm->totloop + sizeof(BMFace) * (size_t)bm->totface;

  double times[4];
  char *names[4];

  int cd_overhead = 0;
  CustomData *cdatas[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  int ctots[4] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};
  for (int i = 0; i < 4; i++) {
    cd_overhead += cdatas[i]->totsize * ctots[i];
  }

  s1 += cd_overhead;
  s2 += cd_overhead;

  printf("    bmesh mem size: %.2fmb %.2fmb\n",
         (float)s1 / 1024.0f / 1024.0f,
         (float)s3 / 1024.0f / 1024.0f);
  printf("meshtest2 mem size: %.2fmb\n", (float)s2 / 1024.0f / 1024.0f);

  printf("= BMesh random order\n");
  times[0] = pbvh_bmesh_smooth_test(bm, pbvh);
  names[0] = "random order";

  BMesh *bm2 = BKE_pbvh_reorder_bmesh(pbvh);

  printf("= BMesh vertex cluster order\n");

  bm2->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm2->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  BM_mesh_elem_table_ensure(bm2, BM_VERT | BM_FACE);
  BM_mesh_elem_index_ensure(bm2, BM_VERT | BM_EDGE | BM_FACE);

  times[1] = pbvh_bmesh_smooth_test(bm2, pbvh);
  names[1] = "vertex cluser";

  printf("= Pure data-oriented (struct of arrays)\n");
  MeshTest *m = meshtest_from_bm(bm2);

  times[2] = pbvh_meshtest_smooth_test(m, pbvh);
  names[2] = "data-oriented";

  free_meshtest(m);

  printf("= Object-oriented but with integer indices instead of pointers\n");
  MeshTest2 *m2 = meshtest2_from_bm(bm2);

  times[3] = pbvh_meshtest2_smooth_test(m2, pbvh);
  names[3] = "integer indices";

  free_meshtest2(m2);

  if (bm2 && bm2 != bm) {
    BM_mesh_free(bm2);
  }

  if (r_bm) {
    *r_bm = bm;
  }
  else {
    BM_mesh_free(bm);
  }

  if (r_pbvh_out) {
    *r_pbvh_out = pbvh;
  }
  else {
    BKE_pbvh_free(pbvh);
  }

  printf("\n== Times ==\n");

  for (int i = 0; i < ARRAY_SIZE(times); i++) {
    if (i > 0) {
      double perc = (times[0] / times[i] - 1.0) * 100.0;
      printf("  %s : %.2f (%.2f%% improvement)\n", names[i], times[i], perc);
    }
    else {
      printf("  %s : %.2f\n", names[i], times[i]);
    }
  }

  printf("== Test Finished ==\n");
}

#include "BLI_smallhash.h"

static void hash_test()
{
  const int count = 1024 * 1024 * 4;

  int *data = MEM_callocN(sizeof(*data) * count, "test data");

  TableGSet *gs = BLI_table_gset_new("test");
  GHash *gh = BLI_ghash_ptr_new("test");
  SmallHash sh;

  BLI_smallhash_init(&sh);
  RNG *rng = BLI_rng_new(0);

  for (int i = 0; i < count; i++) {
    data[i] = i;
  }

  printf("== creation: table_gset ==\n");
  double t = PIL_check_seconds_timer();

  for (int i = 0; i < count; i++) {
    int ri = BLI_rng_get_int(rng) % count;
    int *ptr = POINTER_FROM_INT(data[ri]);

    BLI_table_gset_add(gs, ptr);
  }
  printf("  %.3f\n", PIL_check_seconds_timer() - t);

  printf("== creation: ghash ==\n");
  t = PIL_check_seconds_timer();

  for (int i = 0; i < count; i++) {
    int ri = BLI_rng_get_int(rng) % count;
    int *ptr = POINTER_FROM_INT(data[ri]);

    BLI_ghash_insert(gh, ptr, POINTER_FROM_INT(i));
  }
  printf("  %.3f\n", PIL_check_seconds_timer() - t);

  printf("== creation: small hash ==\n");
  t = PIL_check_seconds_timer();

  for (int i = 0; i < count; i++) {
    int ri = BLI_rng_get_int(rng) % count;
    int *ptr = POINTER_FROM_INT(data[ri]);

    BLI_smallhash_insert(&sh, (uintptr_t)ptr, POINTER_FROM_INT(i));
  }
  printf("  %.3f\n", PIL_check_seconds_timer() - t);

  printf("== lookup: g hash ==\n");
  t = PIL_check_seconds_timer();

  for (int i = 0; i < count; i++) {
    int ri = BLI_rng_get_int(rng) % count;
    int *ptr = POINTER_FROM_INT(data[ri]);

    BLI_ghash_lookup(gh, ptr);
  }
  printf("  %.3f\n", PIL_check_seconds_timer() - t);

  printf("== lookup: small hash ==\n");
  t = PIL_check_seconds_timer();

  for (int i = 0; i < count; i++) {
    int ri = BLI_rng_get_int(rng) % count;
    int *ptr = POINTER_FROM_INT(data[ri]);

    BLI_smallhash_lookup(&sh, (uintptr_t)ptr);
  }
  printf("  %.3f\n", PIL_check_seconds_timer() - t);

  BLI_rng_free(rng);
  BLI_ghash_free(gh, NULL, NULL);
  BLI_smallhash_release(&sh);
  BLI_table_gset_free(gs, NULL);

  MEM_freeN(data);
}

void pbvh_bmesh_do_cache_test()
{
  for (int i = 0; i < 15; i++) {
    printf("\n\n====== %d of %d =====\n", i + 1, 15);
    hash_test();
  }
  // pbvh_bmesh_cache_test_default_params(&params);
  // pbvh_bmesh_cache_test(&params, &bm, &pbvh);
}
