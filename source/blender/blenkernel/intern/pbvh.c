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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_task.h"

#include "DNA_meshdata_types.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_mesh.h" /* for BKE_mesh_calc_normals */
#include "BKE_paint.h"

#include "GPU_buffers.h"
#include "GPU_immediate.h"

#include "bmesh.h"

#include "atomic_ops.h"

#include "pbvh_intern.h"

#include <limits.h>

#define LEAF_LIMIT 10000

//#define PERFCNTRS

#define STACK_FIXED_DEPTH 100

#define PBVH_THREADED_LIMIT 4

typedef struct PBVHStack {
  PBVHNode *node;
  bool revisiting;
} PBVHStack;

typedef struct PBVHIter {
  PBVH *bvh;
  BKE_pbvh_SearchCallback scb;
  void *search_data;

  PBVHStack *stack;
  int stacksize;

  PBVHStack stackfixed[STACK_FIXED_DEPTH];
  int stackspace;
} PBVHIter;

void BB_reset(BB *bb)
{
  bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
  bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

/* Expand the bounding box to include a new coordinate */
void BB_expand(BB *bb, const float co[3])
{
  for (int i = 0; i < 3; ++i) {
    bb->bmin[i] = min_ff(bb->bmin[i], co[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], co[i]);
  }
}

/* Expand the bounding box to include another bounding box */
void BB_expand_with_bb(BB *bb, BB *bb2)
{
  for (int i = 0; i < 3; ++i) {
    bb->bmin[i] = min_ff(bb->bmin[i], bb2->bmin[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], bb2->bmax[i]);
  }
}

/* Return 0, 1, or 2 to indicate the widest axis of the bounding box */
int BB_widest_axis(const BB *bb)
{
  float dim[3];

  for (int i = 0; i < 3; ++i) {
    dim[i] = bb->bmax[i] - bb->bmin[i];
  }

  if (dim[0] > dim[1]) {
    if (dim[0] > dim[2]) {
      return 0;
    }
    else {
      return 2;
    }
  }
  else {
    if (dim[1] > dim[2]) {
      return 1;
    }
    else {
      return 2;
    }
  }
}

void BBC_update_centroid(BBC *bbc)
{
  for (int i = 0; i < 3; ++i) {
    bbc->bcentroid[i] = (bbc->bmin[i] + bbc->bmax[i]) * 0.5f;
  }
}

/* Not recursive */
static void update_node_vb(PBVH *bvh, PBVHNode *node)
{
  BB vb;

  BB_reset(&vb);

  if (node->flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin(bvh, node, vd, PBVH_ITER_ALL)
    {
      BB_expand(&vb, vd.co);
    }
    BKE_pbvh_vertex_iter_end;
  }
  else {
    BB_expand_with_bb(&vb, &bvh->nodes[node->children_offset].vb);
    BB_expand_with_bb(&vb, &bvh->nodes[node->children_offset + 1].vb);
  }

  node->vb = vb;
}

// void BKE_pbvh_node_BB_reset(PBVHNode *node)
//{
//  BB_reset(&node->vb);
//}
//
// void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3])
//{
//  BB_expand(&node->vb, co);
//}

static bool face_materials_match(const MPoly *f1, const MPoly *f2)
{
  return ((f1->flag & ME_SMOOTH) == (f2->flag & ME_SMOOTH) && (f1->mat_nr == f2->mat_nr));
}

static bool grid_materials_match(const DMFlagMat *f1, const DMFlagMat *f2)
{
  return ((f1->flag & ME_SMOOTH) == (f2->flag & ME_SMOOTH) && (f1->mat_nr == f2->mat_nr));
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices(int *prim_indices, int lo, int hi, int axis, float mid, BBC *prim_bbc)
{
  int i = lo, j = hi;
  for (;;) {
    for (; prim_bbc[prim_indices[i]].bcentroid[axis] < mid; i++) {
      /* pass */
    }
    for (; mid < prim_bbc[prim_indices[j]].bcentroid[axis]; j--) {
      /* pass */
    }

    if (!(i < j)) {
      return i;
    }

    SWAP(int, prim_indices[i], prim_indices[j]);
    i++;
  }
}

/* Returns the index of the first element on the right of the partition */
static int partition_indices_material(PBVH *bvh, int lo, int hi)
{
  const MPoly *mpoly = bvh->mpoly;
  const MLoopTri *looptri = bvh->looptri;
  const DMFlagMat *flagmats = bvh->grid_flag_mats;
  const int *indices = bvh->prim_indices;
  const void *first;
  int i = lo, j = hi;

  if (bvh->looptri) {
    first = &mpoly[looptri[bvh->prim_indices[lo]].poly];
  }
  else {
    first = &flagmats[bvh->prim_indices[lo]];
  }

  for (;;) {
    if (bvh->looptri) {
      for (; face_materials_match(first, &mpoly[looptri[indices[i]].poly]); i++) {
        /* pass */
      }
      for (; !face_materials_match(first, &mpoly[looptri[indices[j]].poly]); j--) {
        /* pass */
      }
    }
    else {
      for (; grid_materials_match(first, &flagmats[indices[i]]); i++) {
        /* pass */
      }
      for (; !grid_materials_match(first, &flagmats[indices[j]]); j--) {
        /* pass */
      }
    }

    if (!(i < j)) {
      return i;
    }

    SWAP(int, bvh->prim_indices[i], bvh->prim_indices[j]);
    i++;
  }
}

void pbvh_grow_nodes(PBVH *bvh, int totnode)
{
  if (UNLIKELY(totnode > bvh->node_mem_count)) {
    bvh->node_mem_count = bvh->node_mem_count + (bvh->node_mem_count / 3);
    if (bvh->node_mem_count < totnode) {
      bvh->node_mem_count = totnode;
    }
    bvh->nodes = MEM_recallocN(bvh->nodes, sizeof(PBVHNode) * bvh->node_mem_count);
  }

  bvh->totnode = totnode;
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(
    PBVH *bvh, GHash *map, unsigned int *face_verts, unsigned int *uniq_verts, int vertex)
{
  void *key, **value_p;

  key = POINTER_FROM_INT(vertex);
  if (!BLI_ghash_ensure_p(map, key, &value_p)) {
    int value_i;
    if (BLI_BITMAP_TEST(bvh->vert_bitmap, vertex) == 0) {
      BLI_BITMAP_ENABLE(bvh->vert_bitmap, vertex);
      value_i = *uniq_verts;
      (*uniq_verts)++;
    }
    else {
      value_i = ~(*face_verts);
      (*face_verts)++;
    }
    *value_p = POINTER_FROM_INT(value_i);
    return value_i;
  }
  else {
    return POINTER_AS_INT(*value_p);
  }
}

/* Find vertices used by the faces in this node and update the draw buffers */
static void build_mesh_leaf_node(PBVH *bvh, PBVHNode *node)
{
  bool has_visible = false;

  node->uniq_verts = node->face_verts = 0;
  const int totface = node->totprim;

  /* reserve size is rough guess */
  GHash *map = BLI_ghash_int_new_ex("build_mesh_leaf_node gh", 2 * totface);

  int(*face_vert_indices)[3] = MEM_mallocN(sizeof(int[3]) * totface, "bvh node face vert indices");

  node->face_vert_indices = (const int(*)[3])face_vert_indices;

  for (int i = 0; i < totface; ++i) {
    const MLoopTri *lt = &bvh->looptri[node->prim_indices[i]];
    for (int j = 0; j < 3; ++j) {
      face_vert_indices[i][j] = map_insert_vert(
          bvh, map, &node->face_verts, &node->uniq_verts, bvh->mloop[lt->tri[j]].v);
    }

    if (!paint_is_face_hidden(lt, bvh->verts, bvh->mloop)) {
      has_visible = true;
    }
  }

  int *vert_indices = MEM_callocN(sizeof(int) * (node->uniq_verts + node->face_verts),
                                  "bvh node vert indices");
  node->vert_indices = vert_indices;

  /* Build the vertex list, unique verts first */
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, map) {
    void *value = BLI_ghashIterator_getValue(&gh_iter);
    int ndx = POINTER_AS_INT(value);

    if (ndx < 0) {
      ndx = -ndx + node->uniq_verts - 1;
    }

    vert_indices[ndx] = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
  }

  for (int i = 0; i < totface; ++i) {
    const int sides = 3;

    for (int j = 0; j < sides; ++j) {
      if (face_vert_indices[i][j] < 0) {
        face_vert_indices[i][j] = -face_vert_indices[i][j] + node->uniq_verts - 1;
      }
    }
  }

  BKE_pbvh_node_mark_rebuild_draw(node);

  BKE_pbvh_node_fully_hidden_set(node, !has_visible);

  BLI_ghash_free(map, NULL, NULL);
}

static void update_vb(PBVH *bvh, PBVHNode *node, BBC *prim_bbc, int offset, int count)
{
  BB_reset(&node->vb);
  for (int i = offset + count - 1; i >= offset; --i) {
    BB_expand_with_bb(&node->vb, (BB *)(&prim_bbc[bvh->prim_indices[i]]));
  }
  node->orig_vb = node->vb;
}

/* Returns the number of visible quads in the nodes' grids. */
int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              int *grid_indices,
                              int totgrid,
                              int gridsize)
{
  const int gridarea = (gridsize - 1) * (gridsize - 1);
  int totquad = 0;

  /* grid hidden layer is present, so have to check each grid for
   * visibility */

  for (int i = 0; i < totgrid; i++) {
    const BLI_bitmap *gh = grid_hidden[grid_indices[i]];

    if (gh) {
      /* grid hidden are present, have to check each element */
      for (int y = 0; y < gridsize - 1; y++) {
        for (int x = 0; x < gridsize - 1; x++) {
          if (!paint_is_grid_face_hidden(gh, gridsize, x, y)) {
            totquad++;
          }
        }
      }
    }
    else {
      totquad += gridarea;
    }
  }

  return totquad;
}

static void build_grid_leaf_node(PBVH *bvh, PBVHNode *node)
{
  int totquads = BKE_pbvh_count_grid_quads(
      bvh->grid_hidden, node->prim_indices, node->totprim, bvh->gridkey.grid_size);
  BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
  BKE_pbvh_node_mark_rebuild_draw(node);
}

static void build_leaf(PBVH *bvh, int node_index, BBC *prim_bbc, int offset, int count)
{
  bvh->nodes[node_index].flag |= PBVH_Leaf;

  bvh->nodes[node_index].prim_indices = bvh->prim_indices + offset;
  bvh->nodes[node_index].totprim = count;

  /* Still need vb for searches */
  update_vb(bvh, &bvh->nodes[node_index], prim_bbc, offset, count);

  if (bvh->looptri) {
    build_mesh_leaf_node(bvh, bvh->nodes + node_index);
  }
  else {
    build_grid_leaf_node(bvh, bvh->nodes + node_index);
  }
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static bool leaf_needs_material_split(PBVH *bvh, int offset, int count)
{
  if (count <= 1) {
    return false;
  }

  if (bvh->looptri) {
    const MLoopTri *first = &bvh->looptri[bvh->prim_indices[offset]];
    const MPoly *mp = &bvh->mpoly[first->poly];

    for (int i = offset + count - 1; i > offset; --i) {
      int prim = bvh->prim_indices[i];
      const MPoly *mp_other = &bvh->mpoly[bvh->looptri[prim].poly];
      if (!face_materials_match(mp, mp_other)) {
        return true;
      }
    }
  }
  else {
    const DMFlagMat *first = &bvh->grid_flag_mats[bvh->prim_indices[offset]];

    for (int i = offset + count - 1; i > offset; --i) {
      int prim = bvh->prim_indices[i];
      if (!grid_materials_match(first, &bvh->grid_flag_mats[prim])) {
        return true;
      }
    }
  }

  return false;
}

/* Recursively build a node in the tree
 *
 * vb is the voxel box around all of the primitives contained in
 * this node.
 *
 * cb is the bounding box around all the centroids of the primitives
 * contained in this node
 *
 * offset and start indicate a range in the array of primitive indices
 */

static void build_sub(PBVH *bvh, int node_index, BB *cb, BBC *prim_bbc, int offset, int count)
{
  int end;
  BB cb_backing;

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = count <= bvh->leaf_limit;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(bvh, offset, count)) {
      build_leaf(bvh, node_index, prim_bbc, offset, count);
      return;
    }
  }

  /* Add two child nodes */
  bvh->nodes[node_index].children_offset = bvh->totnode;
  pbvh_grow_nodes(bvh, bvh->totnode + 2);

  /* Update parent node bounding box */
  update_vb(bvh, &bvh->nodes[node_index], prim_bbc, offset, count);

  if (!below_leaf_limit) {
    /* Find axis with widest range of primitive centroids */
    if (!cb) {
      cb = &cb_backing;
      BB_reset(cb);
      for (int i = offset + count - 1; i >= offset; --i) {
        BB_expand(cb, prim_bbc[bvh->prim_indices[i]].bcentroid);
      }
    }
    const int axis = BB_widest_axis(cb);

    /* Partition primitives along that axis */
    end = partition_indices(bvh->prim_indices,
                            offset,
                            offset + count - 1,
                            axis,
                            (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
                            prim_bbc);
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material(bvh, offset, offset + count - 1);
  }

  /* Build children */
  build_sub(bvh, bvh->nodes[node_index].children_offset, NULL, prim_bbc, offset, end - offset);
  build_sub(
      bvh, bvh->nodes[node_index].children_offset + 1, NULL, prim_bbc, end, offset + count - end);
}

static void pbvh_build(PBVH *bvh, BB *cb, BBC *prim_bbc, int totprim)
{
  if (totprim != bvh->totprim) {
    bvh->totprim = totprim;
    if (bvh->nodes) {
      MEM_freeN(bvh->nodes);
    }
    if (bvh->prim_indices) {
      MEM_freeN(bvh->prim_indices);
    }
    bvh->prim_indices = MEM_mallocN(sizeof(int) * totprim, "bvh prim indices");
    for (int i = 0; i < totprim; ++i) {
      bvh->prim_indices[i] = i;
    }
    bvh->totnode = 0;
    if (bvh->node_mem_count < 100) {
      bvh->node_mem_count = 100;
      bvh->nodes = MEM_callocN(sizeof(PBVHNode) * bvh->node_mem_count, "bvh initial nodes");
    }
  }

  bvh->totnode = 1;
  build_sub(bvh, 0, cb, prim_bbc, 0, totprim);
}

/**
 * Do a full rebuild with on Mesh data structure.
 *
 * \note Unlike mpoly/mloop/verts, looptri is **totally owned** by PBVH
 * (which means it may rewrite it if needed, see #BKE_pbvh_apply_vertCos().
 */
void BKE_pbvh_build_mesh(PBVH *bvh,
                         const MPoly *mpoly,
                         const MLoop *mloop,
                         MVert *verts,
                         int totvert,
                         struct CustomData *vdata,
                         struct CustomData *ldata,
                         const MLoopTri *looptri,
                         int looptri_num)
{
  BBC *prim_bbc = NULL;
  BB cb;

  bvh->type = PBVH_FACES;
  bvh->mpoly = mpoly;
  bvh->mloop = mloop;
  bvh->looptri = looptri;
  bvh->verts = verts;
  bvh->vert_bitmap = BLI_BITMAP_NEW(totvert, "bvh->vert_bitmap");
  bvh->totvert = totvert;
  bvh->leaf_limit = LEAF_LIMIT;
  bvh->vdata = vdata;
  bvh->ldata = ldata;

  BB_reset(&cb);

  /* For each face, store the AABB and the AABB centroid */
  prim_bbc = MEM_mallocN(sizeof(BBC) * looptri_num, "prim_bbc");

  for (int i = 0; i < looptri_num; ++i) {
    const MLoopTri *lt = &looptri[i];
    const int sides = 3;
    BBC *bbc = prim_bbc + i;

    BB_reset((BB *)bbc);

    for (int j = 0; j < sides; ++j) {
      BB_expand((BB *)bbc, verts[bvh->mloop[lt->tri[j]].v].co);
    }

    BBC_update_centroid(bbc);

    BB_expand(&cb, bbc->bcentroid);
  }

  if (looptri_num) {
    pbvh_build(bvh, &cb, prim_bbc, looptri_num);
  }

  MEM_freeN(prim_bbc);
  MEM_freeN(bvh->vert_bitmap);
}

/* Do a full rebuild with on Grids data structure */
void BKE_pbvh_build_grids(PBVH *bvh,
                          CCGElem **grids,
                          int totgrid,
                          CCGKey *key,
                          void **gridfaces,
                          DMFlagMat *flagmats,
                          BLI_bitmap **grid_hidden)
{
  const int gridsize = key->grid_size;

  bvh->type = PBVH_GRIDS;
  bvh->grids = grids;
  bvh->gridfaces = gridfaces;
  bvh->grid_flag_mats = flagmats;
  bvh->totgrid = totgrid;
  bvh->gridkey = *key;
  bvh->grid_hidden = grid_hidden;
  bvh->leaf_limit = max_ii(LEAF_LIMIT / ((gridsize - 1) * (gridsize - 1)), 1);

  BB cb;
  BB_reset(&cb);

  /* For each grid, store the AABB and the AABB centroid */
  BBC *prim_bbc = MEM_mallocN(sizeof(BBC) * totgrid, "prim_bbc");

  for (int i = 0; i < totgrid; ++i) {
    CCGElem *grid = grids[i];
    BBC *bbc = prim_bbc + i;

    BB_reset((BB *)bbc);

    for (int j = 0; j < gridsize * gridsize; ++j) {
      BB_expand((BB *)bbc, CCG_elem_offset_co(key, grid, j));
    }

    BBC_update_centroid(bbc);

    BB_expand(&cb, bbc->bcentroid);
  }

  if (totgrid) {
    pbvh_build(bvh, &cb, prim_bbc, totgrid);
  }

  MEM_freeN(prim_bbc);
}

PBVH *BKE_pbvh_new(void)
{
  PBVH *bvh = MEM_callocN(sizeof(PBVH), "pbvh");

  return bvh;
}

void BKE_pbvh_free(PBVH *bvh)
{
  for (int i = 0; i < bvh->totnode; ++i) {
    PBVHNode *node = &bvh->nodes[i];

    if (node->flag & PBVH_Leaf) {
      if (node->draw_buffers) {
        GPU_pbvh_buffers_free(node->draw_buffers);
      }
      if (node->vert_indices) {
        MEM_freeN((void *)node->vert_indices);
      }
      if (node->face_vert_indices) {
        MEM_freeN((void *)node->face_vert_indices);
      }
      BKE_pbvh_node_layer_disp_free(node);

      if (node->bm_faces) {
        BLI_gset_free(node->bm_faces, NULL);
      }
      if (node->bm_unique_verts) {
        BLI_gset_free(node->bm_unique_verts, NULL);
      }
      if (node->bm_other_verts) {
        BLI_gset_free(node->bm_other_verts, NULL);
      }
    }
  }

  if (bvh->deformed) {
    if (bvh->verts) {
      /* if pbvh was deformed, new memory was allocated for verts/faces -- free it */

      MEM_freeN((void *)bvh->verts);
    }
  }

  if (bvh->looptri) {
    MEM_freeN((void *)bvh->looptri);
  }

  if (bvh->nodes) {
    MEM_freeN(bvh->nodes);
  }

  if (bvh->prim_indices) {
    MEM_freeN(bvh->prim_indices);
  }

  MEM_freeN(bvh);
}

void BKE_pbvh_free_layer_disp(PBVH *bvh)
{
  for (int i = 0; i < bvh->totnode; ++i) {
    BKE_pbvh_node_layer_disp_free(&bvh->nodes[i]);
  }
}

static void pbvh_iter_begin(PBVHIter *iter,
                            PBVH *bvh,
                            BKE_pbvh_SearchCallback scb,
                            void *search_data)
{
  iter->bvh = bvh;
  iter->scb = scb;
  iter->search_data = search_data;

  iter->stack = iter->stackfixed;
  iter->stackspace = STACK_FIXED_DEPTH;

  iter->stack[0].node = bvh->nodes;
  iter->stack[0].revisiting = false;
  iter->stacksize = 1;
}

static void pbvh_iter_end(PBVHIter *iter)
{
  if (iter->stackspace > STACK_FIXED_DEPTH) {
    MEM_freeN(iter->stack);
  }
}

static void pbvh_stack_push(PBVHIter *iter, PBVHNode *node, bool revisiting)
{
  if (UNLIKELY(iter->stacksize == iter->stackspace)) {
    iter->stackspace *= 2;
    if (iter->stackspace != (STACK_FIXED_DEPTH * 2)) {
      iter->stack = MEM_reallocN(iter->stack, sizeof(PBVHStack) * iter->stackspace);
    }
    else {
      iter->stack = MEM_mallocN(sizeof(PBVHStack) * iter->stackspace, "PBVHStack");
      memcpy(iter->stack, iter->stackfixed, sizeof(PBVHStack) * iter->stacksize);
    }
  }

  iter->stack[iter->stacksize].node = node;
  iter->stack[iter->stacksize].revisiting = revisiting;
  iter->stacksize++;
}

static PBVHNode *pbvh_iter_next(PBVHIter *iter)
{
  /* purpose here is to traverse tree, visiting child nodes before their
   * parents, this order is necessary for e.g. computing bounding boxes */

  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    PBVHNode *node = iter->stack[iter->stacksize].node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == NULL) {
      return NULL;
    }

    bool revisiting = iter->stack[iter->stacksize].revisiting;

    /* revisiting node already checked */
    if (revisiting) {
      return node;
    }

    if (iter->scb && !iter->scb(node, iter->search_data)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag & PBVH_Leaf) {
      /* immediately hit leaf node */
      return node;
    }
    else {
      /* come back later when children are done */
      pbvh_stack_push(iter, node, true);

      /* push two child nodes on the stack */
      pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset + 1, false);
      pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset, false);
    }
  }

  return NULL;
}

