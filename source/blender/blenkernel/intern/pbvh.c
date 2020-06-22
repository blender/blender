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
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_ccg.h"
#include "BKE_mesh.h" /* for BKE_mesh_calc_normals */
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "PIL_time.h"

#include "GPU_buffers.h"

#include "bmesh.h"

#include "atomic_ops.h"

#include "pbvh_intern.h"

#include <limits.h>

#define LEAF_LIMIT 10000

//#define PERFCNTRS

#define STACK_FIXED_DEPTH 100

typedef struct PBVHStack {
  PBVHNode *node;
  bool revisiting;
} PBVHStack;

typedef struct PBVHIter {
  PBVH *pbvh;
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
  for (int i = 0; i < 3; i++) {
    bb->bmin[i] = min_ff(bb->bmin[i], co[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], co[i]);
  }
}

/* Expand the bounding box to include another bounding box */
void BB_expand_with_bb(BB *bb, BB *bb2)
{
  for (int i = 0; i < 3; i++) {
    bb->bmin[i] = min_ff(bb->bmin[i], bb2->bmin[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], bb2->bmax[i]);
  }
}

/* Return 0, 1, or 2 to indicate the widest axis of the bounding box */
int BB_widest_axis(const BB *bb)
{
  float dim[3];

  for (int i = 0; i < 3; i++) {
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
  for (int i = 0; i < 3; i++) {
    bbc->bcentroid[i] = (bbc->bmin[i] + bbc->bmax[i]) * 0.5f;
  }
}

/* Not recursive */
static void update_node_vb(PBVH *pbvh, PBVHNode *node)
{
  BB vb;

  BB_reset(&vb);

  if (node->flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin(pbvh, node, vd, PBVH_ITER_ALL)
    {
      BB_expand(&vb, vd.co);
    }
    BKE_pbvh_vertex_iter_end;
  }
  else {
    BB_expand_with_bb(&vb, &pbvh->nodes[node->children_offset].vb);
    BB_expand_with_bb(&vb, &pbvh->nodes[node->children_offset + 1].vb);
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
static int partition_indices_material(PBVH *pbvh, int lo, int hi)
{
  const MPoly *mpoly = pbvh->mpoly;
  const MLoopTri *looptri = pbvh->looptri;
  const DMFlagMat *flagmats = pbvh->grid_flag_mats;
  const int *indices = pbvh->prim_indices;
  const void *first;
  int i = lo, j = hi;

  if (pbvh->looptri) {
    first = &mpoly[looptri[pbvh->prim_indices[lo]].poly];
  }
  else {
    first = &flagmats[pbvh->prim_indices[lo]];
  }

  for (;;) {
    if (pbvh->looptri) {
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

    SWAP(int, pbvh->prim_indices[i], pbvh->prim_indices[j]);
    i++;
  }
}

void pbvh_grow_nodes(PBVH *pbvh, int totnode)
{
  if (UNLIKELY(totnode > pbvh->node_mem_count)) {
    pbvh->node_mem_count = pbvh->node_mem_count + (pbvh->node_mem_count / 3);
    if (pbvh->node_mem_count < totnode) {
      pbvh->node_mem_count = totnode;
    }
    pbvh->nodes = MEM_recallocN(pbvh->nodes, sizeof(PBVHNode) * pbvh->node_mem_count);
  }

  pbvh->totnode = totnode;
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(
    PBVH *pbvh, GHash *map, unsigned int *face_verts, unsigned int *uniq_verts, int vertex)
{
  void *key, **value_p;

  key = POINTER_FROM_INT(vertex);
  if (!BLI_ghash_ensure_p(map, key, &value_p)) {
    int value_i;
    if (BLI_BITMAP_TEST(pbvh->vert_bitmap, vertex) == 0) {
      BLI_BITMAP_ENABLE(pbvh->vert_bitmap, vertex);
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
static void build_mesh_leaf_node(PBVH *pbvh, PBVHNode *node)
{
  bool has_visible = false;

  node->uniq_verts = node->face_verts = 0;
  const int totface = node->totprim;

  /* reserve size is rough guess */
  GHash *map = BLI_ghash_int_new_ex("build_mesh_leaf_node gh", 2 * totface);

  int(*face_vert_indices)[3] = MEM_mallocN(sizeof(int[3]) * totface, "bvh node face vert indices");

  node->face_vert_indices = (const int(*)[3])face_vert_indices;

  if (pbvh->respect_hide == false) {
    has_visible = true;
  }

  for (int i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[node->prim_indices[i]];
    for (int j = 0; j < 3; j++) {
      face_vert_indices[i][j] = map_insert_vert(
          pbvh, map, &node->face_verts, &node->uniq_verts, pbvh->mloop[lt->tri[j]].v);
    }

    if (has_visible == false) {
      if (!paint_is_face_hidden(lt, pbvh->verts, pbvh->mloop)) {
        has_visible = true;
      }
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

  for (int i = 0; i < totface; i++) {
    const int sides = 3;

    for (int j = 0; j < sides; j++) {
      if (face_vert_indices[i][j] < 0) {
        face_vert_indices[i][j] = -face_vert_indices[i][j] + node->uniq_verts - 1;
      }
    }
  }

  BKE_pbvh_node_mark_rebuild_draw(node);

  BKE_pbvh_node_fully_hidden_set(node, !has_visible);

  BLI_ghash_free(map, NULL, NULL);
}

static void update_vb(PBVH *pbvh, PBVHNode *node, BBC *prim_bbc, int offset, int count)
{
  BB_reset(&node->vb);
  for (int i = offset + count - 1; i >= offset; i--) {
    BB_expand_with_bb(&node->vb, (BB *)(&prim_bbc[pbvh->prim_indices[i]]));
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

void BKE_pbvh_sync_face_sets_to_grids(PBVH *pbvh)
{
  const int gridsize = pbvh->gridkey.grid_size;
  for (int i = 0; i < pbvh->totgrid; i++) {
    BLI_bitmap *gh = pbvh->grid_hidden[i];
    const int face_index = BKE_subdiv_ccg_grid_to_face_index(pbvh->subdiv_ccg, i);
    if (!gh && pbvh->face_sets[face_index] < 0) {
      gh = pbvh->grid_hidden[i] = BLI_BITMAP_NEW(pbvh->gridkey.grid_area,
                                                 "partialvis_update_grids");
    }
    if (gh) {
      for (int y = 0; y < gridsize; y++) {
        for (int x = 0; x < gridsize; x++) {
          BLI_BITMAP_SET(gh, y * gridsize + x, pbvh->face_sets[face_index] < 0);
        }
      }
    }
  }
}

static void build_grid_leaf_node(PBVH *pbvh, PBVHNode *node)
{
  int totquads = BKE_pbvh_count_grid_quads(
      pbvh->grid_hidden, node->prim_indices, node->totprim, pbvh->gridkey.grid_size);
  BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
  BKE_pbvh_node_mark_rebuild_draw(node);
}

static void build_leaf(PBVH *pbvh, int node_index, BBC *prim_bbc, int offset, int count)
{
  pbvh->nodes[node_index].flag |= PBVH_Leaf;

  pbvh->nodes[node_index].prim_indices = pbvh->prim_indices + offset;
  pbvh->nodes[node_index].totprim = count;

  /* Still need vb for searches */
  update_vb(pbvh, &pbvh->nodes[node_index], prim_bbc, offset, count);

  if (pbvh->looptri) {
    build_mesh_leaf_node(pbvh, pbvh->nodes + node_index);
  }
  else {
    build_grid_leaf_node(pbvh, pbvh->nodes + node_index);
  }
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static bool leaf_needs_material_split(PBVH *pbvh, int offset, int count)
{
  if (count <= 1) {
    return false;
  }

  if (pbvh->looptri) {
    const MLoopTri *first = &pbvh->looptri[pbvh->prim_indices[offset]];
    const MPoly *mp = &pbvh->mpoly[first->poly];

    for (int i = offset + count - 1; i > offset; i--) {
      int prim = pbvh->prim_indices[i];
      const MPoly *mp_other = &pbvh->mpoly[pbvh->looptri[prim].poly];
      if (!face_materials_match(mp, mp_other)) {
        return true;
      }
    }
  }
  else {
    const DMFlagMat *first = &pbvh->grid_flag_mats[pbvh->prim_indices[offset]];

    for (int i = offset + count - 1; i > offset; i--) {
      int prim = pbvh->prim_indices[i];
      if (!grid_materials_match(first, &pbvh->grid_flag_mats[prim])) {
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

static void build_sub(PBVH *pbvh, int node_index, BB *cb, BBC *prim_bbc, int offset, int count)
{
  int end;
  BB cb_backing;

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = count <= pbvh->leaf_limit;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(pbvh, offset, count)) {
      build_leaf(pbvh, node_index, prim_bbc, offset, count);
      return;
    }
  }

  /* Add two child nodes */
  pbvh->nodes[node_index].children_offset = pbvh->totnode;
  pbvh_grow_nodes(pbvh, pbvh->totnode + 2);

  /* Update parent node bounding box */
  update_vb(pbvh, &pbvh->nodes[node_index], prim_bbc, offset, count);

  if (!below_leaf_limit) {
    /* Find axis with widest range of primitive centroids */
    if (!cb) {
      cb = &cb_backing;
      BB_reset(cb);
      for (int i = offset + count - 1; i >= offset; i--) {
        BB_expand(cb, prim_bbc[pbvh->prim_indices[i]].bcentroid);
      }
    }
    const int axis = BB_widest_axis(cb);

    /* Partition primitives along that axis */
    end = partition_indices(pbvh->prim_indices,
                            offset,
                            offset + count - 1,
                            axis,
                            (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
                            prim_bbc);
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material(pbvh, offset, offset + count - 1);
  }

  /* Build children */
  build_sub(pbvh, pbvh->nodes[node_index].children_offset, NULL, prim_bbc, offset, end - offset);
  build_sub(pbvh,
            pbvh->nodes[node_index].children_offset + 1,
            NULL,
            prim_bbc,
            end,
            offset + count - end);
}

static void pbvh_build(PBVH *pbvh, BB *cb, BBC *prim_bbc, int totprim)
{
  if (totprim != pbvh->totprim) {
    pbvh->totprim = totprim;
    if (pbvh->nodes) {
      MEM_freeN(pbvh->nodes);
    }
    if (pbvh->prim_indices) {
      MEM_freeN(pbvh->prim_indices);
    }
    pbvh->prim_indices = MEM_mallocN(sizeof(int) * totprim, "bvh prim indices");
    for (int i = 0; i < totprim; i++) {
      pbvh->prim_indices[i] = i;
    }
    pbvh->totnode = 0;
    if (pbvh->node_mem_count < 100) {
      pbvh->node_mem_count = 100;
      pbvh->nodes = MEM_callocN(sizeof(PBVHNode) * pbvh->node_mem_count, "bvh initial nodes");
    }
  }

  pbvh->totnode = 1;
  build_sub(pbvh, 0, cb, prim_bbc, 0, totprim);
}

/**
 * Do a full rebuild with on Mesh data structure.
 *
 * \note Unlike mpoly/mloop/verts, looptri is **totally owned** by PBVH
 * (which means it may rewrite it if needed, see #BKE_pbvh_vert_coords_apply().
 */
void BKE_pbvh_build_mesh(PBVH *pbvh,
                         const Mesh *mesh,
                         const MPoly *mpoly,
                         const MLoop *mloop,
                         MVert *verts,
                         int totvert,
                         struct CustomData *vdata,
                         struct CustomData *ldata,
                         struct CustomData *pdata,
                         const MLoopTri *looptri,
                         int looptri_num)
{
  BBC *prim_bbc = NULL;
  BB cb;

  pbvh->mesh = mesh;
  pbvh->type = PBVH_FACES;
  pbvh->mpoly = mpoly;
  pbvh->mloop = mloop;
  pbvh->looptri = looptri;
  pbvh->verts = verts;
  pbvh->vert_bitmap = BLI_BITMAP_NEW(totvert, "bvh->vert_bitmap");
  pbvh->totvert = totvert;
  pbvh->leaf_limit = LEAF_LIMIT;
  pbvh->vdata = vdata;
  pbvh->ldata = ldata;
  pbvh->pdata = pdata;

  pbvh->face_sets_color_seed = mesh->face_sets_color_seed;
  pbvh->face_sets_color_default = mesh->face_sets_color_default;

  BB_reset(&cb);

  /* For each face, store the AABB and the AABB centroid */
  prim_bbc = MEM_mallocN(sizeof(BBC) * looptri_num, "prim_bbc");

  for (int i = 0; i < looptri_num; i++) {
    const MLoopTri *lt = &looptri[i];
    const int sides = 3;
    BBC *bbc = prim_bbc + i;

    BB_reset((BB *)bbc);

    for (int j = 0; j < sides; j++) {
      BB_expand((BB *)bbc, verts[pbvh->mloop[lt->tri[j]].v].co);
    }

    BBC_update_centroid(bbc);

    BB_expand(&cb, bbc->bcentroid);
  }

  if (looptri_num) {
    pbvh_build(pbvh, &cb, prim_bbc, looptri_num);
  }

  MEM_freeN(prim_bbc);
  MEM_freeN(pbvh->vert_bitmap);
}

/* Do a full rebuild with on Grids data structure */
void BKE_pbvh_build_grids(PBVH *pbvh,
                          CCGElem **grids,
                          int totgrid,
                          CCGKey *key,
                          void **gridfaces,
                          DMFlagMat *flagmats,
                          BLI_bitmap **grid_hidden)
{
  const int gridsize = key->grid_size;

  pbvh->type = PBVH_GRIDS;
  pbvh->grids = grids;
  pbvh->gridfaces = gridfaces;
  pbvh->grid_flag_mats = flagmats;
  pbvh->totgrid = totgrid;
  pbvh->gridkey = *key;
  pbvh->grid_hidden = grid_hidden;
  pbvh->leaf_limit = max_ii(LEAF_LIMIT / ((gridsize - 1) * (gridsize - 1)), 1);

  BB cb;
  BB_reset(&cb);

  /* For each grid, store the AABB and the AABB centroid */
  BBC *prim_bbc = MEM_mallocN(sizeof(BBC) * totgrid, "prim_bbc");

  for (int i = 0; i < totgrid; i++) {
    CCGElem *grid = grids[i];
    BBC *bbc = prim_bbc + i;

    BB_reset((BB *)bbc);

    for (int j = 0; j < gridsize * gridsize; j++) {
      BB_expand((BB *)bbc, CCG_elem_offset_co(key, grid, j));
    }

    BBC_update_centroid(bbc);

    BB_expand(&cb, bbc->bcentroid);
  }

  if (totgrid) {
    pbvh_build(pbvh, &cb, prim_bbc, totgrid);
  }

  MEM_freeN(prim_bbc);
}

PBVH *BKE_pbvh_new(void)
{
  PBVH *pbvh = MEM_callocN(sizeof(PBVH), "pbvh");
  pbvh->respect_hide = true;
  return pbvh;
}

void BKE_pbvh_free(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = &pbvh->nodes[i];

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

  if (pbvh->deformed) {
    if (pbvh->verts) {
      /* if pbvh was deformed, new memory was allocated for verts/faces -- free it */

      MEM_freeN((void *)pbvh->verts);
    }
  }

  if (pbvh->looptri) {
    MEM_freeN((void *)pbvh->looptri);
  }

  if (pbvh->nodes) {
    MEM_freeN(pbvh->nodes);
  }

  if (pbvh->prim_indices) {
    MEM_freeN(pbvh->prim_indices);
  }

  MEM_freeN(pbvh);
}

static void pbvh_iter_begin(PBVHIter *iter,
                            PBVH *pbvh,
                            BKE_pbvh_SearchCallback scb,
                            void *search_data)
{
  iter->pbvh = pbvh;
  iter->scb = scb;
  iter->search_data = search_data;

  iter->stack = iter->stackfixed;
  iter->stackspace = STACK_FIXED_DEPTH;

  iter->stack[0].node = pbvh->nodes;
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
      pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset + 1, false);
      pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset, false);
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
      pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset + 1, false);
      pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset, false);
    }
  }

  return NULL;
}

void BKE_pbvh_search_gather(
    PBVH *pbvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***r_array, int *r_tot)
{
  PBVHIter iter;
  PBVHNode **array = NULL, *node;
  int tot = 0, space = 0;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

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

void BKE_pbvh_search_callback(PBVH *pbvh,
                              BKE_pbvh_SearchCallback scb,
                              void *search_data,
                              BKE_pbvh_HitCallback hcb,
                              void *hit_data)
{
  PBVHIter iter;
  PBVHNode *node;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

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

static void BKE_pbvh_search_callback_occluded(PBVH *pbvh,
                                              BKE_pbvh_SearchCallback scb,
                                              void *search_data,
                                              BKE_pbvh_HitOccludedCallback hcb,
                                              void *hit_data)
{
  PBVHIter iter;
  PBVHNode *node;
  node_tree *tree = NULL;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

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
  PBVH *pbvh;
  PBVHNode **nodes;
  int totnode;

  float (*vnors)[3];
  int flag;
  bool show_sculpt_face_sets;
} PBVHUpdateData;

static void pbvh_update_normals_accum_task_cb(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;

  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  float(*vnors)[3] = data->vnors;

  if ((node->flag & PBVH_UpdateNormals)) {
    unsigned int mpoly_prev = UINT_MAX;
    float fn[3];

    const int *faces = node->prim_indices;
    const int totface = node->totprim;

    for (int i = 0; i < totface; i++) {
      const MLoopTri *lt = &pbvh->looptri[faces[i]];
      const unsigned int vtri[3] = {
          pbvh->mloop[lt->tri[0]].v,
          pbvh->mloop[lt->tri[1]].v,
          pbvh->mloop[lt->tri[2]].v,
      };
      const int sides = 3;

      /* Face normal and mask */
      if (lt->poly != mpoly_prev) {
        const MPoly *mp = &pbvh->mpoly[lt->poly];
        BKE_mesh_calc_poly_normal(mp, &pbvh->mloop[mp->loopstart], pbvh->verts, fn);
        mpoly_prev = lt->poly;
      }

      for (int j = sides; j--;) {
        const int v = vtri[j];

        if (pbvh->verts[v].flag & ME_VERT_PBVH_UPDATE) {
          /* Note: This avoids `lock, add_v3_v3, unlock`
           * and is five to ten times quicker than a spin-lock.
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
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  float(*vnors)[3] = data->vnors;

  if (node->flag & PBVH_UpdateNormals) {
    const int *verts = node->vert_indices;
    const int totvert = node->uniq_verts;

    for (int i = 0; i < totvert; i++) {
      const int v = verts[i];
      MVert *mvert = &pbvh->verts[v];

      /* No atomics necessary because we are iterating over uniq_verts only,
       * so we know only this thread will handle this vertex. */
      if (mvert->flag & ME_VERT_PBVH_UPDATE) {
        normalize_v3(vnors[v]);
        normal_float_to_short_v3(mvert->no, vnors[v]);
        mvert->flag &= ~ME_VERT_PBVH_UPDATE;
      }
    }

    node->flag &= ~PBVH_UpdateNormals;
  }
}

static void pbvh_faces_update_normals(PBVH *pbvh, PBVHNode **nodes, int totnode)
{
  /* could be per node to save some memory, but also means
   * we have to store for each vertex which node it is in */
  float(*vnors)[3] = MEM_callocN(sizeof(*vnors) * pbvh->totvert, __func__);

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
      .pbvh = pbvh,
      .nodes = nodes,
      .vnors = vnors,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);

  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_accum_task_cb, &settings);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_store_task_cb, &settings);

  MEM_freeN(vnors);
}

static void pbvh_update_mask_redraw_task_cb(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict UNUSED(tls))
{

  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  if (node->flag & PBVH_UpdateMask) {

    bool has_unmasked = false;
    bool has_masked = true;
    if (node->flag & PBVH_Leaf) {
      PBVHVertexIter vd;

      BKE_pbvh_vertex_iter_begin(pbvh, node, vd, PBVH_ITER_ALL)
      {
        if (vd.mask && *vd.mask < 1.0f) {
          has_unmasked = true;
        }
        if (vd.mask && *vd.mask > 0.0f) {
          has_masked = false;
        }
      }
      BKE_pbvh_vertex_iter_end;
    }
    else {
      has_unmasked = true;
      has_masked = true;
    }
    BKE_pbvh_node_fully_masked_set(node, !has_unmasked);
    BKE_pbvh_node_fully_unmasked_set(node, has_masked);

    node->flag &= ~PBVH_UpdateMask;
  }
}

static void pbvh_update_mask_redraw(PBVH *pbvh, PBVHNode **nodes, int totnode, int flag)
{
  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
      .flag = flag,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_mask_redraw_task_cb, &settings);
}

static void pbvh_update_visibility_redraw_task_cb(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict UNUSED(tls))
{

  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  if (node->flag & PBVH_UpdateVisibility) {
    node->flag &= ~PBVH_UpdateVisibility;
    BKE_pbvh_node_fully_hidden_set(node, true);
    if (node->flag & PBVH_Leaf) {
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin(pbvh, node, vd, PBVH_ITER_ALL)
      {
        if (vd.visible) {
          BKE_pbvh_node_fully_hidden_set(node, false);
          return;
        }
      }
      BKE_pbvh_vertex_iter_end;
    }
  }
}

static void pbvh_update_visibility_redraw(PBVH *pbvh, PBVHNode **nodes, int totnode, int flag)
{
  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
      .flag = flag,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_visibility_redraw_task_cb, &settings);
}

static void pbvh_update_BB_redraw_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  const int flag = data->flag;

  if ((flag & PBVH_UpdateBB) && (node->flag & PBVH_UpdateBB)) {
    /* don't clear flag yet, leave it for flushing later */
    /* Note that bvh usage is read-only here, so no need to thread-protect it. */
    update_node_vb(pbvh, node);
  }

  if ((flag & PBVH_UpdateOriginalBB) && (node->flag & PBVH_UpdateOriginalBB)) {
    node->orig_vb = node->vb;
  }

  if ((flag & PBVH_UpdateRedraw) && (node->flag & PBVH_UpdateRedraw)) {
    node->flag &= ~PBVH_UpdateRedraw;
  }
}

void pbvh_update_BB_redraw(PBVH *pbvh, PBVHNode **nodes, int totnode, int flag)
{
  /* update BB, redraw flag */
  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
      .flag = flag,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_BB_redraw_task_cb, &settings);
}

static int pbvh_get_buffers_update_flags(PBVH *UNUSED(pbvh))
{
  int update_flags = GPU_PBVH_BUFFERS_SHOW_VCOL | GPU_PBVH_BUFFERS_SHOW_MASK |
                     GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS;
  return update_flags;
}

static void pbvh_update_draw_buffer_cb(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  /* Create and update draw buffers. The functions called here must not
   * do any OpenGL calls. Flags are not cleared immediately, that happens
   * after GPU_pbvh_buffer_flush() which does the final OpenGL calls. */
  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];

  if (node->flag & PBVH_RebuildDrawBuffers) {
    switch (pbvh->type) {
      case PBVH_GRIDS:
        node->draw_buffers = GPU_pbvh_grid_buffers_build(node->totprim, pbvh->grid_hidden);
        break;
      case PBVH_FACES:
        node->draw_buffers = GPU_pbvh_mesh_buffers_build(
            pbvh->mpoly,
            pbvh->mloop,
            pbvh->looptri,
            pbvh->verts,
            node->prim_indices,
            CustomData_get_layer(pbvh->pdata, CD_SCULPT_FACE_SETS),
            node->totprim,
            pbvh->mesh);
        break;
      case PBVH_BMESH:
        node->draw_buffers = GPU_pbvh_bmesh_buffers_build(pbvh->flags &
                                                          PBVH_DYNTOPO_SMOOTH_SHADING);
        break;
    }
  }

  if (node->flag & PBVH_UpdateDrawBuffers) {
    const int update_flags = pbvh_get_buffers_update_flags(pbvh);
    switch (pbvh->type) {
      case PBVH_GRIDS:
        GPU_pbvh_grid_buffers_update(node->draw_buffers,
                                     pbvh->subdiv_ccg,
                                     pbvh->grids,
                                     pbvh->grid_flag_mats,
                                     node->prim_indices,
                                     node->totprim,
                                     pbvh->face_sets,
                                     pbvh->face_sets_color_seed,
                                     pbvh->face_sets_color_default,
                                     &pbvh->gridkey,
                                     update_flags);
        break;
      case PBVH_FACES:
        GPU_pbvh_mesh_buffers_update(node->draw_buffers,
                                     pbvh->verts,
                                     CustomData_get_layer(pbvh->vdata, CD_PAINT_MASK),
                                     CustomData_get_layer(pbvh->ldata, CD_MLOOPCOL),
                                     CustomData_get_layer(pbvh->pdata, CD_SCULPT_FACE_SETS),
                                     pbvh->face_sets_color_seed,
                                     pbvh->face_sets_color_default,
                                     CustomData_get_layer(pbvh->vdata, CD_PROP_COLOR),
                                     update_flags);
        break;
      case PBVH_BMESH:
        GPU_pbvh_bmesh_buffers_update(node->draw_buffers,
                                      pbvh->bm,
                                      node->bm_faces,
                                      node->bm_unique_verts,
                                      node->bm_other_verts,
                                      update_flags);
        break;
    }
  }
}

static void pbvh_update_draw_buffers(PBVH *pbvh, PBVHNode **nodes, int totnode, int update_flag)
{
  if ((update_flag & PBVH_RebuildDrawBuffers) || ELEM(pbvh->type, PBVH_GRIDS, PBVH_BMESH)) {
    /* Free buffers uses OpenGL, so not in parallel. */
    for (int n = 0; n < totnode; n++) {
      PBVHNode *node = nodes[n];
      if (node->flag & PBVH_RebuildDrawBuffers) {
        GPU_pbvh_buffers_free(node->draw_buffers);
        node->draw_buffers = NULL;
      }
      else if ((node->flag & PBVH_UpdateDrawBuffers) && node->draw_buffers) {
        if (pbvh->type == PBVH_GRIDS) {
          GPU_pbvh_grid_buffers_update_free(
              node->draw_buffers, pbvh->grid_flag_mats, node->prim_indices);
        }
        else if (pbvh->type == PBVH_BMESH) {
          GPU_pbvh_bmesh_buffers_update_free(node->draw_buffers);
        }
      }
    }
  }

  /* Parallel creation and update of draw buffers. */
  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_draw_buffer_cb, &settings);
}

static int pbvh_flush_bb(PBVH *pbvh, PBVHNode *node, int flag)
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
    update |= pbvh_flush_bb(pbvh, pbvh->nodes + node->children_offset, flag);
    update |= pbvh_flush_bb(pbvh, pbvh->nodes + node->children_offset + 1, flag);

    if (update & PBVH_UpdateBB) {
      update_node_vb(pbvh, node);
    }
    if (update & PBVH_UpdateOriginalBB) {
      node->orig_vb = node->vb;
    }
  }

  return update;
}

void BKE_pbvh_update_bounds(PBVH *pbvh, int flag)
{
  if (!pbvh->nodes) {
    return;
  }

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(pbvh, update_search_cb, POINTER_FROM_INT(flag), &nodes, &totnode);

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw)) {
    pbvh_update_BB_redraw(pbvh, nodes, totnode, flag);
  }

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB)) {
    pbvh_flush_bb(pbvh, pbvh->nodes, flag);
  }

  MEM_SAFE_FREE(nodes);
}