static PBVHNode *pbvh_iter_next_occluded(PBVHIter *iter)
{
  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    PBVHNode *node = iter->stack[iter->stacksize].node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == NULL) {
      return NULL;
    }

    if (iter->scb && !iter->scb(node, iter->search_data)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag & PBVH_Leaf) {
      /* immediately hit leaf node */
      return node;
    }
    else {
      pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset + 1, false);
      pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset, false);
    }
  }

  return NULL;
}

void BKE_pbvh_search_gather(
    PBVH *bvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***r_array, int *r_tot)
{
  PBVHIter iter;
  PBVHNode **array = NULL, *node;
  int tot = 0, space = 0;

  pbvh_iter_begin(&iter, bvh, scb, search_data);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_Leaf) {
      if (UNLIKELY(tot == space)) {
        /* resize array if needed */
        space = (tot == 0) ? 32 : space * 2;
        array = MEM_recallocN_id(array, sizeof(PBVHNode *) * space, __func__);
      }

      array[tot] = node;
      tot++;
    }
  }

  pbvh_iter_end(&iter);

  if (tot == 0 && array) {
    MEM_freeN(array);
    array = NULL;
  }

  *r_array = array;
  *r_tot = tot;
}

void BKE_pbvh_search_callback(PBVH *bvh,
                              BKE_pbvh_SearchCallback scb,
                              void *search_data,
                              BKE_pbvh_HitCallback hcb,
                              void *hit_data)
{
  PBVHIter iter;
  PBVHNode *node;

  pbvh_iter_begin(&iter, bvh, scb, search_data);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_Leaf) {
      hcb(node, hit_data);
    }
  }

  pbvh_iter_end(&iter);
}