void BKE_pbvh_update_vertex_data(PBVH *pbvh, int flag)
{
  if (!pbvh->nodes) {
    return;
  }

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(pbvh, update_search_cb, POINTER_FROM_INT(flag), &nodes, &totnode);

  if (flag & (PBVH_UpdateMask)) {
    pbvh_update_mask_redraw(pbvh, nodes, totnode, flag);
  }

  if (flag & (PBVH_UpdateColor)) {
    /* Do nothing */
  }

  if (flag & (PBVH_UpdateVisibility)) {
    pbvh_update_visibility_redraw(pbvh, nodes, totnode, flag);
  }

  if (nodes) {
    MEM_freeN(nodes);
  }
}

static void pbvh_faces_node_visibility_update(PBVH *pbvh, PBVHNode *node)
{
  MVert *mvert;
  const int *vert_indices;
  int totvert, i;
  BKE_pbvh_node_num_verts(pbvh, node, NULL, &totvert);
  BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);

  for (i = 0; i < totvert; i++) {
    MVert *v = &mvert[vert_indices[i]];
    if (!(v->flag & ME_HIDE)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  BKE_pbvh_node_fully_hidden_set(node, true);
}

static void pbvh_grids_node_visibility_update(PBVH *pbvh, PBVHNode *node)
{
  CCGElem **grids;
  BLI_bitmap **grid_hidden;
  int *grid_indices, totgrid, i;

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, NULL, NULL, &grids);
  grid_hidden = BKE_pbvh_grid_hidden(pbvh);
  CCGKey key = *BKE_pbvh_get_grid_key(pbvh);

  for (i = 0; i < totgrid; i++) {
    int g = grid_indices[i], x, y;
    BLI_bitmap *gh = grid_hidden[g];

    if (!gh) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }

    for (y = 0; y < key.grid_size; y++) {
      for (x = 0; x < key.grid_size; x++) {
        if (!BLI_BITMAP_TEST(gh, y * key.grid_size + x)) {
          BKE_pbvh_node_fully_hidden_set(node, false);
          return;
        }
      }
    }
  }
  BKE_pbvh_node_fully_hidden_set(node, true);
}