typedef struct node_tree {
  PBVHNode *data;

  struct node_tree *left;
  struct node_tree *right;
} node_tree;

static void node_tree_insert(node_tree *tree, node_tree *new_node)
{
  if (new_node->data->tmin < tree->data->tmin) {
    if (tree->left) {
      node_tree_insert(tree->left, new_node);
    }
    else {
      tree->left = new_node;
    }
  }
  else {
    if (tree->right) {
      node_tree_insert(tree->right, new_node);
    }
    else {
      tree->right = new_node;
    }
  }
}

static void traverse_tree(node_tree *tree,
                          BKE_pbvh_HitOccludedCallback hcb,
                          void *hit_data,
                          float *tmin)
{
  if (tree->left) {
    traverse_tree(tree->left, hcb, hit_data, tmin);
  }

  hcb(tree->data, hit_data, tmin);

  if (tree->right) {
    traverse_tree(tree->right, hcb, hit_data, tmin);
  }
}

static void free_tree(node_tree *tree)
{
  if (tree->left) {
    free_tree(tree->left);
    tree->left = NULL;
  }

  if (tree->right) {
    free_tree(tree->right);
    tree->right = NULL;
  }

  free(tree);
}

float BKE_pbvh_node_get_tmin(PBVHNode *node)
{
  return node->tmin;
}

static void BKE_pbvh_search_callback_occluded(PBVH *bvh,
                                              BKE_pbvh_SearchCallback scb,
                                              void *search_data,
                                              BKE_pbvh_HitOccludedCallback hcb,
                                              void *hit_data)
{
  PBVHIter iter;
  PBVHNode *node;
  node_tree *tree = NULL;

  pbvh_iter_begin(&iter, bvh, scb, search_data);

  while ((node = pbvh_iter_next_occluded(&iter))) {
    if (node->flag & PBVH_Leaf) {
      node_tree *new_node = malloc(sizeof(node_tree));

      new_node->data = node;

      new_node->left = NULL;
      new_node->right = NULL;

      if (tree) {
        node_tree_insert(tree, new_node);
      }
      else {
        tree = new_node;
      }
    }
  }

  pbvh_iter_end(&iter);

  if (tree) {
    float tmin = FLT_MAX;
    traverse_tree(tree, hcb, hit_data, &tmin);
    free_tree(tree);
  }
}

static bool update_search_cb(PBVHNode *node, void *data_v)
{
  int flag = POINTER_AS_INT(data_v);

  if (node->flag & PBVH_Leaf) {
    return (node->flag & flag) != 0;
  }

  return true;
}

typedef struct PBVHUpdateData {
  PBVH *bvh;
  PBVHNode **nodes;
  int totnode;

  float (*vnors)[3];
  int flag;
} PBVHUpdateData;