static void pbvh_bmesh_node_visibility_update(PBVHNode *node)
{
  GSet *unique, *other;

  unique = BKE_pbvh_bmesh_node_unique_verts(node);
  other = BKE_pbvh_bmesh_node_other_verts(node);

  GSetIterator gs_iter;

  GSET_ITER (gs_iter, unique) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  GSET_ITER (gs_iter, other) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  BKE_pbvh_node_fully_hidden_set(node, true);
}

static void pbvh_update_visibility_task_cb(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{

  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  if (node->flag & PBVH_UpdateMask) {
    switch (BKE_pbvh_type(pbvh)) {
      case PBVH_FACES:
        pbvh_faces_node_visibility_update(pbvh, node);
        break;
      case PBVH_GRIDS:
        pbvh_grids_node_visibility_update(pbvh, node);
        break;
      case PBVH_BMESH:
        pbvh_bmesh_node_visibility_update(node);
        break;
    }
    node->flag &= ~PBVH_UpdateMask;
  }
}

static void pbvh_update_visibility(PBVH *pbvh, PBVHNode **nodes, int totnode)
{
  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_visibility_task_cb, &settings);
}

void BKE_pbvh_update_visibility(PBVH *pbvh)
{
  if (!pbvh->nodes) {
    return;
  }

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateVisibility), &nodes, &totnode);
  pbvh_update_visibility(pbvh, nodes, totnode);

  if (nodes) {
    MEM_freeN(nodes);
  }
}