static void pbvh_update_normals_accum_task_cb(void *__restrict userdata,
                                              const int n,
                                              const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;

  PBVH *bvh = data->bvh;
  PBVHNode *node = data->nodes[n];
  float(*vnors)[3] = data->vnors;

  if ((node->flag & PBVH_UpdateNormals)) {
    unsigned int mpoly_prev = UINT_MAX;
    float fn[3];

    const int *faces = node->prim_indices;
    const int totface = node->totprim;

    for (int i = 0; i < totface; ++i) {
      const MLoopTri *lt = &bvh->looptri[faces[i]];
      const unsigned int vtri[3] = {
          bvh->mloop[lt->tri[0]].v,
          bvh->mloop[lt->tri[1]].v,
          bvh->mloop[lt->tri[2]].v,
      };
      const int sides = 3;

      /* Face normal and mask */
      if (lt->poly != mpoly_prev) {
        const MPoly *mp = &bvh->mpoly[lt->poly];
        BKE_mesh_calc_poly_normal(mp, &bvh->mloop[mp->loopstart], bvh->verts, fn);
        mpoly_prev = lt->poly;
      }

      for (int j = sides; j--;) {
        const int v = vtri[j];

        if (bvh->verts[v].flag & ME_VERT_PBVH_UPDATE) {
          /* Note: This avoids `lock, add_v3_v3, unlock`
           * and is five to ten times quicker than a spinlock.
           * Not exact equivalent though, since atomicity is only ensured for one component
           * of the vector at a time, but here it shall not make any sensible difference. */
          for (int k = 3; k--;) {
            atomic_add_and_fetch_fl(&vnors[v][k], fn[k]);
          }
        }
      }
    }
  }
}

static void pbvh_update_normals_store_task_cb(void *__restrict userdata,
                                              const int n,
                                              const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;
  PBVH *bvh = data->bvh;
  PBVHNode *node = data->nodes[n];
  float(*vnors)[3] = data->vnors;

  if (node->flag & PBVH_UpdateNormals) {
    const int *verts = node->vert_indices;
    const int totvert = node->uniq_verts;

    for (int i = 0; i < totvert; ++i) {
      const int v = verts[i];
      MVert *mvert = &bvh->verts[v];

      /* mvert is shared between nodes, hence between threads. */
      if (atomic_fetch_and_and_char(&mvert->flag, (char)~ME_VERT_PBVH_UPDATE) &
          ME_VERT_PBVH_UPDATE) {
        normalize_v3(vnors[v]);
        normal_float_to_short_v3(mvert->no, vnors[v]);
      }
    }

    node->flag &= ~PBVH_UpdateNormals;
  }
}

static void pbvh_faces_update_normals(PBVH *bvh, PBVHNode **nodes, int totnode)
{
  /* could be per node to save some memory, but also means
   * we have to store for each vertex which node it is in */
  float(*vnors)[3] = MEM_callocN(sizeof(*vnors) * bvh->totvert, __func__);

  /* subtle assumptions:
   * - We know that for all edited vertices, the nodes with faces
   *   adjacent to these vertices have been marked with PBVH_UpdateNormals.
   *   This is true because if the vertex is inside the brush radius, the
   *   bounding box of it's adjacent faces will be as well.
   * - However this is only true for the vertices that have actually been
   *   edited, not for all vertices in the nodes marked for update, so we
   *   can only update vertices marked with ME_VERT_PBVH_UPDATE.
   */

  PBVHUpdateData data = {
      .bvh = bvh,
      .nodes = nodes,
      .vnors = vnors,
  };

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (totnode > PBVH_THREADED_LIMIT);

  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_accum_task_cb, &settings);

  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_store_task_cb, &settings);

  MEM_freeN(vnors);
}

static void pbvh_update_BB_redraw_task_cb(void *__restrict userdata,
                                          const int n,
                                          const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;
  PBVH *bvh = data->bvh;
  PBVHNode *node = data->nodes[n];
  const int flag = data->flag;

  if ((flag & PBVH_UpdateBB) && (node->flag & PBVH_UpdateBB)) {
    /* don't clear flag yet, leave it for flushing later */
    /* Note that bvh usage is read-only here, so no need to thread-protect it. */
    update_node_vb(bvh, node);
  }

  if ((flag & PBVH_UpdateOriginalBB) && (node->flag & PBVH_UpdateOriginalBB)) {
    node->orig_vb = node->vb;
  }

  if ((flag & PBVH_UpdateRedraw) && (node->flag & PBVH_UpdateRedraw)) {
    node->flag &= ~PBVH_UpdateRedraw;
  }
}

void pbvh_update_BB_redraw(PBVH *bvh, PBVHNode **nodes, int totnode, int flag)
{
  /* update BB, redraw flag */
  PBVHUpdateData data = {
      .bvh = bvh,
      .nodes = nodes,
      .flag = flag,
  };

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (totnode > PBVH_THREADED_LIMIT);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_BB_redraw_task_cb, &settings);
}

static int pbvh_get_buffers_update_flags(PBVH *bvh, bool show_vcol)
{
  int update_flags = 0;
  update_flags |= bvh->show_mask ? GPU_PBVH_BUFFERS_SHOW_MASK : 0;
  update_flags |= show_vcol ? GPU_PBVH_BUFFERS_SHOW_VCOL : 0;
  return update_flags;
}

static void pbvh_update_draw_buffers(PBVH *bvh, PBVHNode **nodes, int totnode, bool show_vcol)
{
  /* can't be done in parallel with OpenGL */
  for (int n = 0; n < totnode; n++) {
    PBVHNode *node = nodes[n];

    if (node->flag & PBVH_RebuildDrawBuffers) {
      GPU_pbvh_buffers_free(node->draw_buffers);
      switch (bvh->type) {
        case PBVH_GRIDS:
          node->draw_buffers = GPU_pbvh_grid_buffers_build(node->totprim, bvh->grid_hidden);
          break;
        case PBVH_FACES:
          node->draw_buffers = GPU_pbvh_mesh_buffers_build(node->face_vert_indices,
                                                           bvh->mpoly,
                                                           bvh->mloop,
                                                           bvh->looptri,
                                                           bvh->verts,
                                                           node->prim_indices,
                                                           node->totprim);
          break;
        case PBVH_BMESH:
          node->draw_buffers = GPU_pbvh_bmesh_buffers_build(bvh->flags &
                                                            PBVH_DYNTOPO_SMOOTH_SHADING);
          break;
      }

      node->flag &= ~PBVH_RebuildDrawBuffers;
    }

    if (node->flag & PBVH_UpdateDrawBuffers) {
      const int update_flags = pbvh_get_buffers_update_flags(bvh, show_vcol);
      switch (bvh->type) {
        case PBVH_GRIDS:
          GPU_pbvh_grid_buffers_update(node->draw_buffers,
                                       bvh->grids,
                                       bvh->grid_flag_mats,
                                       node->prim_indices,
                                       node->totprim,
                                       &bvh->gridkey,
                                       update_flags);
          break;
        case PBVH_FACES:
          GPU_pbvh_mesh_buffers_update(node->draw_buffers,
                                       bvh->verts,
                                       node->vert_indices,
                                       node->uniq_verts + node->face_verts,
                                       CustomData_get_layer(bvh->vdata, CD_PAINT_MASK),
                                       CustomData_get_layer(bvh->ldata, CD_MLOOPCOL),
                                       node->face_vert_indices,
                                       update_flags);
          break;
        case PBVH_BMESH:
          GPU_pbvh_bmesh_buffers_update(node->draw_buffers,
                                        bvh->bm,
                                        node->bm_faces,
                                        node->bm_unique_verts,
                                        node->bm_other_verts,
                                        update_flags);
          break;
      }

      node->flag &= ~PBVH_UpdateDrawBuffers;
    }
  }
}

static int pbvh_flush_bb(PBVH *bvh, PBVHNode *node, int flag)
{
  int update = 0;

  /* difficult to multithread well, we just do single threaded recursive */
  if (node->flag & PBVH_Leaf) {
    if (flag & PBVH_UpdateBB) {
      update |= (node->flag & PBVH_UpdateBB);
      node->flag &= ~PBVH_UpdateBB;
    }

    if (flag & PBVH_UpdateOriginalBB) {
      update |= (node->flag & PBVH_UpdateOriginalBB);
      node->flag &= ~PBVH_UpdateOriginalBB;
    }

    return update;
  }
  else {
    update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset, flag);
    update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset + 1, flag);

    if (update & PBVH_UpdateBB) {
      update_node_vb(bvh, node);
    }
    if (update & PBVH_UpdateOriginalBB) {
      node->orig_vb = node->vb;
    }
  }

  return update;
}