void BKE_pbvh_redraw_BB(PBVH *pbvh, float bb_min[3], float bb_max[3])
{
  PBVHIter iter;
  PBVHNode *node;
  BB bb;

  BB_reset(&bb);

  pbvh_iter_begin(&iter, pbvh, NULL, NULL);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_UpdateRedraw) {
      BB_expand_with_bb(&bb, &node->vb);
    }
  }

  pbvh_iter_end(&iter);

  copy_v3_v3(bb_min, bb.bmin);
  copy_v3_v3(bb_max, bb.bmax);
}

void BKE_pbvh_get_grid_updates(PBVH *pbvh, bool clear, void ***r_gridfaces, int *r_totface)
{
  GSet *face_set = BLI_gset_ptr_new(__func__);
  PBVHNode *node;
  PBVHIter iter;

  pbvh_iter_begin(&iter, pbvh, NULL, NULL);

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_UpdateNormals) {
      for (uint i = 0; i < node->totprim; i++) {
        void *face = pbvh->gridfaces[node->prim_indices[i]];
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

PBVHType BKE_pbvh_type(const PBVH *pbvh)
{
  return pbvh->type;
}

bool BKE_pbvh_has_faces(const PBVH *pbvh)
{
  if (pbvh->type == PBVH_BMESH) {
    return (pbvh->bm->totface != 0);
  }
  else {
    return (pbvh->totprim != 0);
  }
}

void BKE_pbvh_bounding_box(const PBVH *pbvh, float min[3], float max[3])
{
  if (pbvh->totnode) {
    const BB *bb = &pbvh->nodes[0].vb;
    copy_v3_v3(min, bb->bmin);
    copy_v3_v3(max, bb->bmax);
  }
  else {
    zero_v3(min);
    zero_v3(max);
  }
}

BLI_bitmap **BKE_pbvh_grid_hidden(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_GRIDS);
  return pbvh->grid_hidden;
}

const CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_GRIDS);
  return &pbvh->gridkey;
}