void BKE_pbvh_update_bounds(PBVH *bvh, int flag)
{
  if (!bvh->nodes) {
    return;
  }

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(bvh, update_search_cb, POINTER_FROM_INT(flag), &nodes, &totnode);

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw)) {
    pbvh_update_BB_redraw(bvh, nodes, totnode, flag);
  }

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB)) {
    pbvh_flush_bb(bvh, bvh->nodes, flag);
  }

  if (nodes) {
    MEM_freeN(nodes);
  }
}

void BKE_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3])
{
  PBVHIter iter;
  PBVHNode *node;
  BB bb;

  BB_reset(&bb);

  pbvh_iter_begin(&iter, bvh, NULL, NULL);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_UpdateRedraw) {
      BB_expand_with_bb(&bb, &node->vb);
    }
  }

  pbvh_iter_end(&iter);

  copy_v3_v3(bb_min, bb.bmin);
  copy_v3_v3(bb_max, bb.bmax);
}

void BKE_pbvh_get_grid_updates(PBVH *bvh, bool clear, void ***r_gridfaces, int *r_totface)
{
  GSet *face_set = BLI_gset_ptr_new(__func__);
  PBVHNode *node;
  PBVHIter iter;

  pbvh_iter_begin(&iter, bvh, NULL, NULL);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_UpdateNormals) {
      for (unsigned i = 0; i < node->totprim; ++i) {
        void *face = bvh->gridfaces[node->prim_indices[i]];
        BLI_gset_add(face_set, face);
      }

      if (clear) {
        node->flag &= ~PBVH_UpdateNormals;
      }
    }
  }

  pbvh_iter_end(&iter);

  const int tot = BLI_gset_len(face_set);
  if (tot == 0) {
    *r_totface = 0;
    *r_gridfaces = NULL;
    BLI_gset_free(face_set, NULL);
    return;
  }

  void **faces = MEM_mallocN(sizeof(*faces) * tot, "PBVH Grid Faces");

  GSetIterator gs_iter;
  int i;
  GSET_ITER_INDEX (gs_iter, face_set, i) {
    faces[i] = BLI_gsetIterator_getKey(&gs_iter);
  }

  BLI_gset_free(face_set, NULL);

  *r_totface = tot;
  *r_gridfaces = faces;
}

/***************************** PBVH Access ***********************************/

PBVHType BKE_pbvh_type(const PBVH *bvh)
{
  return bvh->type;
}

bool BKE_pbvh_has_faces(const PBVH *bvh)
{
  if (bvh->type == PBVH_BMESH) {
    return (bvh->bm->totface != 0);
  }
  else {
    return (bvh->totprim != 0);
  }
}

void BKE_pbvh_bounding_box(const PBVH *bvh, float min[3], float max[3])
{
  if (bvh->totnode) {
    const BB *bb = &bvh->nodes[0].vb;
    copy_v3_v3(min, bb->bmin);
    copy_v3_v3(max, bb->bmax);
  }
  else {
    zero_v3(min);
    zero_v3(max);
  }
}

BLI_bitmap **BKE_pbvh_grid_hidden(const PBVH *bvh)
{
  BLI_assert(bvh->type == PBVH_GRIDS);
  return bvh->grid_hidden;
}

void BKE_pbvh_get_grid_key(const PBVH *bvh, CCGKey *key)
{
  BLI_assert(bvh->type == PBVH_GRIDS);
  *key = bvh->gridkey;
}

struct CCGElem **BKE_pbvh_get_grids(const PBVH *bvh, int *num_grids)
{
  BLI_assert(bvh->type == PBVH_GRIDS);
  *num_grids = bvh->totgrid;
  return bvh->grids;
}

BMesh *BKE_pbvh_get_bmesh(PBVH *bvh)
{
  BLI_assert(bvh->type == PBVH_BMESH);
  return bvh->bm;
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node)
{
  node->flag |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_redraw(PBVHNode *node)
{
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_normals_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals;
}

void BKE_pbvh_node_fully_hidden_set(PBVHNode *node, int fully_hidden)
{
  BLI_assert(node->flag & PBVH_Leaf);

  if (fully_hidden) {
    node->flag |= PBVH_FullyHidden;
  }
  else {
    node->flag &= ~PBVH_FullyHidden;
  }
}

void BKE_pbvh_node_get_verts(PBVH *bvh,
                             PBVHNode *node,
                             const int **r_vert_indices,
                             MVert **r_verts)
{
  if (r_vert_indices) {
    *r_vert_indices = node->vert_indices;
  }

  if (r_verts) {
    *r_verts = bvh->verts;
  }
}

void BKE_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node, int *r_uniquevert, int *r_totvert)
{
  int tot;

  switch (bvh->type) {
    case PBVH_GRIDS:
      tot = node->totprim * bvh->gridkey.grid_area;
      if (r_totvert) {
        *r_totvert = tot;
      }
      if (r_uniquevert) {
        *r_uniquevert = tot;
      }
      break;
    case PBVH_FACES:
      if (r_totvert) {
        *r_totvert = node->uniq_verts + node->face_verts;
      }
      if (r_uniquevert) {
        *r_uniquevert = node->uniq_verts;
      }
      break;
    case PBVH_BMESH:
      tot = BLI_gset_len(node->bm_unique_verts);
      if (r_totvert) {
        *r_totvert = tot + BLI_gset_len(node->bm_other_verts);
      }
      if (r_uniquevert) {
        *r_uniquevert = tot;
      }
      break;
  }
}

void BKE_pbvh_node_get_grids(PBVH *bvh,
                             PBVHNode *node,
                             int **r_grid_indices,
                             int *r_totgrid,
                             int *r_maxgrid,
                             int *r_gridsize,
                             CCGElem ***r_griddata)
{
  switch (bvh->type) {
    case PBVH_GRIDS:
      if (r_grid_indices) {
        *r_grid_indices = node->prim_indices;
      }
      if (r_totgrid) {
        *r_totgrid = node->totprim;
      }
      if (r_maxgrid) {
        *r_maxgrid = bvh->totgrid;
      }
      if (r_gridsize) {
        *r_gridsize = bvh->gridkey.grid_size;
      }
      if (r_griddata) {
        *r_griddata = bvh->grids;
      }
      break;
    case PBVH_FACES:
    case PBVH_BMESH:
      if (r_grid_indices) {
        *r_grid_indices = NULL;
      }
      if (r_totgrid) {
        *r_totgrid = 0;
      }
      if (r_maxgrid) {
        *r_maxgrid = 0;
      }
      if (r_gridsize) {
        *r_gridsize = 0;
      }
      if (r_griddata) {
        *r_griddata = NULL;
      }
      break;
  }
}

void BKE_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
  copy_v3_v3(bb_min, node->vb.bmin);
  copy_v3_v3(bb_max, node->vb.bmax);
}

void BKE_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
  copy_v3_v3(bb_min, node->orig_vb.bmin);
  copy_v3_v3(bb_max, node->orig_vb.bmax);
}

void BKE_pbvh_node_get_proxies(PBVHNode *node, PBVHProxyNode **proxies, int *proxy_count)
{
  if (node->proxy_count > 0) {
    if (proxies) {
      *proxies = node->proxies;
    }
    if (proxy_count) {
      *proxy_count = node->proxy_count;
    }
  }
  else {
    if (proxies) {
      *proxies = NULL;
    }
    if (proxy_count) {
      *proxy_count = 0;
    }
  }
}

void BKE_pbvh_node_get_bm_orco_data(PBVHNode *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3])
{
  *r_orco_tris = node->bm_ortri;
  *r_orco_tris_num = node->bm_tot_ortri;
  *r_orco_coords = node->bm_orco;
}

/**
 * \note doing a full search on all vertices here seems expensive,
 * however this is important to avoid having to recalculate boundbox & sync the buffers to the GPU
 * (which is far more expensive!) See: T47232.
 */
bool BKE_pbvh_node_vert_update_check_any(PBVH *bvh, PBVHNode *node)
{
  BLI_assert(bvh->type == PBVH_FACES);
  const int *verts = node->vert_indices;
  const int totvert = node->uniq_verts + node->face_verts;

  for (int i = 0; i < totvert; ++i) {
    const int v = verts[i];
    const MVert *mvert = &bvh->verts[v];

    if (mvert->flag & ME_VERT_PBVH_UPDATE) {
      return true;
    }
  }

  return false;
}

/********************************* Raycast ***********************************/

typedef struct {
  struct IsectRayAABB_Precalc ray;
  bool original;
} RaycastData;

static bool ray_aabb_intersect(PBVHNode *node, void *data_v)
{
  RaycastData *rcd = data_v;
  const float *bb_min, *bb_max;

  if (rcd->original) {
    /* BKE_pbvh_node_get_original_BB */
    bb_min = node->orig_vb.bmin;
    bb_max = node->orig_vb.bmax;
  }
  else {
    /* BKE_pbvh_node_get_BB */
    bb_min = node->vb.bmin;
    bb_max = node->vb.bmax;
  }

  return isect_ray_aabb_v3(&rcd->ray, bb_min, bb_max, &node->tmin);
}

void BKE_pbvh_raycast(PBVH *bvh,
                      BKE_pbvh_HitOccludedCallback cb,
                      void *data,
                      const float ray_start[3],
                      const float ray_normal[3],
                      bool original)
{
  RaycastData rcd;

  isect_ray_aabb_v3_precalc(&rcd.ray, ray_start, ray_normal);
  rcd.original = original;

  BKE_pbvh_search_callback_occluded(bvh, ray_aabb_intersect, &rcd, cb, data);
}

bool ray_face_intersection_quad(const float ray_start[3],
                                const float ray_normal[3],
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth)
{
  float depth_test;

  if ((isect_ray_tri_epsilon_v3(ray_start, ray_normal, t0, t1, t2, &depth_test, NULL, 0.1f) &&
       (depth_test < *depth)) ||
      (isect_ray_tri_epsilon_v3(ray_start, ray_normal, t0, t2, t3, &depth_test, NULL, 0.1f) &&
       (depth_test < *depth))) {
    *depth = depth_test;
    return true;
  }
  else {
    return false;
  }
}

bool ray_face_intersection_tri(const float ray_start[3],
                               const float ray_normal[3],
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth)
{
  float depth_test;

  if ((isect_ray_tri_epsilon_v3(ray_start, ray_normal, t0, t1, t2, &depth_test, NULL, 0.1f) &&
       (depth_test < *depth))) {
    *depth = depth_test;
    return true;
  }
  else {
    return false;
  }
}

/* Take advantage of the fact we know this wont be an intersection.
 * Just handle ray-tri edges. */
static float dist_squared_ray_to_tri_v3_fast(const float ray_origin[3],
                                             const float ray_direction[3],
                                             const float v0[3],
                                             const float v1[3],
                                             const float v2[3],
                                             float r_point[3],
                                             float *r_depth)
{
  const float *tri[3] = {v0, v1, v2};
  float dist_sq_best = FLT_MAX;
  for (int i = 0, j = 2; i < 3; j = i++) {
    float point_test[3], depth_test = FLT_MAX;
    const float dist_sq_test = dist_squared_ray_to_seg_v3(
        ray_origin, ray_direction, tri[i], tri[j], point_test, &depth_test);
    if (dist_sq_test < dist_sq_best || i == 0) {
      copy_v3_v3(r_point, point_test);
      *r_depth = depth_test;
      dist_sq_best = dist_sq_test;
    }
  }
  return dist_sq_best;
}

bool ray_face_nearest_quad(const float ray_start[3],
                           const float ray_normal[3],
                           const float t0[3],
                           const float t1[3],
                           const float t2[3],
                           const float t3[3],
                           float *depth,
                           float *dist_sq)
{
  float dist_sq_test;
  float co[3], depth_test;

  if (((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
            ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)) {
    *dist_sq = dist_sq_test;
    *depth = depth_test;
    if (((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
              ray_start, ray_normal, t0, t2, t3, co, &depth_test)) < *dist_sq)) {
      *dist_sq = dist_sq_test;
      *depth = depth_test;
    }
    return true;
  }
  else {
    return false;
  }
}

bool ray_face_nearest_tri(const float ray_start[3],
                          const float ray_normal[3],
                          const float t0[3],
                          const float t1[3],
                          const float t2[3],
                          float *depth,
                          float *dist_sq)
{
  float dist_sq_test;
  float co[3], depth_test;

  if (((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
            ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)) {
    *dist_sq = dist_sq_test;
    *depth = depth_test;
    return true;
  }
  else {
    return false;
  }
}

static bool pbvh_faces_node_raycast(PBVH *bvh,
                                    const PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth)
{
  const MVert *vert = bvh->verts;
  const MLoop *mloop = bvh->mloop;
  const int *faces = node->prim_indices;
  int i, totface = node->totprim;
  bool hit = false;

  for (i = 0; i < totface; ++i) {
    const MLoopTri *lt = &bvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (paint_is_face_hidden(lt, vert, mloop)) {
      continue;
    }

    if (origco) {
      /* intersect with backuped original coordinates */
      hit |= ray_face_intersection_tri(ray_start,
                                       ray_normal,
                                       origco[face_verts[0]],
                                       origco[face_verts[1]],
                                       origco[face_verts[2]],
                                       depth);
    }
    else {
      /* intersect with current coordinates */
      hit |= ray_face_intersection_tri(ray_start,
                                       ray_normal,
                                       vert[mloop[lt->tri[0]].v].co,
                                       vert[mloop[lt->tri[1]].v].co,
                                       vert[mloop[lt->tri[2]].v].co,
                                       depth);
    }
  }

  return hit;
}

static bool pbvh_grids_node_raycast(PBVH *bvh,
                                    PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth)
{
  const int totgrid = node->totprim;
  const int gridsize = bvh->gridkey.grid_size;
  bool hit = false;

  for (int i = 0; i < totgrid; ++i) {
    CCGElem *grid = bvh->grids[node->prim_indices[i]];
    BLI_bitmap *gh;

    if (!grid) {
      continue;
    }

    gh = bvh->grid_hidden[node->prim_indices[i]];

    for (int y = 0; y < gridsize - 1; ++y) {
      for (int x = 0; x < gridsize - 1; ++x) {
        /* check if grid face is hidden */
        if (gh) {
          if (paint_is_grid_face_hidden(gh, gridsize, x, y)) {
            continue;
          }
        }

        if (origco) {
          hit |= ray_face_intersection_quad(ray_start,
                                            ray_normal,
                                            origco[y * gridsize + x],
                                            origco[y * gridsize + x + 1],
                                            origco[(y + 1) * gridsize + x + 1],
                                            origco[(y + 1) * gridsize + x],
                                            depth);
        }
        else {
          hit |= ray_face_intersection_quad(ray_start,
                                            ray_normal,
                                            CCG_grid_elem_co(&bvh->gridkey, grid, x, y),
                                            CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y),
                                            CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y + 1),
                                            CCG_grid_elem_co(&bvh->gridkey, grid, x, y + 1),
                                            depth);
        }
      }
    }

    if (origco) {
      origco += gridsize * gridsize;
    }
  }

  return hit;
}

bool BKE_pbvh_node_raycast(PBVH *bvh,
                           PBVHNode *node,
                           float (*origco)[3],
                           bool use_origco,
                           const float ray_start[3],
                           const float ray_normal[3],
                           float *depth)
{
  bool hit = false;

  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  switch (bvh->type) {
    case PBVH_FACES:
      hit |= pbvh_faces_node_raycast(bvh, node, origco, ray_start, ray_normal, depth);
      break;
    case PBVH_GRIDS:
      hit |= pbvh_grids_node_raycast(bvh, node, origco, ray_start, ray_normal, depth);
      break;
    case PBVH_BMESH:
      hit = pbvh_bmesh_node_raycast(node, ray_start, ray_normal, depth, use_origco);
      break;
  }

  return hit;
}

void BKE_pbvh_raycast_project_ray_root(
    PBVH *bvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (bvh->nodes) {
    float rootmin_start, rootmin_end;
    float bb_min_root[3], bb_max_root[3], bb_center[3], bb_diff[3];
    struct IsectRayAABB_Precalc ray;
    float ray_normal_inv[3];
    float offset = 1.0f + 1e-3f;
    float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

    if (original) {
      BKE_pbvh_node_get_original_BB(bvh->nodes, bb_min_root, bb_max_root);
    }
    else {
      BKE_pbvh_node_get_BB(bvh->nodes, bb_min_root, bb_max_root);
    }

    /* Slightly offset min and max in case we have a zero width node
     * (due to a plane mesh for instance), or faces very close to the bounding box boundary. */
    mid_v3_v3v3(bb_center, bb_max_root, bb_min_root);
    /* diff should be same for both min/max since it's calculated from center */
    sub_v3_v3v3(bb_diff, bb_max_root, bb_center);
    /* handles case of zero width bb */
    add_v3_v3(bb_diff, offset_vec);
    madd_v3_v3v3fl(bb_max_root, bb_center, bb_diff, offset);
    madd_v3_v3v3fl(bb_min_root, bb_center, bb_diff, -offset);

    /* first project start ray */
    isect_ray_aabb_v3_precalc(&ray, ray_start, ray_normal);
    if (!isect_ray_aabb_v3(&ray, bb_min_root, bb_max_root, &rootmin_start)) {
      return;
    }

    /* then the end ray */
    mul_v3_v3fl(ray_normal_inv, ray_normal, -1.0);
    isect_ray_aabb_v3_precalc(&ray, ray_end, ray_normal_inv);
    /* unlikely to fail exiting if entering succeeded, still keep this here */
    if (!isect_ray_aabb_v3(&ray, bb_min_root, bb_max_root, &rootmin_end)) {
      return;
    }

    madd_v3_v3v3fl(ray_start, ray_start, ray_normal, rootmin_start);
    madd_v3_v3v3fl(ray_end, ray_end, ray_normal_inv, rootmin_end);
  }
}