struct CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_GRIDS);
  return pbvh->grids;
}

BLI_bitmap **BKE_pbvh_get_grid_visibility(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_GRIDS);
  return pbvh->grid_hidden;
}

int BKE_pbvh_get_grid_num_vertices(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_GRIDS);
  return pbvh->totgrid * pbvh->gridkey.grid_area;
}

BMesh *BKE_pbvh_get_bmesh(PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_BMESH);
  return pbvh->bm;
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_mask(PBVHNode *node)
{
  node->flag |= PBVH_UpdateMask | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_color(PBVHNode *node)
{
  node->flag |= PBVH_UpdateColor | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_visibility(PBVHNode *node)
{
  node->flag |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                PBVH_UpdateRedraw;
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

void BKE_pbvh_node_fully_masked_set(PBVHNode *node, int fully_masked)
{
  BLI_assert(node->flag & PBVH_Leaf);

  if (fully_masked) {
    node->flag |= PBVH_FullyMasked;
  }
  else {
    node->flag &= ~PBVH_FullyMasked;
  }
}

bool BKE_pbvh_node_fully_masked_get(PBVHNode *node)
{
  return (node->flag & PBVH_Leaf) && (node->flag & PBVH_FullyMasked);
}

void BKE_pbvh_node_fully_unmasked_set(PBVHNode *node, int fully_masked)
{
  BLI_assert(node->flag & PBVH_Leaf);

  if (fully_masked) {
    node->flag |= PBVH_FullyUnmasked;
  }
  else {
    node->flag &= ~PBVH_FullyUnmasked;
  }
}

bool BKE_pbvh_node_fully_unmasked_get(PBVHNode *node)
{
  return (node->flag & PBVH_Leaf) && (node->flag & PBVH_FullyUnmasked);
}

void BKE_pbvh_node_get_verts(PBVH *pbvh,
                             PBVHNode *node,
                             const int **r_vert_indices,
                             MVert **r_verts)
{
  if (r_vert_indices) {
    *r_vert_indices = node->vert_indices;
  }

  if (r_verts) {
    *r_verts = pbvh->verts;
  }
}

void BKE_pbvh_node_num_verts(PBVH *pbvh, PBVHNode *node, int *r_uniquevert, int *r_totvert)
{
  int tot;

  switch (pbvh->type) {
    case PBVH_GRIDS:
      tot = node->totprim * pbvh->gridkey.grid_area;
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

void BKE_pbvh_node_get_grids(PBVH *pbvh,
                             PBVHNode *node,
                             int **r_grid_indices,
                             int *r_totgrid,
                             int *r_maxgrid,
                             int *r_gridsize,
                             CCGElem ***r_griddata)
{
  switch (pbvh->type) {
    case PBVH_GRIDS:
      if (r_grid_indices) {
        *r_grid_indices = node->prim_indices;
      }
      if (r_totgrid) {
        *r_totgrid = node->totprim;
      }
      if (r_maxgrid) {
        *r_maxgrid = pbvh->totgrid;
      }
      if (r_gridsize) {
        *r_gridsize = pbvh->gridkey.grid_size;
      }
      if (r_griddata) {
        *r_griddata = pbvh->grids;
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
 * however this is important to avoid having to recalculate bound-box & sync the buffers to the
 * GPU (which is far more expensive!) See: T47232.
 */
bool BKE_pbvh_node_vert_update_check_any(PBVH *pbvh, PBVHNode *node)
{
  BLI_assert(pbvh->type == PBVH_FACES);
  const int *verts = node->vert_indices;
  const int totvert = node->uniq_verts + node->face_verts;

  for (int i = 0; i < totvert; i++) {
    const int v = verts[i];
    const MVert *mvert = &pbvh->verts[v];

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

void BKE_pbvh_raycast(PBVH *pbvh,
                      BKE_pbvh_HitOccludedCallback cb,
                      void *data,
                      const float ray_start[3],
                      const float ray_normal[3],
                      bool original)
{
  RaycastData rcd;

  isect_ray_aabb_v3_precalc(&rcd.ray, ray_start, ray_normal);
  rcd.original = original;

  BKE_pbvh_search_callback_occluded(pbvh, ray_aabb_intersect, &rcd, cb, data);
}

bool ray_face_intersection_quad(const float ray_start[3],
                                struct IsectRayPrecalc *isect_precalc,
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth)
{
  float depth_test;

  if ((isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, NULL) &&
       (depth_test < *depth)) ||
      (isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t2, t3, &depth_test, NULL) &&
       (depth_test < *depth))) {
    *depth = depth_test;
    return true;
  }
  else {
    return false;
  }
}

bool ray_face_intersection_tri(const float ray_start[3],
                               struct IsectRayPrecalc *isect_precalc,
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth)
{
  float depth_test;
  if ((isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, NULL) &&
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

static bool pbvh_faces_node_raycast(PBVH *pbvh,
                                    const PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    struct IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    int *r_active_vertex_index,
                                    int *r_active_face_index,
                                    float *r_face_normal)
{
  const MVert *vert = pbvh->verts;
  const MLoop *mloop = pbvh->mloop;
  const int *faces = node->prim_indices;
  int totface = node->totprim;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  for (int i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (pbvh->respect_hide && paint_is_face_hidden(lt, vert, mloop)) {
      continue;
    }

    const float *co[3];
    if (origco) {
      /* intersect with backuped original coordinates */
      co[0] = origco[face_verts[0]];
      co[1] = origco[face_verts[1]];
      co[2] = origco[face_verts[2]];
    }
    else {
      /* intersect with current coordinates */
      co[0] = vert[mloop[lt->tri[0]].v].co;
      co[1] = vert[mloop[lt->tri[1]].v].co;
      co[2] = vert[mloop[lt->tri[2]].v].co;
    }

    if (ray_face_intersection_tri(ray_start, isect_precalc, co[0], co[1], co[2], depth)) {
      hit = true;

      if (r_face_normal) {
        normal_tri_v3(r_face_normal, co[0], co[1], co[2]);
      }

      if (r_active_vertex_index) {
        float location[3] = {0.0f};
        madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);
        for (int j = 0; j < 3; j++) {
          /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
           * uninitialized values. This stores the closest vertex in the current intersecting
           * triangle. */
          if (j == 0 ||
              len_squared_v3v3(location, co[j]) < len_squared_v3v3(location, nearest_vertex_co)) {
            copy_v3_v3(nearest_vertex_co, co[j]);
            *r_active_vertex_index = mloop[lt->tri[j]].v;
            *r_active_face_index = lt->poly;
          }
        }
      }
    }
  }

  return hit;
}

static bool pbvh_grids_node_raycast(PBVH *pbvh,
                                    PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    struct IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    int *r_active_vertex_index,
                                    int *r_active_grid_index,
                                    float *r_face_normal)
{
  const int totgrid = node->totprim;
  const int gridsize = pbvh->gridkey.grid_size;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0};
  const CCGKey *gridkey = &pbvh->gridkey;

  for (int i = 0; i < totgrid; i++) {
    const int grid_index = node->prim_indices[i];
    CCGElem *grid = pbvh->grids[grid_index];
    BLI_bitmap *gh;

    if (!grid) {
      continue;
    }

    gh = pbvh->grid_hidden[grid_index];

    for (int y = 0; y < gridsize - 1; y++) {
      for (int x = 0; x < gridsize - 1; x++) {
        /* check if grid face is hidden */
        if (gh) {
          if (paint_is_grid_face_hidden(gh, gridsize, x, y)) {
            continue;
          }
        }

        const float *co[4];
        if (origco) {
          co[0] = origco[y * gridsize + x];
          co[1] = origco[y * gridsize + x + 1];
          co[2] = origco[(y + 1) * gridsize + x + 1];
          co[3] = origco[(y + 1) * gridsize + x];
        }
        else {
          co[0] = CCG_grid_elem_co(gridkey, grid, x, y);
          co[1] = CCG_grid_elem_co(gridkey, grid, x + 1, y);
          co[2] = CCG_grid_elem_co(gridkey, grid, x + 1, y + 1);
          co[3] = CCG_grid_elem_co(gridkey, grid, x, y + 1);
        }

        if (ray_face_intersection_quad(
                ray_start, isect_precalc, co[0], co[1], co[2], co[3], depth)) {
          hit = true;

          if (r_face_normal) {
            normal_quad_v3(r_face_normal, co[0], co[1], co[2], co[3]);
          }

          if (r_active_vertex_index) {
            float location[3] = {0.0};
            madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);

            const int x_it[4] = {0, 1, 1, 0};
            const int y_it[4] = {0, 0, 1, 1};

            for (int j = 0; j < 4; j++) {
              /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
               * uninitialized values. This stores the closest vertex in the current intersecting
               * quad. */
              if (j == 0 || len_squared_v3v3(location, co[j]) <
                                len_squared_v3v3(location, nearest_vertex_co)) {
                copy_v3_v3(nearest_vertex_co, co[j]);

                *r_active_vertex_index = gridkey->grid_area * grid_index +
                                         (y + y_it[j]) * gridkey->grid_size + (x + x_it[j]);
              }
            }
          }
          if (r_active_grid_index) {
            *r_active_grid_index = grid_index;
          }
        }
      }
    }

    if (origco) {
      origco += gridsize * gridsize;
    }
  }

  return hit;
}

bool BKE_pbvh_node_raycast(PBVH *pbvh,
                           PBVHNode *node,
                           float (*origco)[3],
                           bool use_origco,
                           const float ray_start[3],
                           const float ray_normal[3],
                           struct IsectRayPrecalc *isect_precalc,
                           float *depth,
                           int *active_vertex_index,
                           int *active_face_grid_index,
                           float *face_normal)
{
  bool hit = false;

  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  switch (pbvh->type) {
    case PBVH_FACES:
      hit |= pbvh_faces_node_raycast(pbvh,
                                     node,
                                     origco,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     depth,
                                     active_vertex_index,
                                     active_face_grid_index,
                                     face_normal);
      break;
    case PBVH_GRIDS:
      hit |= pbvh_grids_node_raycast(pbvh,
                                     node,
                                     origco,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     depth,
                                     active_vertex_index,
                                     active_face_grid_index,
                                     face_normal);
      break;
    case PBVH_BMESH:
      BM_mesh_elem_index_ensure(pbvh->bm, BM_VERT);
      hit = pbvh_bmesh_node_raycast(node,
                                    ray_start,
                                    ray_normal,
                                    isect_precalc,
                                    depth,
                                    use_origco,
                                    active_vertex_index,
                                    face_normal);
      break;
  }

  return hit;
}

void BKE_pbvh_raycast_project_ray_root(
    PBVH *pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (pbvh->nodes) {
    float rootmin_start, rootmin_end;
    float bb_min_root[3], bb_max_root[3], bb_center[3], bb_diff[3];
    struct IsectRayAABB_Precalc ray;
    float ray_normal_inv[3];
    float offset = 1.0f + 1e-3f;
    float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

    if (original) {
      BKE_pbvh_node_get_original_BB(pbvh->nodes, bb_min_root, bb_max_root);
    }
    else {
      BKE_pbvh_node_get_BB(pbvh->nodes, bb_min_root, bb_max_root);
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

void BKE_pbvh_find_nearest_to_ray(PBVH *pbvh,
                                  BKE_pbvh_SearchNearestCallback cb,
                                  void *data,
                                  const float ray_start[3],
                                  const float ray_normal[3],
                                  bool original)
{
  FindNearestRayData ncd;

  dist_squared_ray_to_aabb_v3_precalc(&ncd.dist_ray_to_aabb_precalc, ray_start, ray_normal);
  ncd.original = original;

  BKE_pbvh_search_callback_occluded(pbvh, nearest_to_ray_aabb_dist_sq, &ncd, cb, data);
}

static bool pbvh_faces_node_nearest_to_ray(PBVH *pbvh,
                                           const PBVHNode *node,
                                           float (*origco)[3],
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  const MVert *vert = pbvh->verts;
  const MLoop *mloop = pbvh->mloop;
  const int *faces = node->prim_indices;
  int i, totface = node->totprim;
  bool hit = false;

  for (i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (pbvh->respect_hide && paint_is_face_hidden(lt, vert, mloop)) {
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

static bool pbvh_grids_node_nearest_to_ray(PBVH *pbvh,
                                           PBVHNode *node,
                                           float (*origco)[3],
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  const int totgrid = node->totprim;
  const int gridsize = pbvh->gridkey.grid_size;
  bool hit = false;

  for (int i = 0; i < totgrid; i++) {
    CCGElem *grid = pbvh->grids[node->prim_indices[i]];
    BLI_bitmap *gh;

    if (!grid) {
      continue;
    }

    gh = pbvh->grid_hidden[node->prim_indices[i]];

    for (int y = 0; y < gridsize - 1; y++) {
      for (int x = 0; x < gridsize - 1; x++) {
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
                                       CCG_grid_elem_co(&pbvh->gridkey, grid, x, y),
                                       CCG_grid_elem_co(&pbvh->gridkey, grid, x + 1, y),
                                       CCG_grid_elem_co(&pbvh->gridkey, grid, x + 1, y + 1),
                                       CCG_grid_elem_co(&pbvh->gridkey, grid, x, y + 1),
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

bool BKE_pbvh_node_find_nearest_to_ray(PBVH *pbvh,
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

  switch (pbvh->type) {
    case PBVH_FACES:
      hit |= pbvh_faces_node_nearest_to_ray(
          pbvh, node, origco, ray_start, ray_normal, depth, dist_sq);
      break;
    case PBVH_GRIDS:
      hit |= pbvh_grids_node_nearest_to_ray(
          pbvh, node, origco, ray_start, ray_normal, depth, dist_sq);
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
static PlaneAABBIsect test_frustum_aabb(const float bb_min[3],
                                        const float bb_max[3],
                                        PBVHFrustumPlanes *frustum)
{
  PlaneAABBIsect ret = ISECT_INSIDE;
  float(*planes)[4] = frustum->planes;

  for (int i = 0; i < frustum->num_planes; i++) {
    float vmin[3], vmax[3];

    for (int axis = 0; axis < 3; axis++) {
      if (planes[i][axis] < 0) {
        vmin[axis] = bb_min[axis];
        vmax[axis] = bb_max[axis];
      }
      else {
        vmin[axis] = bb_max[axis];
        vmax[axis] = bb_min[axis];
      }
    }

    if (dot_v3v3(planes[i], vmin) + planes[i][3] < 0) {
      return ISECT_OUTSIDE;
    }
    else if (dot_v3v3(planes[i], vmax) + planes[i][3] <= 0) {
      ret = ISECT_INTERSECT;
    }
  }

  return ret;
}

bool BKE_pbvh_node_frustum_contain_AABB(PBVHNode *node, void *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.bmin;
  bb_max = node->vb.bmax;

  return test_frustum_aabb(bb_min, bb_max, data) != ISECT_OUTSIDE;
}

bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, void *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.bmin;
  bb_max = node->vb.bmax;

  return test_frustum_aabb(bb_min, bb_max, data) != ISECT_INSIDE;
}

void BKE_pbvh_update_normals(PBVH *pbvh, struct SubdivCCG *subdiv_ccg)
{
  /* Update normals */
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateNormals), &nodes, &totnode);

  if (totnode > 0) {
    if (pbvh->type == PBVH_BMESH) {
      pbvh_bmesh_normals_update(nodes, totnode);
    }
    else if (pbvh->type == PBVH_FACES) {
      pbvh_faces_update_normals(pbvh, nodes, totnode);
    }
    else if (pbvh->type == PBVH_GRIDS) {
      struct CCGFace **faces;
      int num_faces;
      BKE_pbvh_get_grid_updates(pbvh, true, (void ***)&faces, &num_faces);
      if (num_faces > 0) {
        BKE_subdiv_ccg_update_normals(subdiv_ccg, faces, num_faces);
        MEM_freeN(faces);
      }
    }
  }

  MEM_SAFE_FREE(nodes);
}

void BKE_pbvh_face_sets_color_set(PBVH *pbvh, int seed, int color_default)
{
  pbvh->face_sets_color_seed = seed;
  pbvh->face_sets_color_default = color_default;
}

/**
 * PBVH drawing, updating draw buffers as needed and culling any nodes outside
 * the specified frustum.
 */
typedef struct PBVHDrawSearchData {
  PBVHFrustumPlanes *frustum;
  int accum_update_flag;
} PBVHDrawSearchData;

static bool pbvh_draw_search_cb(PBVHNode *node, void *data_v)
{
  PBVHDrawSearchData *data = data_v;
  if (data->frustum && !BKE_pbvh_node_frustum_contain_AABB(node, data->frustum)) {
    return false;
  }

  data->accum_update_flag |= node->flag;
  return true;
}

void BKE_pbvh_draw_cb(PBVH *pbvh,
                      bool update_only_visible,
                      PBVHFrustumPlanes *update_frustum,
                      PBVHFrustumPlanes *draw_frustum,
                      void (*draw_fn)(void *user_data, GPU_PBVH_Buffers *buffers),
                      void *user_data)
{
  PBVHNode **nodes;
  int totnode;

  const int update_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;

  if (!update_only_visible) {
    /* Update all draw buffers, also those outside the view. */
    BKE_pbvh_search_gather(
        pbvh, update_search_cb, POINTER_FROM_INT(update_flag), &nodes, &totnode);

    if (totnode) {
      pbvh_update_draw_buffers(pbvh, nodes, totnode, update_flag);
    }

    MEM_SAFE_FREE(nodes);
  }

  /* Gather visible nodes. */
  PBVHDrawSearchData data = {.frustum = update_frustum, .accum_update_flag = 0};
  BKE_pbvh_search_gather(pbvh, pbvh_draw_search_cb, &data, &nodes, &totnode);

  if (update_only_visible && (data.accum_update_flag & update_flag)) {
    /* Update draw buffers in visible nodes. */
    pbvh_update_draw_buffers(pbvh, nodes, totnode, data.accum_update_flag);
  }

  /* Draw. */
  for (int a = 0; a < totnode; a++) {
    PBVHNode *node = nodes[a];

    if (node->flag & PBVH_UpdateDrawBuffers) {
      /* Flush buffers uses OpenGL, so not in parallel. */
      GPU_pbvh_buffers_update_flush(node->draw_buffers);
    }

    node->flag &= ~(PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers);
  }

  MEM_SAFE_FREE(nodes);

  PBVHDrawSearchData draw_data = {.frustum = draw_frustum, .accum_update_flag = 0};
  BKE_pbvh_search_gather(pbvh, pbvh_draw_search_cb, &draw_data, &nodes, &totnode);

  for (int a = 0; a < totnode; a++) {
    PBVHNode *node = nodes[a];
    if (!(node->flag & PBVH_FullyHidden)) {
      draw_fn(user_data, node->draw_buffers);
    }
  }

  MEM_SAFE_FREE(nodes);
}

void BKE_pbvh_draw_debug_cb(
    PBVH *pbvh,
    void (*draw_fn)(void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag),
    void *user_data)
{
  for (int a = 0; a < pbvh->totnode; a++) {
    PBVHNode *node = &pbvh->nodes[a];

    draw_fn(user_data, node->vb.bmin, node->vb.bmax, node->flag);
  }
}

void BKE_pbvh_grids_update(
    PBVH *pbvh, CCGElem **grids, void **gridfaces, DMFlagMat *flagmats, BLI_bitmap **grid_hidden)
{
  pbvh->grids = grids;
  pbvh->gridfaces = gridfaces;

  if (flagmats != pbvh->grid_flag_mats || pbvh->grid_hidden != grid_hidden) {
    pbvh->grid_flag_mats = flagmats;
    pbvh->grid_hidden = grid_hidden;

    for (int a = 0; a < pbvh->totnode; a++) {
      BKE_pbvh_node_mark_rebuild_draw(&pbvh->nodes[a]);
    }
  }
}

float (*BKE_pbvh_vert_coords_alloc(PBVH *pbvh))[3]
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

void BKE_pbvh_vert_coords_apply(PBVH *pbvh, const float (*vertCos)[3], const int totvert)
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
    for (int a = 0; a < pbvh->totvert; a++, mvert++) {
      /* no need for float comparison here (memory is exactly equal or not) */
      if (memcmp(mvert->co, vertCos[a], sizeof(float[3])) != 0) {
        copy_v3_v3(mvert->co, vertCos[a]);
        mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }

    /* coordinates are new -- normals should also be updated */
    BKE_mesh_calc_normals_looptri(
        pbvh->verts, pbvh->totvert, pbvh->mloop, pbvh->looptri, pbvh->totprim, NULL);

    for (int a = 0; a < pbvh->totnode; a++) {
      BKE_pbvh_node_mark_update(&pbvh->nodes[a]);
    }

    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
  }
}

bool BKE_pbvh_is_deformed(PBVH *pbvh)
{
  return pbvh->deformed;
}
/* Proxies */

PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *pbvh, PBVHNode *node)
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

  BKE_pbvh_node_num_verts(pbvh, node, &totverts, NULL);
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

PBVHColorBufferNode *BKE_pbvh_node_color_buffer_get(PBVHNode *node)
{

  if (!node->color_buffer.color) {
    node->color_buffer.color = MEM_callocN(node->uniq_verts * sizeof(float) * 4, "Color buffer");
  }
  return &node->color_buffer;
}

void BKE_pbvh_node_color_buffer_free(PBVH *pbvh)
{
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  for (int i = 0; i < totnode; i++) {
    MEM_SAFE_FREE(nodes[i]->color_buffer.color);
  }
  MEM_SAFE_FREE(nodes);
}

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode)
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

  vi->respect_hide = pbvh->respect_hide;
  if (pbvh->respect_hide == false) {
    /* The same value for all vertices. */
    vi->visible = true;
  }

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids);
  BKE_pbvh_node_num_verts(pbvh, node, &uniq_verts, &totvert);
  BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &verts);
  vi->key = pbvh->gridkey;

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

  if (pbvh->type == PBVH_BMESH) {
    BLI_gsetIterator_init(&vi->bm_unique_verts, node->bm_unique_verts);
    BLI_gsetIterator_init(&vi->bm_other_verts, node->bm_other_verts);
    vi->bm_vdata = &pbvh->bm->vdata;
    vi->cd_vert_mask_offset = CustomData_get_offset(vi->bm_vdata, CD_PAINT_MASK);
  }

  vi->gh = NULL;
  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = pbvh->grid_hidden;
  }

  vi->mask = NULL;
  if (pbvh->type == PBVH_FACES) {
    vi->vmask = CustomData_get_layer(pbvh->vdata, CD_PAINT_MASK);
    vi->vcol = CustomData_get_layer(pbvh->vdata, CD_PROP_COLOR);
  }
}

bool pbvh_has_mask(PBVH *pbvh)
{
  switch (pbvh->type) {
    case PBVH_GRIDS:
      return (pbvh->gridkey.has_mask != 0);
    case PBVH_FACES:
      return (pbvh->vdata && CustomData_get_layer(pbvh->vdata, CD_PAINT_MASK));
    case PBVH_BMESH:
      return (pbvh->bm && (CustomData_get_offset(&pbvh->bm->vdata, CD_PAINT_MASK) != -1));
  }

  return false;
}

bool pbvh_has_face_sets(PBVH *pbvh)
{
  switch (pbvh->type) {
    case PBVH_GRIDS:
      return (pbvh->pdata && CustomData_get_layer(pbvh->pdata, CD_SCULPT_FACE_SETS));
    case PBVH_FACES:
      return (pbvh->pdata && CustomData_get_layer(pbvh->pdata, CD_SCULPT_FACE_SETS));
    case PBVH_BMESH:
      return false;
  }

  return false;
}

void pbvh_show_mask_set(PBVH *pbvh, bool show_mask)
{
  pbvh->show_mask = show_mask;
}

void pbvh_show_face_sets_set(PBVH *pbvh, bool show_face_sets)
{
  pbvh->show_face_sets = show_face_sets;
}

void BKE_pbvh_set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes)
{
  pbvh->num_planes = planes->num_planes;
  for (int i = 0; i < pbvh->num_planes; i++) {
    copy_v4_v4(pbvh->planes[i], planes->planes[i]);
  }
}

void BKE_pbvh_get_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes)
{
  planes->num_planes = pbvh->num_planes;
  for (int i = 0; i < planes->num_planes; i++) {
    copy_v4_v4(planes->planes[i], pbvh->planes[i]);
  }
}

void BKE_pbvh_parallel_range_settings(TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode)
{
  memset(settings, 0, sizeof(*settings));
  settings->use_threading = use_threading && totnode > 1;
}

MVert *BKE_pbvh_get_verts(const PBVH *pbvh)
{
  BLI_assert(pbvh->type == PBVH_FACES);
  return pbvh->verts;
}

void BKE_pbvh_subdiv_cgg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  pbvh->subdiv_ccg = subdiv_ccg;
}

void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets)
{
  pbvh->face_sets = face_sets;
}

void BKE_pbvh_respect_hide_set(PBVH *pbvh, bool respect_hide)
{
  pbvh->respect_hide = respect_hide;
}