/* -------------------------------------------------------------------- */

typedef struct {
  struct DistRayAABB_Precalc dist_ray_to_aabb_precalc;
  bool original;
} FindNearestRayData;

static bool nearest_to_ray_aabb_dist_sq(PBVHNode *node, void *data_v)
{
  FindNearestRayData *rcd = data_v;
  const float *bb_min, *bb_max;

  if (rcd->original) {
    /* BKE_pbvh_node_get_original_BB */
    bb_min = node->orig_vb.bmin;
    bb_max = node->orig_vb.bmax;
  }
  else {
    /* BKE_pbvh_node_get_BB */
    bb_min = node->vb.bmin;
    bb_max = node->vb.bmax;
  }

  float co_dummy[3], depth;
  node->tmin = dist_squared_ray_to_aabb_v3(
      &rcd->dist_ray_to_aabb_precalc, bb_min, bb_max, co_dummy, &depth);
  /* Ideally we would skip distances outside the range. */
  return depth > 0.0f;
}

void BKE_pbvh_find_nearest_to_ray(PBVH *bvh,
                                  BKE_pbvh_SearchNearestCallback cb,
                                  void *data,
                                  const float ray_start[3],
                                  const float ray_normal[3],
                                  bool original)
{
  FindNearestRayData ncd;

  dist_squared_ray_to_aabb_v3_precalc(&ncd.dist_ray_to_aabb_precalc, ray_start, ray_normal);
  ncd.original = original;

  BKE_pbvh_search_callback_occluded(bvh, nearest_to_ray_aabb_dist_sq, &ncd, cb, data);
}

static bool pbvh_faces_node_nearest_to_ray(PBVH *bvh,
                                           const PBVHNode *node,
                                           float (*origco)[3],
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  const MVert *vert = bvh->verts;
  const MLoop *mloop = bvh->mloop;
  const int *faces = node->prim_indices;
  int i, totface = node->totprim;
  bool hit = false;

  for (i = 0; i < totface; ++i) {
    const MLoopTri *lt = &bvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (paint_is_face_hidden(lt, vert, mloop)) {
      continue;
    }

    if (origco) {
      /* intersect with backuped original coordinates */
      hit |= ray_face_nearest_tri(ray_start,
                                  ray_normal,
                                  origco[face_verts[0]],
                                  origco[face_verts[1]],
                                  origco[face_verts[2]],
                                  depth,
                                  dist_sq);
    }
    else {
      /* intersect with current coordinates */
      hit |= ray_face_nearest_tri(ray_start,
                                  ray_normal,
                                  vert[mloop[lt->tri[0]].v].co,
                                  vert[mloop[lt->tri[1]].v].co,
                                  vert[mloop[lt->tri[2]].v].co,
                                  depth,
                                  dist_sq);
    }
  }

  return hit;
}

static bool pbvh_grids_node_nearest_to_ray(PBVH *bvh,
                                           PBVHNode *node,
                                           float (*origco)[3],
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  const int totgrid = node->totprim;
  const int gridsize = bvh->gridkey.grid_size;
  bool hit = false;

  for (int i = 0; i < totgrid; ++i) {
    CCGElem *grid = bvh->grids[node->prim_indices[i]];
    BLI_bitmap *gh;

    if (!grid) {
      continue;
    }

    gh = bvh->grid_hidden[node->prim_indices[i]];

    for (int y = 0; y < gridsize - 1; ++y) {
      for (int x = 0; x < gridsize - 1; ++x) {
        /* check if grid face is hidden */
        if (gh) {
          if (paint_is_grid_face_hidden(gh, gridsize, x, y)) {
            continue;
          }
        }

        if (origco) {
          hit |= ray_face_nearest_quad(ray_start,
                                       ray_normal,
                                       origco[y * gridsize + x],
                                       origco[y * gridsize + x + 1],
                                       origco[(y + 1) * gridsize + x + 1],
                                       origco[(y + 1) * gridsize + x],
                                       depth,
                                       dist_sq);
        }
        else {
          hit |= ray_face_nearest_quad(ray_start,
                                       ray_normal,
                                       CCG_grid_elem_co(&bvh->gridkey, grid, x, y),
                                       CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y),
                                       CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y + 1),
                                       CCG_grid_elem_co(&bvh->gridkey, grid, x, y + 1),
                                       depth,
                                       dist_sq);
        }
      }
    }

    if (origco) {
      origco += gridsize * gridsize;
    }
  }

  return hit;
}

bool BKE_pbvh_node_find_nearest_to_ray(PBVH *bvh,
                                       PBVHNode *node,
                                       float (*origco)[3],
                                       bool use_origco,
                                       const float ray_start[3],
                                       const float ray_normal[3],
                                       float *depth,
                                       float *dist_sq)
{
  bool hit = false;

  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  switch (bvh->type) {
    case PBVH_FACES:
      hit |= pbvh_faces_node_nearest_to_ray(
          bvh, node, origco, ray_start, ray_normal, depth, dist_sq);
      break;
    case PBVH_GRIDS:
      hit |= pbvh_grids_node_nearest_to_ray(
          bvh, node, origco, ray_start, ray_normal, depth, dist_sq);
      break;
    case PBVH_BMESH:
      hit = pbvh_bmesh_node_nearest_to_ray(
          node, ray_start, ray_normal, depth, dist_sq, use_origco);
      break;
  }

  return hit;
}

typedef enum {
  ISECT_INSIDE,
  ISECT_OUTSIDE,
  ISECT_INTERSECT,
} PlaneAABBIsect;

/* Adapted from:
 * http://www.gamedev.net/community/forums/topic.asp?topic_id=512123
 * Returns true if the AABB is at least partially within the frustum
 * (ok, not a real frustum), false otherwise.
 */
static PlaneAABBIsect test_planes_aabb(const float bb_min[3],
                                       const float bb_max[3],
                                       const float (*planes)[4])
{
  float vmin[3], vmax[3];
  PlaneAABBIsect ret = ISECT_INSIDE;

  for (int i = 0; i < 4; ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      if (planes[i][axis] > 0) {
        vmin[axis] = bb_min[axis];
        vmax[axis] = bb_max[axis];
      }
      else {
        vmin[axis] = bb_max[axis];
        vmax[axis] = bb_min[axis];
      }
    }

    if (dot_v3v3(planes[i], vmin) + planes[i][3] > 0) {
      return ISECT_OUTSIDE;
    }
    else if (dot_v3v3(planes[i], vmax) + planes[i][3] >= 0) {
      ret = ISECT_INTERSECT;
    }
  }

  return ret;
}

bool BKE_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.bmin;
  bb_max = node->vb.bmax;

  return test_planes_aabb(bb_min, bb_max, data) != ISECT_OUTSIDE;
}

bool BKE_pbvh_node_planes_exclude_AABB(PBVHNode *node, void *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.bmin;
  bb_max = node->vb.bmax;

  return test_planes_aabb(bb_min, bb_max, data) != ISECT_INSIDE;
}

typedef struct PBVHNodeDrawCallbackData {
  void (*draw_fn)(void *user_data, GPU_PBVH_Buffers *buffers);
  void *user_data;
} PBVHNodeDrawCallbackData;

static void pbvh_node_draw_cb(PBVHNode *node, void *data_v)
{
  PBVHNodeDrawCallbackData *data = data_v;

  if (!(node->flag & PBVH_FullyHidden)) {
    data->draw_fn(data->user_data, node->draw_buffers);
  }
}

void BKE_pbvh_update_normals(PBVH *bvh, struct SubdivCCG *subdiv_ccg)
{
  /* Update normals */
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(
      bvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateNormals), &nodes, &totnode);

  if (bvh->type == PBVH_BMESH) {
    pbvh_bmesh_normals_update(nodes, totnode);
  }
  else if (bvh->type == PBVH_FACES) {
    pbvh_faces_update_normals(bvh, nodes, totnode);
  }
  else if (bvh->type == PBVH_GRIDS) {
    struct CCGFace **faces;
    int num_faces;
    BKE_pbvh_get_grid_updates(bvh, true, (void ***)&faces, &num_faces);
    if (num_faces > 0) {
      BKE_subdiv_ccg_update_normals(subdiv_ccg, faces, num_faces);
      MEM_freeN(faces);
    }
  }

  if (nodes) {
    MEM_freeN(nodes);
  }
}

void BKE_pbvh_update_draw_buffers(PBVH *bvh, bool show_vcol)
{
  /* Update GPU buffers */
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(
      bvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateDrawBuffers), &nodes, &totnode);

  pbvh_update_draw_buffers(bvh, nodes, totnode, show_vcol);

  if (nodes) {
    MEM_freeN(nodes);
  }
}

/**
 * Version of #BKE_pbvh_draw that runs a callback.
 */
void BKE_pbvh_draw_cb(PBVH *bvh,
                      float (*planes)[4],
                      void (*draw_fn)(void *user_data, GPU_PBVH_Buffers *buffers),
                      void *user_data)
{
  PBVHNodeDrawCallbackData draw_data = {
      .draw_fn = draw_fn,
      .user_data = user_data,
  };

  if (planes) {
    BKE_pbvh_search_callback(
        bvh, BKE_pbvh_node_planes_contain_AABB, planes, pbvh_node_draw_cb, &draw_data);
  }
  else {
    BKE_pbvh_search_callback(bvh, NULL, NULL, pbvh_node_draw_cb, &draw_data);
  }
}

void BKE_pbvh_draw_debug_cb(
    PBVH *bvh,
    void (*draw_fn)(void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag),
    void *user_data)
{
  for (int a = 0; a < bvh->totnode; a++) {
    PBVHNode *node = &bvh->nodes[a];

    draw_fn(user_data, node->vb.bmin, node->vb.bmax, node->flag);
  }
}

void BKE_pbvh_grids_update(
    PBVH *bvh, CCGElem **grids, void **gridfaces, DMFlagMat *flagmats, BLI_bitmap **grid_hidden)
{
  bvh->grids = grids;
  bvh->gridfaces = gridfaces;

  if (flagmats != bvh->grid_flag_mats || bvh->grid_hidden != grid_hidden) {
    bvh->grid_flag_mats = flagmats;
    bvh->grid_hidden = grid_hidden;

    for (int a = 0; a < bvh->totnode; ++a) {
      BKE_pbvh_node_mark_rebuild_draw(&bvh->nodes[a]);
    }
  }
}

/* Get the node's displacement layer, creating it if necessary */
float *BKE_pbvh_node_layer_disp_get(PBVH *bvh, PBVHNode *node)
{
  if (!node->layer_disp) {
    int totvert = 0;
    BKE_pbvh_node_num_verts(bvh, node, &totvert, NULL);
    node->layer_disp = MEM_callocN(sizeof(float) * totvert, "layer disp");
  }
  return node->layer_disp;
}

/* If the node has a displacement layer, free it and set to null */
void BKE_pbvh_node_layer_disp_free(PBVHNode *node)
{
  if (node->layer_disp) {
    MEM_freeN(node->layer_disp);
    node->layer_disp = NULL;
  }
}

float (*BKE_pbvh_get_vertCos(PBVH *pbvh))[3]
{
  float(*vertCos)[3] = NULL;

  if (pbvh->verts) {
    MVert *mvert = pbvh->verts;

    vertCos = MEM_callocN(3 * pbvh->totvert * sizeof(float), "BKE_pbvh_get_vertCoords");
    float *co = (float *)vertCos;

    for (int a = 0; a < pbvh->totvert; a++, mvert++, co += 3) {
      copy_v3_v3(co, mvert->co);
    }
  }

  return vertCos;
}

void BKE_pbvh_apply_vertCos(PBVH *pbvh, float (*vertCos)[3], const int totvert)
{
  if (totvert != pbvh->totvert) {
    BLI_assert(!"PBVH: Given deforming vcos number does not natch PBVH vertex number!");
    return;
  }

  if (!pbvh->deformed) {
    if (pbvh->verts) {
      /* if pbvh is not already deformed, verts/faces points to the */
      /* original data and applying new coords to this arrays would lead to */
      /* unneeded deformation -- duplicate verts/faces to avoid this */

      pbvh->verts = MEM_dupallocN(pbvh->verts);
      /* No need to dupalloc pbvh->looptri, this one is 'totally owned' by pbvh,
       * it's never some mesh data. */

      pbvh->deformed = true;
    }
  }

  if (pbvh->verts) {
    MVert *mvert = pbvh->verts;
    /* copy new verts coords */
    for (int a = 0; a < pbvh->totvert; ++a, ++mvert) {
      /* no need for float comparison here (memory is exactly equal or not) */
      if (memcmp(mvert->co, vertCos[a], sizeof(float[3])) != 0) {
        copy_v3_v3(mvert->co, vertCos[a]);
        mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }

    /* coordinates are new -- normals should also be updated */
    BKE_mesh_calc_normals_looptri(
        pbvh->verts, pbvh->totvert, pbvh->mloop, pbvh->looptri, pbvh->totprim, NULL);

    for (int a = 0; a < pbvh->totnode; ++a) {
      BKE_pbvh_node_mark_update(&pbvh->nodes[a]);
    }

    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
  }
}

bool BKE_pbvh_isDeformed(PBVH *pbvh)
{
  return pbvh->deformed;
}
/* Proxies */

PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *bvh, PBVHNode *node)
{
  int index, totverts;

  index = node->proxy_count;

  node->proxy_count++;

  if (node->proxies) {
    node->proxies = MEM_reallocN(node->proxies, node->proxy_count * sizeof(PBVHProxyNode));
  }
  else {
    node->proxies = MEM_mallocN(sizeof(PBVHProxyNode), "PBVHNodeProxy");
  }

  BKE_pbvh_node_num_verts(bvh, node, &totverts, NULL);
  node->proxies[index].co = MEM_callocN(sizeof(float[3]) * totverts, "PBVHNodeProxy.co");

  return node->proxies + index;
}

void BKE_pbvh_node_free_proxies(PBVHNode *node)
{
  for (int p = 0; p < node->proxy_count; p++) {
    MEM_freeN(node->proxies[p].co);
    node->proxies[p].co = NULL;
  }

  MEM_freeN(node->proxies);
  node->proxies = NULL;

  node->proxy_count = 0;
}

void BKE_pbvh_gather_proxies(PBVH *pbvh, PBVHNode ***r_array, int *r_tot)
{
  PBVHNode **array = NULL;
  int tot = 0, space = 0;

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = pbvh->nodes + n;

    if (node->proxy_count > 0) {
      if (tot == space) {
        /* resize array if needed */
        space = (tot == 0) ? 32 : space * 2;
        array = MEM_recallocN_id(array, sizeof(PBVHNode *) * space, __func__);
      }

      array[tot] = node;
      tot++;
    }
  }

  if (tot == 0 && array) {
    MEM_freeN(array);
    array = NULL;
  }

  *r_array = array;
  *r_tot = tot;
}

void pbvh_vertex_iter_init(PBVH *bvh, PBVHNode *node, PBVHVertexIter *vi, int mode)
{
  struct CCGElem **grids;
  struct MVert *verts;
  const int *vert_indices;
  int *grid_indices;
  int totgrid, gridsize, uniq_verts, totvert;

  vi->grid = NULL;
  vi->no = NULL;
  vi->fno = NULL;
  vi->mvert = NULL;

  BKE_pbvh_node_get_grids(bvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids);
  BKE_pbvh_node_num_verts(bvh, node, &uniq_verts, &totvert);
  BKE_pbvh_node_get_verts(bvh, node, &vert_indices, &verts);
  vi->key = &bvh->gridkey;

  vi->grids = grids;
  vi->grid_indices = grid_indices;
  vi->totgrid = (grids) ? totgrid : 1;
  vi->gridsize = gridsize;

  if (mode == PBVH_ITER_ALL) {
    vi->totvert = totvert;
  }
  else {
    vi->totvert = uniq_verts;
  }
  vi->vert_indices = vert_indices;
  vi->mverts = verts;

  if (bvh->type == PBVH_BMESH) {
    BLI_gsetIterator_init(&vi->bm_unique_verts, node->bm_unique_verts);
    BLI_gsetIterator_init(&vi->bm_other_verts, node->bm_other_verts);
    vi->bm_vdata = &bvh->bm->vdata;
    vi->cd_vert_mask_offset = CustomData_get_offset(vi->bm_vdata, CD_PAINT_MASK);
  }

  vi->gh = NULL;
  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = bvh->grid_hidden;
  }

  vi->mask = NULL;
  if (bvh->type == PBVH_FACES) {
    vi->vmask = CustomData_get_layer(bvh->vdata, CD_PAINT_MASK);
  }
}

bool pbvh_has_mask(PBVH *bvh)
{
  switch (bvh->type) {
    case PBVH_GRIDS:
      return (bvh->gridkey.has_mask != 0);
    case PBVH_FACES:
      return (bvh->vdata && CustomData_get_layer(bvh->vdata, CD_PAINT_MASK));
    case PBVH_BMESH:
      return (bvh->bm && (CustomData_get_offset(&bvh->bm->vdata, CD_PAINT_MASK) != -1));
  }

  return false;
}

void pbvh_show_diffuse_color_set(PBVH *bvh, bool show_diffuse_color)
{
  bvh->show_diffuse_color = !pbvh_has_mask(bvh) || show_diffuse_color;
}

void pbvh_show_mask_set(PBVH *bvh, bool show_mask)
{
  bvh->show_mask = show_mask;
}
