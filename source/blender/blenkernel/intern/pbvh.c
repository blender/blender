/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_main.h"
#include "BKE_mesh.h" /* for BKE_mesh_calc_normals */
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph_query.h"

#include "DRW_pbvh.h"
#include "PIL_time.h"

#include "PIL_time.h"

#include "bmesh.h"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

#include <limits.h>

#define LEAF_LIMIT 4000

/* Uncomment to test if triangles of the same face are
 * properly clustered into single nodes.
 */
//#define TEST_PBVH_FACE_SPLIT

/* Uncomment to test that faces are only assigned to one PBVHNode */
//#define VALIDATE_UNIQUE_NODE_FACES

//#define PERFCNTRS

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

  PBVHStack stackfixed[PBVH_STACK_FIXED_DEPTH];
  int stackspace;
} PBVHIter;

void BB_reset(BB *bb)
{
  bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
  bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

void BB_intersect(BB *r_out, BB *a, BB *b)
{
  for (int i = 0; i < 3; i++) {
    r_out->bmin[i] = max_ff(a->bmin[i], b->bmin[i]);
    r_out->bmax[i] = min_ff(a->bmax[i], b->bmax[i]);

    if (r_out->bmax[i] < r_out->bmin[i]) {
      r_out->bmax[i] = r_out->bmin[i] = 0.0f;
    }
  }
}

float BB_volume(const BB *bb)
{
  float dx = bb->bmax[0] - bb->bmin[0];
  float dy = bb->bmax[1] - bb->bmin[1];
  float dz = bb->bmax[2] - bb->bmin[2];

  return dx * dy * dz;
}

/* Expand the bounding box to include a new coordinate */
void BB_expand(BB *bb, const float co[3])
{
  for (int i = 0; i < 3; i++) {
    bb->bmin[i] = min_ff(bb->bmin[i], co[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], co[i]);
  }
}

void BB_expand_with_bb(BB *bb, BB *bb2)
{
  for (int i = 0; i < 3; i++) {
    bb->bmin[i] = min_ff(bb->bmin[i], bb2->bmin[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], bb2->bmax[i]);
  }
}

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

    return 2;
  }

  if (dim[1] > dim[2]) {
    return 1;
  }

  return 2;
}

void BBC_update_centroid(BBC *bbc)
{
  for (int i = 0; i < 3; i++) {
    bbc->bcentroid[i] = (bbc->bmin[i] + bbc->bmax[i]) * 0.5f;
  }
}

/* Not recursive */
static void update_node_vb(PBVH *pbvh, PBVHNode *node, int updateflag)
{
  if (!(updateflag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB))) {
    return;
  }

  /* cannot clear flag here, causes leaky pbvh */
  // node->flag &= ~(updateflag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB));

  BB vb;
  BB orig_vb;

  BB_reset(&vb);
  BB_reset(&orig_vb);

  bool do_orig = true;    // XXX updateflag & PBVH_UpdateOriginalBB;
  bool do_normal = true;  // XXX updateflag & PBVH_UpdateBB;

  if (node->flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_ALL) {
      if (do_normal) {
        BB_expand(&vb, vd.co);
      }

      if (do_orig) {
        float *origco = nullptr;

        if (pbvh->header.type == PBVH_BMESH) {
          origco = pbvh->cd_origco != -1 ? BM_ELEM_CD_PTR<float *>(vd.bm_vert, pbvh->cd_origco) :
                                           nullptr;
        }
        else {
          origco = pbvh->origco
        }

        if (origco) {
          BB_expand(&orig_vb, vd.co);
        }
        else {
          BB_expand(&orig_vb, mv->origco);
        }
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
  else {
    if (do_normal) {
      BB_expand_with_bb(&vb, &pbvh->nodes[node->children_offset].vb);
      BB_expand_with_bb(&vb, &pbvh->nodes[node->children_offset + 1].vb);
    }

    if (do_orig) {
      BB_expand_with_bb(&orig_vb, &pbvh->nodes[node->children_offset].orig_vb);
      BB_expand_with_bb(&orig_vb, &pbvh->nodes[node->children_offset + 1].orig_vb);
    }
  }

  if (do_normal) {
    node->vb = vb;
  }

  if (do_orig) {
#if 0
    float size[3];

    sub_v3_v3v3(size, orig_vb.bmax, orig_vb.bmin);
    mul_v3_fl(size, 0.05);

    sub_v3_v3(orig_vb.bmin, size);
    add_v3_v3(orig_vb.bmax, size);
#endif
    node->orig_vb = orig_vb;
  }
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

static bool face_materials_match(const PBVH *pbvh, const int a, const int b)
{
  if (pbvh->material_indices) {
    if (pbvh->material_indices[a] != pbvh->material_indices[b]) {
      return false;
    }
  }
  return (pbvh->mpoly[a].flag & ME_SMOOTH) == (pbvh->mpoly[b].flag & ME_SMOOTH);
}

static bool grid_materials_match(const DMFlagMat *f1, const DMFlagMat *f2)
{
  return ((f1->flag & ME_SMOOTH) == (f2->flag & ME_SMOOTH) && (f1->mat_nr == f2->mat_nr));
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices_faces(int *prim_indices,
                                   int *prim_scratch,
                                   int lo,
                                   int hi,
                                   int axis,
                                   float mid,
                                   BBC *prim_bbc,
                                   const MLoopTri *looptri)
{
  for (int i = lo; i < hi; i++) {
    prim_scratch[i - lo] = prim_indices[i];
  }

  int lo2 = lo, hi2 = hi - 1;
  int i1 = lo, i2 = 0;

  while (i1 < hi) {
    int poly = looptri[prim_scratch[i2]].poly;
    bool side = prim_bbc[prim_scratch[i2]].bcentroid[axis] >= mid;

    while (i1 < hi && looptri[prim_scratch[i2]].poly == poly) {
      prim_indices[side ? hi2-- : lo2++] = prim_scratch[i2];
      i1++;
      i2++;
    }
  }

  return lo2;
}

static int partition_indices_grids(int *prim_indices,
                                   int *prim_scratch,
                                   int lo,
                                   int hi,
                                   int axis,
                                   float mid,
                                   BBC *prim_bbc,
                                   SubdivCCG *subdiv_ccg)
{
  for (int i = lo; i < hi; i++) {
    prim_scratch[i - lo] = prim_indices[i];
  }

  int lo2 = lo, hi2 = hi - 1;
  int i1 = lo, i2 = 0;

  while (i1 < hi) {
    int poly = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, prim_scratch[i2]);
    bool side = prim_bbc[prim_scratch[i2]].bcentroid[axis] >= mid;

    while (i1 < hi && BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, prim_scratch[i2]) == poly) {
      prim_indices[side ? hi2-- : lo2++] = prim_scratch[i2];
      i1++;
      i2++;
    }
  }

  return lo2;
}

/* Returns the index of the first element on the right of the partition */
static int partition_indices_material(PBVH *pbvh, int lo, int hi)
{
  const MLoopTri *looptri = pbvh->looptri;
  const DMFlagMat *flagmats = pbvh->grid_flag_mats;
  const int *indices = pbvh->prim_indices;
  int i = lo, j = hi;

  for (;;) {
    if (pbvh->looptri) {
      const int first = looptri[pbvh->prim_indices[lo]].poly;
      for (; face_materials_match(pbvh, first, looptri[indices[i]].poly); i++) {
        /* pass */
      }
      for (; !face_materials_match(pbvh, first, looptri[indices[j]].poly); j--) {
        /* pass */
      }
    }
    else {
      const DMFlagMat *first = &flagmats[pbvh->prim_indices[lo]];
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

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!node->id) {
      node->id = ++pbvh->idgen;
    }
  }
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(PBVH *pbvh, GHash *map, uint *face_verts, uint *uniq_verts, int vertex)
{
  void *key, **value_p;

  key = POINTER_FROM_INT(vertex);
  if (!BLI_ghash_ensure_p(map, key, &value_p)) {
    int value_i;
    if (!pbvh->vert_bitmap[vertex]) {
      pbvh->vert_bitmap[vertex] = true;
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

  return POINTER_AS_INT(*value_p);
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
      if (!paint_is_face_hidden(lt, pbvh->hide_poly)) {
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
  BKE_pbvh_vert_tag_update_normal_tri_area(node);

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

int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              const int *grid_indices,
                              int totgrid,
                              int gridsize,
                              int display_gridsize)
{
  const int gridarea = (gridsize - 1) * (gridsize - 1);
  int totquad = 0;

  /* grid hidden layer is present, so have to check each grid for
   * visibility */

  int depth1 = (int)(log2((double)gridsize - 1.0) + DBL_EPSILON);
  int depth2 = (int)(log2((double)display_gridsize - 1.0) + DBL_EPSILON);

  int skip = depth2 < depth1 ? 1 << (depth1 - depth2 - 1) : 1;

  for (int i = 0; i < totgrid; i++) {
    const BLI_bitmap *gh = grid_hidden[grid_indices[i]];

    if (gh) {
      /* grid hidden are present, have to check each element */
      for (int y = 0; y < gridsize - skip; y += skip) {
        for (int x = 0; x < gridsize - skip; x += skip) {
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

static void build_grid_leaf_node(PBVH *pbvh, PBVHNode *node)
{
  int totquads = BKE_pbvh_count_grid_quads(pbvh->grid_hidden,
                                           node->prim_indices,
                                           node->totprim,
                                           pbvh->gridkey.grid_size,
                                           pbvh->gridkey.grid_size);
  BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
  BKE_pbvh_node_mark_rebuild_draw(node);
  BKE_pbvh_vert_tag_update_normal_tri_area(node);
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
    for (int i = offset + count - 1; i > offset; i--) {
      int prim = pbvh->prim_indices[i];
      if (!face_materials_match(pbvh, first->poly, pbvh->looptri[prim].poly)) {
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

#ifdef TEST_PBVH_FACE_SPLIT
static void test_face_boundaries(PBVH *pbvh)
{
  int faces_num = BKE_pbvh_num_faces(pbvh);
  int *node_map = MEM_calloc_arrayN(faces_num, sizeof(int), __func__);
  for (int i = 0; i < faces_num; i++) {
    node_map[i] = -1;
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    switch (BKE_pbvh_type(pbvh)) {
      case PBVH_FACES: {
        for (int j = 0; j < node->totprim; j++) {
          int poly = pbvh->looptri[node->prim_indices[j]].poly;

          if (node_map[poly] >= 0 && node_map[poly] != i) {
            int old_i = node_map[poly];
            int prim_i = node->prim_indices - pbvh->prim_indices + j;

            printf("PBVH split error; poly: %d, prim_i: %d, node1: %d, node2: %d, totprim: %d\n",
                   poly,
                   prim_i,
                   old_i,
                   i,
                   node->totprim);
          }

          node_map[poly] = i;
        }
        break;
      }
      case PBVH_GRIDS:
        break;
      case PBVH_BMESH:
        break;
    }
  }

  MEM_SAFE_FREE(node_map);
}
#endif

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

static void build_sub(PBVH *pbvh,
                      int node_index,
                      BB *cb,
                      BBC *prim_bbc,
                      int offset,
                      int count,
                      int *prim_scratch,
                      int depth)
{
  int end;
  BB cb_backing;

  if (!prim_scratch) {
    prim_scratch = MEM_malloc_arrayN(pbvh->totprim, sizeof(int), __func__);
  }

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = count <= pbvh->leaf_limit || depth == PBVH_STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(pbvh, offset, count)) {
      build_leaf(pbvh, node_index, prim_bbc, offset, count);

      if (node_index == 0) {
        MEM_SAFE_FREE(prim_scratch);
      }

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
    if (pbvh->header.type == PBVH_FACES) {
      end = partition_indices_faces(pbvh->prim_indices,
                                    prim_scratch,
                                    offset,
                                    offset + count,
                                    axis,
                                    (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
                                    prim_bbc,
                                    pbvh->looptri);
    }
    else {
      end = partition_indices_grids(pbvh->prim_indices,
                                    prim_scratch,
                                    offset,
                                    offset + count,
                                    axis,
                                    (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
                                    prim_bbc,
                                    pbvh->subdiv_ccg);
    }
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material(pbvh, offset, offset + count - 1);
  }

  /* Build children */
  build_sub(pbvh,
            pbvh->nodes[node_index].children_offset,
            NULL,
            prim_bbc,
            offset,
            end - offset,
            prim_scratch,
            depth + 1);
  build_sub(pbvh,
            pbvh->nodes[node_index].children_offset + 1,
            NULL,
            prim_bbc,
            end,
            offset + count - end,
            prim_scratch,
            depth + 1);

  if (node_index == 0) {
    MEM_SAFE_FREE(prim_scratch);
  }
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
  build_sub(pbvh, 0, cb, prim_bbc, 0, totprim, NULL, 0);
}

void BKE_pbvh_set_face_areas(PBVH *pbvh, float *face_areas)
{
  pbvh->face_areas = face_areas;
}

/* XXX investigate this global. */
bool pbvh_show_orig_co;

static void pbvh_draw_args_init(PBVH *pbvh, PBVH_GPU_Args *args, PBVHNode *node)
{
  memset((void *)args, 0, sizeof(*args));

  args->pbvh_type = pbvh->header.type;
  args->mesh_verts_num = pbvh->totvert;
  args->mesh_grids_num = pbvh->totgrid;
  args->node = node;

  BKE_pbvh_node_num_verts(pbvh, node, NULL, &args->node_verts_num);

  args->grid_hidden = pbvh->grid_hidden;
  args->face_sets_color_default = pbvh->face_sets_color_default;
  args->face_sets_color_seed = pbvh->face_sets_color_seed;
  args->vert_positions = pbvh->vert_positions;
  args->mloop = pbvh->mloop;
  args->mpoly = pbvh->mpoly;
  args->mlooptri = pbvh->looptri;
  args->flat_vcol_shading = pbvh->flat_vcol_shading;
  args->show_orig = pbvh_show_orig_co;
  args->updategen = node->updategen;
  args->msculptverts = pbvh->msculptverts;

  if (ELEM(pbvh->header.type, PBVH_FACES, PBVH_GRIDS)) {
    args->hide_poly = pbvh->pdata ?
                          CustomData_get_layer_named(pbvh->pdata, CD_PROP_BOOL, ".hide_poly") :
                          NULL;
  }

  switch (pbvh->header.type) {
    case PBVH_FACES:
      args->mesh_faces_num = pbvh->mesh->totpoly;
      args->vdata = pbvh->vdata;
      args->ldata = pbvh->ldata;
      args->pdata = pbvh->pdata;
      args->totprim = node->totprim;
      args->me = pbvh->mesh;
      args->mpoly = pbvh->mpoly;
      args->vert_normals = pbvh->vert_normals;

      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->prim_indices = node->prim_indices;
      args->face_sets = pbvh->face_sets;
      break;
    case PBVH_GRIDS:
      args->vdata = pbvh->vdata;
      args->ldata = pbvh->ldata;
      args->pdata = pbvh->pdata;
      args->ccg_key = pbvh->gridkey;
      args->me = pbvh->mesh;
      args->totprim = node->totprim;
      args->grid_indices = node->prim_indices;
      args->subdiv_ccg = pbvh->subdiv_ccg;
      args->face_sets = pbvh->face_sets;
      args->mpoly = pbvh->mpoly;

      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->mesh_grids_num = pbvh->totgrid;
      args->grids = pbvh->grids;
      args->gridfaces = pbvh->gridfaces;
      args->grid_flag_mats = pbvh->grid_flag_mats;
      args->vert_normals = pbvh->vert_normals;

      args->face_sets = pbvh->face_sets;
      break;
    case PBVH_BMESH:
      args->bm = pbvh->header.bm;

      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->me = pbvh->mesh;
      args->vdata = &args->bm->vdata;
      args->ldata = &args->bm->ldata;
      args->pdata = &args->bm->pdata;
      args->bm_faces = node->bm_faces;
      args->bm_other_verts = node->bm_other_verts;
      args->bm_unique_vert = node->bm_unique_verts;
      args->totprim = BLI_table_gset_len(node->bm_faces);
      args->cd_mask_layer = CustomData_get_offset(&pbvh->header.bm->vdata, CD_PAINT_MASK);

      args->tribuf = node->tribuf;
      args->tri_buffers = node->tri_buffers;
      args->tot_tri_buffers = node->tot_tri_buffers;

      break;
  }
}

#ifdef VALIDATE_UNIQUE_NODE_FACES
static void pbvh_validate_node_prims(PBVH *pbvh)
{
  int totface = 0;

  if (pbvh->header.type == PBVH_BMESH) {
    return;
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    for (int j = 0; j < node->totprim; j++) {
      int poly;

      if (pbvh->header.type == PBVH_FACES) {
        poly = pbvh->looptri[node->prim_indices[j]].poly;
      }
      else {
        poly = BKE_subdiv_ccg_grid_to_face_index(pbvh->subdiv_ccg, node->prim_indices[j]);
      }

      totface = max_ii(totface, poly + 1);
    }
  }

  int *facemap = (int *)MEM_malloc_arrayN(totface, sizeof(*facemap), __func__);

  for (int i = 0; i < totface; i++) {
    facemap[i] = -1;
  }

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    for (int j = 0; j < node->totprim; j++) {
      int poly;

      if (pbvh->header.type == PBVH_FACES) {
        poly = pbvh->looptri[node->prim_indices[j]].poly;
      }
      else {
        poly = BKE_subdiv_ccg_grid_to_face_index(pbvh->subdiv_ccg, node->prim_indices[j]);
      }

      if (facemap[poly] != -1 && facemap[poly] != i) {
        printf("%s: error: face spanned multiple nodes (old: %d new: %d)\n",
               __func__,
               facemap[poly],
               i);
      }

      facemap[poly] = i;
    }
  }
  MEM_SAFE_FREE(facemap);
}
#endif

void BKE_pbvh_build_mesh(PBVH *pbvh,
                         Mesh *mesh,
                         const MPoly *mpoly,
                         const MLoop *mloop,
                         float (*vert_positions)[3],
                         MSculptVert *msculptverts,
                         int totvert,
                         struct CustomData *vdata,
                         struct CustomData *ldata,
                         struct CustomData *pdata,
                         const MLoopTri *looptri,
                         int looptri_num,
                         bool fast_draw,
                         float *face_areas,
                         SculptPMap *pmap)
{
  BBC *prim_bbc = NULL;
  BB cb;

  if (pbvh->pmap != pmap) {
    BKE_pbvh_pmap_aquire(pmap);
  }

  pbvh->pmap = pmap;
  pbvh->face_areas = face_areas;
  pbvh->mesh = mesh;
  pbvh->header.type = PBVH_FACES;
  pbvh->mpoly = mpoly;
  pbvh->hide_poly = (bool *)CustomData_get_layer_named_for_write(
      &mesh->pdata, CD_PROP_BOOL, ".hide_poly", mesh->totpoly);
  pbvh->material_indices = (const int *)CustomData_get_layer_named(
      &mesh->pdata, CD_PROP_INT32, "material_index");
  pbvh->mloop = mloop;
  pbvh->looptri = looptri;
  pbvh->msculptverts = msculptverts;
  pbvh->vert_positions = vert_positions;
  BKE_mesh_vertex_normals_ensure(mesh);
  pbvh->vert_normals = BKE_mesh_vertex_normals_for_write(mesh);
  pbvh->hide_vert = (bool *)CustomData_get_layer_named_for_write(
      &mesh->vdata, CD_PROP_BOOL, ".hide_vert", mesh->totvert);
  pbvh->vert_bitmap = MEM_calloc_arrayN(totvert, sizeof(bool), "bvh->vert_bitmap");
  pbvh->totvert = totvert;

#ifdef TEST_PBVH_FACE_SPLIT
  /* Use lower limit to increase probability of
   * edge cases.
   */
  pbvh->leaf_limit = 100;
#else
  pbvh->leaf_limit = LEAF_LIMIT;
#endif

  pbvh->vdata = vdata;
  pbvh->ldata = ldata;
  pbvh->pdata = pdata;
  pbvh->faces_num = mesh->totpoly;

  pbvh->face_sets_color_seed = mesh->face_sets_color_seed;
  pbvh->face_sets_color_default = mesh->face_sets_color_default;

  BB_reset(&cb);

  /* For each face, store the AABB and the AABB centroid */
  prim_bbc = MEM_mallocN(sizeof(BBC) * looptri_num, "prim_bbc");

  for (int i = 0; i < mesh->totvert; i++) {
    msculptverts[i].flag &= ~SCULPTFLAG_NEED_VALENCE;
    msculptverts[i].valence = pmap->pmap[i].count;
  }

  for (int i = 0; i < looptri_num; i++) {
    const MLoopTri *lt = &looptri[i];
    const int sides = 3;
    BBC *bbc = prim_bbc + i;

    BB_reset((BB *)bbc);

    for (int j = 0; j < sides; j++) {
      BB_expand((BB *)bbc, vert_positions[pbvh->mloop[lt->tri[j]].v]);
    }

    BBC_update_centroid(bbc);

    BB_expand(&cb, bbc->bcentroid);
  }

  if (looptri_num) {
    pbvh_build(pbvh, &cb, prim_bbc, looptri_num);

#ifdef TEST_PBVH_FACE_SPLIT
    test_face_boundaries(pbvh);
#endif
  }

  if (fast_draw) {
    pbvh->flags |= PBVH_FAST_DRAW;
  }

  MEM_freeN(prim_bbc);

  /* Clear the bitmap so it can be used as an update tag later on. */
  memset(pbvh->vert_bitmap, 0, sizeof(bool) * totvert);

  BKE_pbvh_update_active_vcol(pbvh, mesh);

#ifdef VALIDATE_UNIQUE_NODE_FACES
  pbvh_validate_node_prims(pbvh);
#endif
}

void BKE_pbvh_build_grids(PBVH *pbvh,
                          CCGElem **grids,
                          int totgrid,
                          CCGKey *key,
                          void **gridfaces,
                          DMFlagMat *flagmats,
                          BLI_bitmap **grid_hidden,
                          bool fast_draw,
                          float *face_areas,
                          Mesh *me,
                          SubdivCCG *subdiv_ccg)
{
  const int gridsize = key->grid_size;

  pbvh->header.type = PBVH_GRIDS;
  pbvh->face_areas = face_areas;
  pbvh->grids = grids;
  pbvh->gridfaces = gridfaces;
  pbvh->grid_flag_mats = flagmats;
  pbvh->totgrid = totgrid;
  pbvh->gridkey = *key;
  pbvh->grid_hidden = grid_hidden;
  pbvh->subdiv_ccg = subdiv_ccg;
  pbvh->faces_num = me->totpoly;

  /* Find maximum number of grids per face. */
  int max_grids = 1;
  const MPoly *mpoly = BKE_mesh_polys(me);

  for (int i = 0; i < me->totpoly; i++) {
    max_grids = max_ii(max_grids, mpoly[i].totloop);
  }

  /* Ensure leaf limit is at least 4 so there's room
   * to split at original face boundaries.
   * Fixes T102209.
   */
  pbvh->leaf_limit = max_ii(LEAF_LIMIT / (gridsize * gridsize), max_grids);

  /* We need the base mesh attribute layout for PBVH draw. */
  pbvh->vdata = &me->vdata;
  pbvh->ldata = &me->ldata;
  pbvh->pdata = &me->pdata;

  pbvh->mpoly = BKE_mesh_polys(me);
  pbvh->mloop = BKE_mesh_loops(me);

  /* We also need the base mesh for PBVH draw. */
  pbvh->mesh = me;

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

#ifdef TEST_PBVH_FACE_SPLIT
    test_face_boundaries(pbvh);
#endif
  }

  if (fast_draw) {
    pbvh->flags |= PBVH_FAST_DRAW;
  }

  MEM_freeN(prim_bbc);
#ifdef VALIDATE_UNIQUE_NODE_FACES
  pbvh_validate_node_prims(pbvh);
#endif
}

PBVH *BKE_pbvh_new(PBVHType type)
{
  PBVH *pbvh = MEM_callocN(sizeof(PBVH), "pbvh");
  pbvh->respect_hide = true;
  pbvh->draw_cache_invalid = true;
  pbvh->header.type = type;

  /* Initialize this to true, instead of waiting for a draw engine
   * to set it.  Prevents a crash in draw manager instancing code.
   */
  pbvh->is_drawing = true;
  return pbvh;
}

void BKE_pbvh_free(PBVH *pbvh)
{
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = &pbvh->nodes[i];

    if (node->flag & PBVH_Leaf) {
      if (node->draw_batches) {
        DRW_pbvh_node_free(node->draw_batches);
      }
      if (node->vert_indices) {
        MEM_freeN((void *)node->vert_indices);
      }
      if (node->loop_indices) {
        MEM_freeN(node->loop_indices);
      }
      if (node->face_vert_indices) {
        MEM_freeN((void *)node->face_vert_indices);
      }
      if (node->bm_faces) {
        BLI_table_gset_free(node->bm_faces, NULL);
      }
      if (node->bm_unique_verts) {
        BLI_table_gset_free(node->bm_unique_verts, NULL);
      }
      if (node->bm_other_verts) {
        BLI_table_gset_free(node->bm_other_verts, NULL);
      }

      if (node->tribuf || node->tri_buffers) {
        BKE_pbvh_bmesh_free_tris(pbvh, node);
      }

#ifdef PROXY_ADVANCED
      BKE_pbvh_free_proxyarray(pbvh, node);
#endif
      pbvh_node_pixels_free(node);
    }
  }

  if (pbvh->deformed) {
    if (pbvh->vert_positions) {
      /* if pbvh was deformed, new memory was allocated for verts/faces -- free it */

      MEM_freeN((void *)pbvh->vert_positions);
    }

    pbvh->vert_positions = NULL;
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

  MEM_SAFE_FREE(pbvh->vert_bitmap);

  BKE_pbvh_pmap_release(pbvh->pmap);
  pbvh->pmap = NULL;

  pbvh->invalid = true;
  pbvh_pixels_free(pbvh);

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
  iter->stackspace = PBVH_STACK_FIXED_DEPTH;

  iter->stack[0].node = pbvh->nodes;
  iter->stack[0].revisiting = false;
  iter->stacksize = 1;
}

static void pbvh_iter_end(PBVHIter *iter)
{
  if (iter->stackspace > PBVH_STACK_FIXED_DEPTH) {
    MEM_freeN(iter->stack);
  }
}

static void pbvh_stack_push(PBVHIter *iter, PBVHNode *node, bool revisiting)
{
  if (UNLIKELY(iter->stacksize == iter->stackspace)) {
    iter->stackspace *= 2;
    if (iter->stackspace != (PBVH_STACK_FIXED_DEPTH * 2)) {
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

static PBVHNode *pbvh_iter_next(PBVHIter *iter, PBVHNodeFlags leaf_flag = PBVH_Leaf)
{
  /* purpose here is to traverse tree, visiting child nodes beforse their
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

    if (node->flag & leaf_flag) {
      /* immediately hit leaf node */
      return node;
    }

    /* come back later when children are done */
    pbvh_stack_push(iter, node, true);

    /* push two child nodes on the stack */
    pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset + 1, false);
    pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset, false);
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

    float ff = dot_v3v3(node->vb.bmin, node->vb.bmax);
    if (isnan(ff) || !isfinite(ff)) {
      printf("%s: nan!\n", __func__);
    }

    if (iter->scb && !iter->scb(node, iter->search_data)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag & PBVH_Leaf) {
      /* immediately hit leaf node */
      return node;
    }

    pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset + 1, false);
    pbvh_stack_push(iter, iter->pbvh->nodes + node->children_offset, false);
  }

  return NULL;
}

void BKE_pbvh_search_gather_ex(PBVH *pbvh,
                               BKE_pbvh_SearchCallback scb,
                               void *search_data,
                               PBVHNode ***r_array,
                               int *r_tot,
                               PBVHNodeFlags leaf_flag)
{
  PBVHIter iter;
  PBVHNode **array = NULL, *node;
  int tot = 0, space = 0;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

  while ((node = pbvh_iter_next(&iter, leaf_flag))) {
    if (node->flag & leaf_flag) {
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

void BKE_pbvh_search_gather(
    PBVH *pbvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***r_array, int *r_tot)
{
  BKE_pbvh_search_gather_ex(pbvh, scb, search_data, r_array, r_tot, PBVH_Leaf);
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

  float (*vert_normals)[3];
  int flag;
  bool show_sculpt_face_sets;
  bool flat_vcol_shading;
  Mesh *mesh;
  PBVHAttrReq *attrs;
  int attrs_num;
} PBVHUpdateData;

static void pbvh_update_normals_clear_task_cb(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  float(*vert_normals)[3] = data->vert_normals;

  if (node->flag & PBVH_UpdateNormals) {
    const int *verts = node->vert_indices;
    const int totvert = node->uniq_verts;
    for (int i = 0; i < totvert; i++) {
      const int v = verts[i];
      if (pbvh->vert_bitmap[v]) {
        zero_v3(vert_normals[v]);
      }
    }
  }
}

static void pbvh_update_normals_accum_task_cb(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHUpdateData *data = userdata;

  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  float(*vert_normals)[3] = data->vert_normals;

  if (node->flag & PBVH_UpdateNormals) {
    uint mpoly_prev = UINT_MAX;
    float fn[3];

    const int *faces = node->prim_indices;
    const int totface = node->totprim;

    for (int i = 0; i < totface; i++) {
      const MLoopTri *lt = &pbvh->looptri[faces[i]];
      const uint vtri[3] = {
          pbvh->mloop[lt->tri[0]].v,
          pbvh->mloop[lt->tri[1]].v,
          pbvh->mloop[lt->tri[2]].v,
      };
      const int sides = 3;

      /* Face normal and mask */
      if (lt->poly != mpoly_prev) {
        const MPoly *mp = &pbvh->mpoly[lt->poly];
        BKE_mesh_calc_poly_normal(mp, &pbvh->mloop[mp->loopstart], pbvh->vert_positions, fn);
        mpoly_prev = lt->poly;
      }

      for (int j = sides; j--;) {
        const int v = vtri[j];

        if (pbvh->vert_bitmap[v]) {
          /* NOTE: This avoids `lock, add_v3_v3, unlock`
           * and is five to ten times quicker than a spin-lock.
           * Not exact equivalent though, since atomicity is only ensured for one component
           * of the vector at a time, but here it shall not make any sensible difference. */
          for (int k = 3; k--;) {
            atomic_add_and_fetch_fl(&vert_normals[v][k], fn[k]);
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
  float(*vert_normals)[3] = data->vert_normals;

  if (node->flag & PBVH_UpdateNormals) {
    const int *verts = node->vert_indices;
    const int totvert = node->uniq_verts;

    for (int i = 0; i < totvert; i++) {
      const int v = verts[i];

      /* No atomics necessary because we are iterating over uniq_verts only,
       * so we know only this thread will handle this vertex. */
      if (pbvh->vert_bitmap[v]) {
        normalize_v3(vert_normals[v]);
        pbvh->vert_bitmap[v] = false;
      }
    }

    node->flag &= ~PBVH_UpdateNormals;
  }
}

static void pbvh_faces_update_normals(PBVH *pbvh, PBVHNode **nodes, int totnode)
{
  /* subtle assumptions:
   * - We know that for all edited vertices, the nodes with faces
   *   adjacent to these vertices have been marked with PBVH_UpdateNormals.
   *   This is true because if the vertex is inside the brush radius, the
   *   bounding box of its adjacent faces will be as well.
   * - However this is only true for the vertices that have actually been
   *   edited, not for all vertices in the nodes marked for update, so we
   *   can only update vertices marked in the `vert_bitmap`.
   */

  PBVHUpdateData data = {
      .pbvh = pbvh,
      .nodes = nodes,
      .vert_normals = pbvh->vert_normals,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);

  /* Zero normals before accumulation. */
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_clear_task_cb, &settings);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_accum_task_cb, &settings);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_normals_store_task_cb, &settings);
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

      BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_ALL) {
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
      BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_ALL) {
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

  update_node_vb(pbvh, node, flag);

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

bool BKE_pbvh_get_color_layer(const Mesh *me, CustomDataLayer **r_layer, eAttrDomain *r_attr)
{
  CustomDataLayer *layer = BKE_id_attributes_color_find(&me->id, me->active_color_attribute);

  if (!layer || !ELEM(layer->type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
    *r_layer = NULL;
    *r_attr = ATTR_DOMAIN_POINT;
    return false;
  }

  eAttrDomain domain = BKE_id_attribute_domain(&me->id, layer);

  if (!ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
    *r_layer = NULL;
    *r_attr = ATTR_DOMAIN_POINT;
    return false;
  }

  *r_layer = layer;
  *r_attr = domain;

  return true;
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
  Mesh *me = data->mesh;

  CustomDataLayer *vcol_layer = NULL;
  eAttrDomain vcol_domain;

  BKE_pbvh_get_color_layer(pbvh, me, &vcol_layer, &vcol_domain);

  CustomData *vdata, *ldata;

  if (!pbvh->header.bm) {
    vdata = pbvh->vdata ? pbvh->vdata : &me->vdata;
    ldata = pbvh->ldata ? pbvh->ldata : &me->ldata;
  }
  else {
    vdata = &pbvh->header.bm->vdata;
    ldata = &pbvh->header.bm->ldata;
  }

  Mesh me_query;
  BKE_id_attribute_copy_domains_temp(ID_ME, vdata, NULL, ldata, NULL, NULL, &me_query.id);
  me_query.active_color_attribute = me->active_color_attribute;

  if (!pbvh->header.bm) {
    vdata = pbvh->vdata;
    ldata = pbvh->ldata;
  }
  else {
    vdata = &pbvh->header.bm->vdata;
    ldata = &pbvh->header.bm->ldata;
  }

  if (node->flag & PBVH_RebuildDrawBuffers) {
    PBVH_GPU_Args args;
    pbvh_draw_args_init(pbvh, &args, node);

    node->draw_batches = DRW_pbvh_node_create(&args);
  }

  if (node->flag & PBVH_UpdateDrawBuffers) {
    node->updategen++;
    node->debug_draw_gen++;

    if (node->draw_batches) {
      PBVH_GPU_Args args;

      pbvh_draw_args_init(pbvh, &args, node);
      DRW_pbvh_node_update(node->draw_batches, &args);
    }
  }
}

void BKE_pbvh_set_flat_vcol_shading(PBVH *pbvh, bool value)
{
  if (value != pbvh->flat_vcol_shading) {
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if (!(node->flag & PBVH_Leaf)) {
        continue;
      }

      BKE_pbvh_node_mark_rebuild_draw(node);
    }
  }

  pbvh->flat_vcol_shading = value;
}

void pbvh_free_draw_buffers(PBVH *UNUSED(pbvh), PBVHNode *node)
{
  if (node->draw_batches) {
    DRW_pbvh_node_free(node->draw_batches);
    node->draw_batches = NULL;
  }
}

static void pbvh_update_draw_buffers(
    PBVH *pbvh, Mesh *me, PBVHNode **nodes, int totnode, int update_flag)
{
  const CustomData *vdata;

  switch (pbvh->header.type) {
    case PBVH_BMESH:
      if (!pbvh->header.bm) {
        /* BMesh hasn't been created yet */
        return;
      }

      vdata = &pbvh->header.bm->vdata;
      break;
    case PBVH_FACES:
      vdata = pbvh->vdata;
      break;
    case PBVH_GRIDS:
      vdata = NULL;
      break;
  }
  UNUSED_VARS(vdata);

  if ((update_flag & PBVH_RebuildDrawBuffers) || ELEM(pbvh->header.type, PBVH_GRIDS, PBVH_BMESH)) {
    /* Free buffers uses OpenGL, so not in parallel. */
    for (int n = 0; n < totnode; n++) {
      PBVHNode *node = nodes[n];
      if (node->flag & PBVH_RebuildDrawBuffers) {
        pbvh_free_draw_buffers(pbvh, node);
      }
      else if ((node->flag & PBVH_UpdateDrawBuffers) && node->draw_batches) {
        PBVH_GPU_Args args;

        pbvh_draw_args_init(pbvh, &args, node);
        DRW_pbvh_update_pre(node->draw_batches, &args);
      }
    }
  }

  /* Parallel creation and update of draw buffers. */
  PBVHUpdateData data = {
      .pbvh = pbvh, .nodes = nodes, .flat_vcol_shading = pbvh->flat_vcol_shading, .mesh = me};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_update_draw_buffer_cb, &settings);

  for (int i = 0; i < totnode; i++) {
    PBVHNode *node = nodes[i];

    if (node->flag & PBVH_UpdateDrawBuffers) {
      /* Flush buffers uses OpenGL, so not in parallel. */
      if (node->draw_batches) {
        DRW_pbvh_node_gpu_flush(node->draw_batches);
      }
    }

    node->flag &= ~(PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers);
  }
}

static int pbvh_flush_bb(PBVH *pbvh, PBVHNode *node, int flag)
{
  int update = 0;

  /* Difficult to multi-thread well, we just do single threaded recursive. */
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

  update |= pbvh_flush_bb(pbvh, pbvh->nodes + node->children_offset, flag);
  update |= pbvh_flush_bb(pbvh, pbvh->nodes + node->children_offset + 1, flag);

  update_node_vb(pbvh, node, update);

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
    for (int i = 0; i < totnode; i++) {
      nodes[i]->flag |= PBVH_UpdateRedraw | PBVH_UpdateDrawBuffers | PBVH_UpdateColor;
    }
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
  int totvert, i;
  BKE_pbvh_node_num_verts(pbvh, node, NULL, &totvert);
  const int *vert_indices = BKE_pbvh_node_get_vert_indices(node);

  if (pbvh->hide_vert == NULL) {
    BKE_pbvh_node_fully_hidden_set(node, false);
    return;
  }
  for (i = 0; i < totvert; i++) {
    if (!(pbvh->hide_vert[vert_indices[i]])) {
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
  TableGSet *unique, *other;

  unique = BKE_pbvh_bmesh_node_unique_verts(node);
  other = BKE_pbvh_bmesh_node_other_verts(node);

  BMVert *v;

  TGSET_ITER (v, unique) {
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }
  TGSET_ITER_END

  TGSET_ITER (v, other) {
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }
  TGSET_ITER_END

  BKE_pbvh_node_fully_hidden_set(node, true);
}

static void pbvh_update_visibility_task_cb(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{

  PBVHUpdateData *data = userdata;
  PBVH *pbvh = data->pbvh;
  PBVHNode *node = data->nodes[n];
  if (node->flag & PBVH_UpdateVisibility) {
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
    node->flag &= ~PBVH_UpdateVisibility;
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

bool BKE_pbvh_has_faces(const PBVH *pbvh)
{
  if (pbvh->header.type == PBVH_BMESH) {
    return (pbvh->header.bm->totface != 0);
  }

  return (pbvh->totprim != 0);
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
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return pbvh->grid_hidden;
}

const CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return &pbvh->gridkey;
}

struct CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return pbvh->grids;
}

BLI_bitmap **BKE_pbvh_get_grid_visibility(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return pbvh->grid_hidden;
}

int BKE_pbvh_get_grid_num_verts(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return pbvh->totgrid * pbvh->gridkey.grid_area;
}

int BKE_pbvh_get_grid_num_faces(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_GRIDS);
  return pbvh->totgrid * (pbvh->gridkey.grid_size - 1) * (pbvh->gridkey.grid_size - 1);
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_original_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateOriginalBB;
}

void BKE_pbvh_node_mark_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw | PBVH_UpdateCurvatureDir |
                PBVH_RebuildPixels | PBVH_UpdateTriAreas;
}

void BKE_pbvh_node_mark_update_mask(PBVHNode *node)
{
  node->flag |= PBVH_UpdateMask | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_color(PBVHNode *node)
{
  node->flag |= PBVH_UpdateColor | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_face_sets(PBVHNode *node)
{
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_mark_rebuild_pixels(PBVH *pbvh)
{
  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
    if (node->flag & PBVH_Leaf) {
      node->flag |= PBVH_RebuildPixels;
    }
  }
}

void BKE_pbvh_node_mark_update_visibility(PBVHNode *node)
{
  node->flag |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                PBVH_UpdateRedraw | PBVH_UpdateTris;
}

void BKE_pbvh_vert_tag_update_normal_visibility(PBVHNode *node)
{
  node->flag |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                PBVH_UpdateRedraw | PBVH_UpdateCurvatureDir | PBVH_UpdateTris;
}

void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node)
{
  node->flag |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw |
                PBVH_UpdateCurvatureDir;
}

void BKE_pbvh_node_mark_redraw(PBVHNode *node)
{
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_normals_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals | PBVH_UpdateCurvatureDir;
}

void BKE_pbvh_node_mark_curvature_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateCurvatureDir;
}

void BKE_pbvh_curvature_update_set(PBVHNode *node, bool state)
{
  if (state) {
    node->flag |= PBVH_UpdateCurvatureDir;
  }
  else {
    node->flag &= ~PBVH_UpdateCurvatureDir;
  }
}

bool BKE_pbvh_curvature_update_get(PBVHNode *node)
{
  return node->flag & PBVH_UpdateCurvatureDir;
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

bool BKE_pbvh_node_fully_hidden_get(PBVHNode *node)
{
  return (node->flag & PBVH_Leaf) && (node->flag & PBVH_FullyHidden);
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

void BKE_pbvh_vert_tag_update_normal(PBVH *pbvh, PBVHVertRef vertex)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  pbvh->vert_bitmap[vertex.i] = true;
}

void BKE_pbvh_node_get_loops(PBVH *pbvh,
                             PBVHNode *node,
                             const int **r_loop_indices,
                             const MLoop **r_loops)
{
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  if (r_loop_indices) {
    *r_loop_indices = node->loop_indices;
  }

  if (r_loops) {
    *r_loops = pbvh->mloop;
  }
}

int BKE_pbvh_num_faces(const PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
    case PBVH_FACES:
      return pbvh->faces_num;
    case PBVH_BMESH:
      return pbvh->header.bm->totface;
  }

  BLI_assert_unreachable();
  return 0;
}

const int *BKE_pbvh_node_get_vert_indices(PBVHNode *node)

{
  return node->vert_indices;
}

void BKE_pbvh_node_num_verts(PBVH *pbvh, PBVHNode *node, int *r_uniquevert, int *r_totvert)
{
  int tot;

  switch (pbvh->header.type) {
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
      // not a leaf? return zero
      if (!(node->flag & PBVH_Leaf)) {
        if (r_totvert) {
          *r_totvert = 0;
        }

        if (r_uniquevert) {
          *r_uniquevert = 0;
        }

        return;
      }

      pbvh_bmesh_check_other_verts(node);

      tot = BLI_table_gset_len(node->bm_unique_verts);
      if (r_totvert) {
        *r_totvert = tot + BLI_table_gset_len(node->bm_other_verts);
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
  switch (pbvh->header.type) {
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

bool BKE_pbvh_node_has_vert_with_normal_update_tag(PBVH *pbvh, PBVHNode *node)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  const int *verts = node->vert_indices;
  const int totvert = node->uniq_verts + node->face_verts;

  for (int i = 0; i < totvert; i++) {
    const int v = verts[i];

    if (pbvh->vert_bitmap[v]) {
      return true;
    }
  }

  return false;
}

/********************************* Ray-cast ***********************************/

typedef struct {
  struct IsectRayAABB_Precalc ray;
  bool original;
  int stroke_id;
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
                      bool original,
                      int stroke_id)
{
  RaycastData rcd;

  isect_ray_aabb_v3_precalc(&rcd.ray, ray_start, ray_normal);

  rcd.original = original;
  rcd.stroke_id = stroke_id;
  pbvh->stroke_id = stroke_id;

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
       (depth_test < *depth)))
  {
    *depth = depth_test;
    return true;
  }

  return false;
}

bool ray_face_intersection_tri(const float ray_start[3],
                               struct IsectRayPrecalc *isect_precalc,
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth)
{
  float depth_test;
  if (isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, NULL) &&
      (depth_test < *depth))
  {
    *depth = depth_test;
    return true;
  }

  return false;
}

bool ray_update_depth_and_hit_count(const float depth_test,
                                    float *r_depth,
                                    float *r_back_depth,
                                    int *hit_count)
{
  (*hit_count)++;
  if (depth_test < *r_depth) {
    *r_back_depth = *r_depth;
    *r_depth = depth_test;
    return true;
  }
  else if (depth_test > *r_depth && depth_test <= *r_back_depth) {
    *r_back_depth = depth_test;
    return false;
  }

  return false;
}

bool ray_face_intersection_depth_quad(const float ray_start[3],
                                      struct IsectRayPrecalc *isect_precalc,
                                      const float t0[3],
                                      const float t1[3],
                                      const float t2[3],
                                      const float t3[3],
                                      float *r_depth,
                                      float *r_back_depth,
                                      int *hit_count)
{
  float depth_test;
  if (!(isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, NULL) ||
        isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t2, t3, &depth_test, NULL)))
  {
    return false;
  }
  return ray_update_depth_and_hit_count(depth_test, r_depth, r_back_depth, hit_count);
}

bool ray_face_intersection_depth_tri(const float ray_start[3],
                                     struct IsectRayPrecalc *isect_precalc,
                                     const float t0[3],
                                     const float t1[3],
                                     const float t2[3],
                                     float *r_depth,
                                     float *r_back_depth,
                                     int *hit_count)
{
  float depth_test;

  if (!isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, NULL)) {
    return false;
  }
  return ray_update_depth_and_hit_count(depth_test, r_depth, r_back_depth, hit_count);
}

/* Take advantage of the fact we know this won't be an intersection.
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

  if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
           ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)
  {
    *dist_sq = dist_sq_test;
    *depth = depth_test;
    if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
             ray_start, ray_normal, t0, t2, t3, co, &depth_test)) < *dist_sq)
    {
      *dist_sq = dist_sq_test;
      *depth = depth_test;
    }
    return true;
  }

  return false;
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

  if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
           ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)
  {
    *dist_sq = dist_sq_test;
    *depth = depth_test;
    return true;
  }

  return false;
}

static bool pbvh_faces_node_raycast(PBVH *pbvh,
                                    const PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    struct IsectRayPrecalc *isect_precalc,
                                    int *hit_count,
                                    float *depth,
                                    float *depth_back,
                                    PBVHVertRef *r_active_vertex_index,
                                    PBVHFaceRef *r_active_face_index,
                                    float *r_face_normal,
                                    int stroke_id)
{
  const float(*positions)[3] = pbvh->vert_positions;
  const MLoop *mloop = pbvh->mloop;
  const int *faces = node->prim_indices;
  int totface = node->totprim;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  for (int i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (pbvh->respect_hide && paint_is_face_hidden(lt, pbvh->hide_poly)) {
      continue;
    }

    const float *co[3];
    if (origco) {
      /* Intersect with backed up original coordinates. */
      co[0] = origco[face_verts[0]];
      co[1] = origco[face_verts[1]];
      co[2] = origco[face_verts[2]];
    }
    else {
      /* intersect with current coordinates */
      co[0] = positions[mloop[lt->tri[0]].v];
      co[1] = positions[mloop[lt->tri[1]].v];
      co[2] = positions[mloop[lt->tri[2]].v];
    }

    if (!ray_face_intersection_depth_tri(
            ray_start, isect_precalc, co[0], co[1], co[2], depth, depth_back, hit_count))
    {
      continue;
    }

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
          *r_active_vertex_index = (PBVHVertRef){.i = mloop[lt->tri[j]].v};
          *r_active_face_index = (PBVHFaceRef){.i = lt->poly};
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
                                    int *hit_count,
                                    float *depth,
                                    float *back_depth,

                                    PBVHVertRef *r_active_vertex,
                                    PBVHFaceRef *r_active_grid,
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
          co[0] = origco[(y + 1) * gridsize + x];
          co[1] = origco[(y + 1) * gridsize + x + 1];
          co[2] = origco[y * gridsize + x + 1];
          co[3] = origco[y * gridsize + x];
        }
        else {
          co[0] = CCG_grid_elem_co(gridkey, grid, x, y + 1);
          co[1] = CCG_grid_elem_co(gridkey, grid, x + 1, y + 1);
          co[2] = CCG_grid_elem_co(gridkey, grid, x + 1, y);
          co[3] = CCG_grid_elem_co(gridkey, grid, x, y);
        }

        if (!ray_face_intersection_depth_quad(ray_start,
                                              isect_precalc,
                                              co[0],
                                              co[1],
                                              co[2],
                                              co[3],
                                              depth,
                                              back_depth,
                                              hit_count))
        {
          continue;
        }
        hit = true;

        if (r_face_normal) {
          normal_quad_v3(r_face_normal, co[0], co[1], co[2], co[3]);
        }

        if (r_active_vertex) {
          float location[3] = {0.0};
          madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);

          const int x_it[4] = {0, 1, 1, 0};
          const int y_it[4] = {1, 1, 0, 0};

          for (int j = 0; j < 4; j++) {
            /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
             * uninitialized values. This stores the closest vertex in the current intersecting
             * quad. */
            if (j == 0 || len_squared_v3v3(location, co[j]) <
                              len_squared_v3v3(location, nearest_vertex_co)) {
              copy_v3_v3(nearest_vertex_co, co[j]);

              r_active_vertex->i = gridkey->grid_area * grid_index +
                                   (y + y_it[j]) * gridkey->grid_size + (x + x_it[j]);
            }
          }
        }

        if (r_active_grid) {
          r_active_grid->i = grid_index;
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
                           int *hit_count,
                           float *depth,
                           float *back_depth,
                           PBVHVertRef *active_vertex,
                           PBVHFaceRef *active_face_grid,
                           float *face_normal,
                           int stroke_id)
{
  bool hit = false;

  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  switch (pbvh->header.type) {
    case PBVH_FACES:
      hit |= pbvh_faces_node_raycast(pbvh,
                                     node,
                                     origco,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     hit_count,
                                     depth,
                                     back_depth,
                                     active_vertex,
                                     active_face_grid,
                                     face_normal,
                                     stroke_id);

      break;
    case PBVH_GRIDS:
      hit |= pbvh_grids_node_raycast(pbvh,
                                     node,
                                     origco,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     hit_count,
                                     depth,
                                     back_depth,
                                     active_vertex,
                                     active_face_grid,
                                     face_normal);
      break;
    case PBVH_BMESH:
      hit = pbvh_bmesh_node_raycast(pbvh,
                                    node,
                                    ray_start,
                                    ray_normal,
                                    isect_precalc,
                                    hit_count,
                                    depth,
                                    back_depth,
                                    use_origco,
                                    active_vertex,
                                    active_face_grid,
                                    face_normal,
                                    stroke_id);
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
    const float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

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
  const float(*positions)[3] = pbvh->vert_positions;
  const MLoop *mloop = pbvh->mloop;
  const int *faces = node->prim_indices;
  int i, totface = node->totprim;
  bool hit = false;

  for (i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[faces[i]];
    const int *face_verts = node->face_vert_indices[i];

    if (pbvh->respect_hide && paint_is_face_hidden(lt, pbvh->hide_poly)) {
      continue;
    }

    if (origco) {
      /* Intersect with backed-up original coordinates. */
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
                                  positions[mloop[lt->tri[0]].v],
                                  positions[mloop[lt->tri[1]].v],
                                  positions[mloop[lt->tri[2]].v],
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
                                       float *dist_sq,
                                       int stroke_id)
{
  bool hit = false;

  if (node->flag & PBVH_FullyHidden) {
    return false;
  }

  switch (pbvh->header.type) {
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
          pbvh, node, ray_start, ray_normal, depth, dist_sq, use_origco, stroke_id);
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
    if (dot_v3v3(planes[i], vmax) + planes[i][3] <= 0) {
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

  if (pbvh->header.type == PBVH_BMESH) {
    for (int i = 0; i < pbvh->totnode; i++) {
      if (pbvh->nodes[i].flag & PBVH_Leaf) {
        BKE_pbvh_bmesh_check_tris(pbvh, pbvh->nodes + i);
      }
    }
  }

  BKE_pbvh_search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateNormals), &nodes, &totnode);

  if (totnode > 0) {
    if (pbvh->header.type == PBVH_BMESH) {
      pbvh_bmesh_normals_update(pbvh, nodes, totnode);
    }
    else if (pbvh->header.type == PBVH_FACES) {
      pbvh_faces_update_normals(pbvh, nodes, totnode);
    }
    else if (pbvh->header.type == PBVH_GRIDS) {
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
  PBVHAttrReq *attrs;
  int attrs_num;
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
                      Mesh *me,
                      bool update_only_visible,
                      PBVHFrustumPlanes *update_frustum,
                      PBVHFrustumPlanes *draw_frustum,
                      void (*draw_fn)(void *user_data, PBVHBatches *batches, PBVH_GPU_Args *args),
                      void *user_data,
                      bool UNUSED(full_render),
                      PBVHAttrReq *attrs,
                      int attrs_num)
{
  PBVHNode **nodes;
  int totnode;
  int update_flag = 0;

  pbvh->draw_cache_invalid = false;

  /* Search for nodes that need updates. */
  if (update_only_visible) {
    /* Get visible nodes with draw updates. */
    PBVHDrawSearchData data = {
        .frustum = update_frustum, .accum_update_flag = 0, attrs, attrs_num};
    BKE_pbvh_search_gather(pbvh, pbvh_draw_search_cb, &data, &nodes, &totnode);
    update_flag = data.accum_update_flag;
  }
  else {
    /* Get all nodes with draw updates, also those outside the view. */
    const int search_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
    BKE_pbvh_search_gather(
        pbvh, update_search_cb, POINTER_FROM_INT(search_flag), &nodes, &totnode);
    update_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
  }

  /* Update draw buffers. */
  if (totnode != 0 && (update_flag & (PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers))) {
    // check that need_full_render is set to GPU_pbvh_need_full_render_get(),
    // but only if nodes need updating}
    pbvh_update_draw_buffers(pbvh, me, nodes, totnode, update_flag);
  }
  MEM_SAFE_FREE(nodes);

  /* Draw visible nodes. */
  PBVHDrawSearchData draw_data = {.frustum = draw_frustum, .accum_update_flag = 0};
  BKE_pbvh_search_gather(pbvh, pbvh_draw_search_cb, &draw_data, &nodes, &totnode);

  PBVH_GPU_Args args;

  for (int i = 0; i < totnode; i++) {
    PBVHNode *node = nodes[i];
    if (!(node->flag & PBVH_FullyHidden)) {
      pbvh_draw_args_init(pbvh, &args, node);

      draw_fn(user_data, node->draw_batches, &args);
    }
  }

  MEM_SAFE_FREE(nodes);
}

void BKE_pbvh_draw_debug_cb(PBVH *pbvh,
                            void (*draw_fn)(PBVHNode *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data)
{
  for (int a = 0; a < pbvh->totnode; a++) {
    PBVHNode *node = &pbvh->nodes[a];

    if (pbvh_show_orig_co) {
      draw_fn(node, user_data, node->orig_vb.bmin, node->orig_vb.bmax, node->flag);
    }
    else {
      draw_fn(node, user_data, node->vb.bmin, node->vb.bmax, node->flag);
    }
  }
}

void BKE_pbvh_grids_update(PBVH *pbvh,
                           CCGElem **grids,
                           void **gridfaces,
                           DMFlagMat *flagmats,
                           BLI_bitmap **grid_hidden,
                           CCGKey *key)
{
  pbvh->gridkey = *key;
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

  if (pbvh->vert_positions) {
    vertCos = MEM_malloc_arrayN(pbvh->totvert, sizeof(float[3]), __func__);
    memcpy(vertCos, pbvh->vert_positions, sizeof(float[3]) * pbvh->totvert);
  }

  return vertCos;
}

void BKE_pbvh_vert_coords_apply(PBVH *pbvh, const float (*vertCos)[3], const int totvert)
{
  if (totvert != pbvh->totvert) {
    BLI_assert_msg(0, "PBVH: Given deforming vcos number does not match PBVH vertex number!");
    return;
  }

  if (!pbvh->deformed) {
    if (pbvh->vert_positions) {
      /* if pbvh is not already deformed, verts/faces points to the */
      /* original data and applying new coords to this arrays would lead to */
      /* unneeded deformation -- duplicate verts/faces to avoid this */

      pbvh->vert_positions = MEM_dupallocN(pbvh->vert_positions);
      /* No need to dupalloc pbvh->looptri, this one is 'totally owned' by pbvh,
       * it's never some mesh data. */

      pbvh->deformed = true;
    }
  }

  if (pbvh->vert_positions) {
    float(*positions)[3] = pbvh->vert_positions;
    /* copy new verts coords */
    for (int a = 0; a < pbvh->totvert; a++) {
      /* no need for float comparison here (memory is exactly equal or not) */
      if (memcmp(positions[a], vertCos[a], sizeof(float[3])) != 0) {
        copy_v3_v3(positions[a], vertCos[a]);
        BKE_pbvh_vert_tag_update_normal(pbvh, BKE_pbvh_make_vref(a));
      }
    }

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

  MEM_SAFE_FREE(node->proxies);
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

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode)
{
  struct CCGElem **grids;
  int *grid_indices;
  int totgrid, gridsize, uniq_verts, totvert;

  vi->grid = NULL;
  vi->no = NULL;
  vi->fno = NULL;
  vi->vert_positions = NULL;
  vi->vertex.i = 0LL;
  vi->index = 0;

  vi->respect_hide = pbvh->respect_hide;
  if (pbvh->respect_hide == false) {
    /* The same value for all vertices. */
    vi->visible = true;
  }

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids);
  BKE_pbvh_node_num_verts(pbvh, node, &uniq_verts, &totvert);
  const int *vert_indices = BKE_pbvh_node_get_vert_indices(node);
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
  vi->vert_positions = pbvh->vert_positions;
  vi->is_mesh = pbvh->vert_positions != NULL;

  if (pbvh->header.type == PBVH_BMESH) {
    if (mode == PBVH_ITER_ALL) {
      pbvh_bmesh_check_other_verts(node);
    }

    vi->vert_positions = NULL;

    vi->bi = 0;
    vi->bm_cur_set = node->bm_unique_verts;
    vi->bm_unique_verts = node->bm_unique_verts;
    vi->bm_other_verts = node->bm_other_verts;
    vi->bm_vdata = &pbvh->header.bm->vdata;
    vi->bm_vert = NULL;

    vi->cd_sculpt_vert = CustomData_get_offset(vi->bm_vdata, CD_DYNTOPO_VERT);
    vi->cd_vert_mask_offset = CustomData_get_offset(vi->bm_vdata, CD_PAINT_MASK);
  }

  vi->gh = NULL;
  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = pbvh->grid_hidden;
  }

  vi->mask = NULL;
  if (pbvh->header.type == PBVH_FACES) {
    vi->vert_normals = pbvh->vert_normals;
    vi->hide_vert = pbvh->hide_vert;

    vi->vmask = CustomData_get_layer_for_write(pbvh->vdata, CD_PAINT_MASK, pbvh->mesh->totvert);
  }
}

bool BKE_pbvh_draw_mask(const PBVH *pbvh)
{
  return BKE_pbvh_has_mask(pbvh) && !(pbvh->flags & PBVH_FAST_DRAW);
}

bool BKE_pbvh_has_mask(const PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      return (pbvh->gridkey.has_mask != 0);
    case PBVH_FACES:
      return (pbvh->vdata && CustomData_get_layer(pbvh->vdata, CD_PAINT_MASK));
    case PBVH_BMESH:
      return (pbvh->header.bm &&
              (CustomData_get_offset(&pbvh->header.bm->vdata, CD_PAINT_MASK) != -1));
  }

  return false;
}

bool BKE_pbvh_draw_face_sets(PBVH *pbvh)
{
  if (pbvh->flags & PBVH_FAST_DRAW) {
    return false;
  }

  switch (pbvh->header.type) {
    case PBVH_GRIDS:
    case PBVH_FACES:
      return pbvh->pdata &&
             CustomData_get_layer_named(pbvh->pdata, CD_PROP_INT32, ".sculpt_face_set") != NULL;
    case PBVH_BMESH:
      return (pbvh->header.bm && CustomData_get_named_layer_index(&pbvh->header.bm->pdata,
                                                                  CD_PROP_INT32,
                                                                  ".sculpt_face_set") != -1);
  }

  return false;
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

#include "BKE_global.h"
void BKE_pbvh_parallel_range_settings(TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode)
{
  memset(settings, 0, sizeof(*settings));
  settings->use_threading = use_threading && totnode > 1 && G.debug_value != 890;
}

float (*BKE_pbvh_get_vert_positions(const PBVH *pbvh))[3]
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return pbvh->vert_positions;
}

const float (*BKE_pbvh_get_vert_normals(const PBVH *pbvh))[3]
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return pbvh->vert_normals;
}

const bool *BKE_pbvh_get_vert_hide(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return pbvh->hide_vert;
}

const bool *BKE_pbvh_get_poly_hide(const PBVH *pbvh)
{
  BLI_assert(ELEM(pbvh->header.type, PBVH_FACES, PBVH_GRIDS));
  return pbvh->hide_poly;
}

bool *BKE_pbvh_get_vert_hide_for_write(PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  if (pbvh->hide_vert) {
    return pbvh->hide_vert;
  }
  pbvh->hide_vert = CustomData_get_layer_named_for_write(
      &pbvh->mesh->vdata, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->totvert);
  if (pbvh->hide_vert) {
    return pbvh->hide_vert;
  }
  pbvh->hide_vert = (bool *)CustomData_add_layer_named(
      &pbvh->mesh->vdata, CD_PROP_BOOL, CD_SET_DEFAULT, NULL, pbvh->mesh->totvert, ".hide_vert");
  return pbvh->hide_vert;
}

void BKE_pbvh_subdiv_ccg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  pbvh->subdiv_ccg = subdiv_ccg;
  pbvh->gridfaces = (void **)subdiv_ccg->grid_faces;
  pbvh->grid_hidden = subdiv_ccg->grid_hidden;
  pbvh->grid_flag_mats = subdiv_ccg->grid_flag_mats;
  pbvh->grids = subdiv_ccg->grids;
}

void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets)
{
  pbvh->face_sets = face_sets;
}

void BKE_pbvh_update_hide_attributes_from_mesh(PBVH *pbvh)
{
  if (pbvh->header.type == PBVH_FACES) {
    pbvh->hide_vert = CustomData_get_layer_named_for_write(
        &pbvh->mesh->vdata, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->totvert);
    pbvh->hide_poly = CustomData_get_layer_named_for_write(
        &pbvh->mesh->pdata, CD_PROP_BOOL, ".hide_poly", pbvh->mesh->totpoly);
  }
}

void BKE_pbvh_respect_hide_set(PBVH *pbvh, bool respect_hide)
{
  pbvh->respect_hide = respect_hide;
}

int BKE_pbvh_get_node_index(PBVH *pbvh, PBVHNode *node)
{
  return (int)(node - pbvh->nodes);
}

int BKE_pbvh_get_totnodes(PBVH *pbvh)
{
  return pbvh->totnode;
}

int BKE_pbvh_get_node_id(PBVH *pbvh, PBVHNode *node)
{
  return node->id;
}

void BKE_pbvh_get_nodes(PBVH *pbvh, int flag, PBVHNode ***r_array, int *r_totnode)
{
  BKE_pbvh_search_gather(pbvh, update_search_cb, POINTER_FROM_INT(flag), r_array, r_totnode);
}

PBVHNode *BKE_pbvh_node_from_index(PBVH *pbvh, int node_i)
{
  return pbvh->nodes + node_i;
}

#ifdef PROXY_ADVANCED
// TODO: if this really works, make sure to pull the neighbor iterator out of sculpt.c and put it
// here
/* clang-format off */
#  include "BKE_context.h"
#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"
#  include "../../editors/sculpt_paint/sculpt_intern.h"
/* clang-format on */

int checkalloc(void **data, int esize, int oldsize, int newsize, int emask, int umask)
{
  // update channel if it already was allocated once, or is requested by umask
  if (newsize != oldsize && (*data || (emask & umask))) {
    if (*data) {
      *data = MEM_reallocN(*data, newsize * esize);
    }
    else {
      *data = MEM_mallocN(newsize * esize, "pbvh proxy vert arrays");
    }
    return emask;
  }

  return 0;
}

void BKE_pbvh_ensure_proxyarray_indexmap(PBVH *pbvh, PBVHNode *node, GHash *vert_node_map)
{
  ProxyVertArray *p = &node->proxyverts;

  int totvert = 0;
  BKE_pbvh_node_num_verts(pbvh, node, &totvert, NULL);

  bool update = !p->indexmap || p->size != totvert;
  update = update || (p->indexmap && BLI_ghash_len(p->indexmap) != totvert);

  if (!update) {
    return;
  }

  if (p->indexmap) {
    BLI_ghash_free(p->indexmap, NULL, NULL);
  }

  GHash *gs = p->indexmap = BLI_ghash_ptr_new("BKE_pbvh_ensure_proxyarray_indexmap");

  PBVHVertexIter vd;

  int i = 0;
  BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_UNIQUE) {
    BLI_ghash_insert(gs, (void *)vd.vertex.i, (void *)i);
    i++;
  }
  BKE_pbvh_vertex_iter_end;
}

bool pbvh_proxyarray_needs_update(PBVH *pbvh, PBVHNode *node, int mask)
{
  ProxyVertArray *p = &node->proxyverts;
  int totvert = 0;

  if (!(node->flag & PBVH_Leaf) || !node->bm_unique_verts) {
    return false;
  }

  BKE_pbvh_node_num_verts(pbvh, node, &totvert, NULL);

  bool bad = p->size != totvert;
  bad = bad || ((mask & PV_NEIGHBORS) && p->neighbors_dirty);
  bad = bad || (p->datamask & mask) != mask;

  bad = bad && totvert > 0;

  return bad;
}

GHash *pbvh_build_vert_node_map(PBVH *pbvh, PBVHNode **nodes, int totnode, int mask)
{
  GHash *vert_node_map = BLI_ghash_ptr_new("BKE_pbvh_ensure_proxyarrays");

  for (int i = 0; i < totnode; i++) {
    PBVHVertexIter vd;
    PBVHNode *node = nodes[i];

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_UNIQUE) {
      BLI_ghash_insert(vert_node_map, (void *)vd.vertex.i, (void *)(node - pbvh->nodes));
    }
    BKE_pbvh_vertex_iter_end;
  }

  return vert_node_map;
}

void BKE_pbvh_ensure_proxyarrays(
    SculptSession *ss, PBVH *pbvh, PBVHNode **nodes, int totnode, int mask)
{

  bool update = false;

  for (int i = 0; i < totnode; i++) {
    if (pbvh_proxyarray_needs_update(pbvh, nodes[i], mask)) {
      update = true;
      break;
    }
  }

  if (!update) {
    return;
  }

  GHash *vert_node_map = pbvh_build_vert_node_map(pbvh, nodes, totnode, mask);

  for (int i = 0; i < totnode; i++) {
    if (nodes[i]->flag & PBVH_Leaf) {
      BKE_pbvh_ensure_proxyarray_indexmap(pbvh, nodes[i], vert_node_map);
    }
  }

  for (int i = 0; i < totnode; i++) {
    if (nodes[i]->flag & PBVH_Leaf) {
      BKE_pbvh_ensure_proxyarray(ss, pbvh, nodes[i], mask, vert_node_map, false, false);
    }
  }

  if (vert_node_map) {
    BLI_ghash_free(vert_node_map, NULL, NULL);
  }
}

void BKE_pbvh_ensure_proxyarray(SculptSession *ss,
                                PBVH *pbvh,
                                PBVHNode *node,
                                int mask,
                                GHash *vert_node_map,
                                bool check_indexmap,
                                bool force_update)
{
  ProxyVertArray *p = &node->proxyverts;

  if (check_indexmap) {
    BKE_pbvh_ensure_proxyarray_indexmap(pbvh, node, vert_node_map);
  }

  GHash *gs = p->indexmap;

  int totvert = 0;
  BKE_pbvh_node_num_verts(pbvh, node, &totvert, NULL);

  if (!totvert) {
    return;
  }

  int updatemask = 0;

#  define UPDATETEST(name, emask, esize) \
    if (mask & emask) { \
      updatemask |= checkalloc((void **)&p->name, esize, p->size, totvert, emask, mask); \
    }

  UPDATETEST(ownerco, PV_OWNERCO, sizeof(void *))
  UPDATETEST(ownerno, PV_OWNERNO, sizeof(void *))
  UPDATETEST(ownermask, PV_OWNERMASK, sizeof(void *))
  UPDATETEST(ownercolor, PV_OWNERCOLOR, sizeof(void *))
  UPDATETEST(co, PV_CO, sizeof(float) * 3)
  UPDATETEST(no, PV_NO, sizeof(short) * 3)
  UPDATETEST(fno, PV_NO, sizeof(float) * 3)
  UPDATETEST(mask, PV_MASK, sizeof(float))
  UPDATETEST(color, PV_COLOR, sizeof(float) * 4)
  UPDATETEST(index, PV_INDEX, sizeof(PBVHVertRef))
  UPDATETEST(neighbors, PV_NEIGHBORS, sizeof(ProxyKey) * MAX_PROXY_NEIGHBORS)

  p->size = totvert;

  if (force_update) {
    updatemask |= mask;
  }

  if ((mask & PV_NEIGHBORS) && p->neighbors_dirty) {
    updatemask |= PV_NEIGHBORS;
  }

  if (!updatemask) {
    return;
  }

  p->datamask |= mask;

  PBVHVertexIter vd;

  int i = 0;

#  if 1
  BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_UNIQUE) {
    void **val;

    if (!BLI_ghash_ensure_p(gs, (void *)vd.vertex.i, &val)) {
      *val = (void *)i;
    };
    i++;
  }
  BKE_pbvh_vertex_iter_end;
#  endif

  if (updatemask & PV_NEIGHBORS) {
    p->neighbors_dirty = false;
  }

  i = 0;
  BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (i >= p->size) {
      printf("error!! %s\n", __func__);
      break;
    }

    if (updatemask & PV_OWNERCO) {
      p->ownerco[i] = vd.co;
    }
    if (updatemask & PV_INDEX) {
      p->index[i] = vd.vertex;
    }
    if (updatemask & PV_OWNERNO) {
      p->ownerno[i] = vd.no;
    }
    if (updatemask & PV_NO) {
      if (vd.fno) {
        if (p->fno) {
          copy_v3_v3(p->fno[i], vd.fno);
        }
        normal_float_to_short_v3(p->no[i], vd.fno);
      }
      else if (vd.no) {
        copy_v3_v3_short(p->no[i], vd.no);
        if (p->fno) {
          normal_short_to_float_v3(p->fno[i], vd.no);
        }
      }
      else {
        p->no[i][0] = p->no[i][1] = p->no[i][2] = 0;
        if (p->fno) {
          zero_v3(p->fno[i]);
        }
      }
    }
    if (updatemask & PV_CO) {
      copy_v3_v3(p->co[i], vd.co);
    }
    if (updatemask & PV_OWNERMASK) {
      p->ownermask[i] = vd.mask;
    }
    if (updatemask & PV_MASK) {
      p->mask[i] = vd.mask ? *vd.mask : 0.0f;
    }
    if (updatemask & PV_COLOR) {
      if (vd.vcol) {
        copy_v4_v4(p->color[i], vd.vcol->color);
      }
    }

    if (updatemask & PV_NEIGHBORS) {
      int j = 0;
      SculptVertexNeighborIter ni;

      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
        if (j >= MAX_PROXY_NEIGHBORS - 1) {
          break;
        }

        ProxyKey key;

        int *pindex = (int *)BLI_ghash_lookup_p(gs, (void *)ni.vertex.i);

        if (!pindex) {
          if (vert_node_map) {
            int *nindex = (int *)BLI_ghash_lookup_p(vert_node_map, (void *)ni.vertex.i);

            if (!nindex) {
              p->neighbors_dirty = true;
              continue;
            }

            PBVHNode *node2 = pbvh->nodes + *nindex;
            if (node2->proxyverts.indexmap) {
              pindex = (int *)BLI_ghash_lookup_p(node2->proxyverts.indexmap, (void *)ni.vertex.i);
            }
            else {
              pindex = NULL;
            }

            if (!pindex) {
              p->neighbors_dirty = true;
              continue;
            }

            key.node = (int)(node2 - pbvh->nodes);
            key.pindex = *pindex;
            //*
            if (node2->proxyverts.size != 0 &&
                (key.pindex < 0 || key.pindex >= node2->proxyverts.size)) {
              printf("error! %s\n", __func__);
              fflush(stdout);
              p->neighbors_dirty = true;
              continue;
            }
            //*/
          }
          else {
            p->neighbors_dirty = true;
            continue;
          }
        }
        else {
          key.node = (int)(node - pbvh->nodes);
          key.pindex = *pindex;
        }

        p->neighbors[i][j++] = key;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      p->neighbors[i][j].node = -1;
    }

    i++;
  }
  BKE_pbvh_vertex_iter_end;
}

typedef struct GatherProxyThread {
  PBVHNode **nodes;
  PBVH *pbvh;
  int mask;
} GatherProxyThread;

static void pbvh_load_proxyarray_exec(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  GatherProxyThread *data = (GatherProxyThread *)userdata;
  PBVHNode *node = data->nodes[n];
  PBVHVertexIter vd;
  ProxyVertArray *p = &node->proxyverts;
  int i = 0;

  int mask = p->datamask;

  BKE_pbvh_ensure_proxyarray(NULL, data->pbvh, node, data->mask, NULL, false, true);
}

void BKE_pbvh_load_proxyarrays(PBVH *pbvh, PBVHNode **nodes, int totnode, int mask)
{
  GatherProxyThread data = {.nodes = nodes, .pbvh = pbvh, .mask = mask};

  mask = mask & ~PV_NEIGHBORS;  // don't update neighbors in threaded code?

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_load_proxyarray_exec, &settings);
}

static void pbvh_gather_proxyarray_exec(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  GatherProxyThread *data = (GatherProxyThread *)userdata;
  PBVHNode *node = data->nodes[n];
  PBVHVertexIter vd;
  ProxyVertArray *p = &node->proxyverts;
  int i = 0;

  int mask = p->datamask;

  BKE_pbvh_vertex_iter_begin (data->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (mask & PV_CO) {
      copy_v3_v3(vd.co, p->co[i]);
    }

    if (mask & PV_COLOR && vd.col) {
      copy_v4_v4(vd.col, p->color[i]);
    }

    if (vd.mask && (mask & PV_MASK)) {
      *vd.mask = p->mask[i];
    }

    i++;
  }
  BKE_pbvh_vertex_iter_end;
}

void BKE_pbvh_gather_proxyarray(PBVH *pbvh, PBVHNode **nodes, int totnode)
{
  GatherProxyThread data = {.nodes = nodes, .pbvh = pbvh};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, pbvh_gather_proxyarray_exec, &settings);
}

void BKE_pbvh_free_proxyarray(PBVH *pbvh, PBVHNode *node)
{
  ProxyVertArray *p = &node->proxyverts;

  if (p->indexmap) {
    BLI_ghash_free(p->indexmap, NULL, NULL);
  }
  if (p->co)
    MEM_freeN(p->co);
  if (p->no)
    MEM_freeN(p->no);
  if (p->index)
    MEM_freeN(p->index);
  if (p->mask)
    MEM_freeN(p->mask);
  if (p->ownerco)
    MEM_freeN(p->ownerco);
  if (p->ownerno)
    MEM_freeN(p->ownerno);
  if (p->ownermask)
    MEM_freeN(p->ownermask);
  if (p->ownercolor)
    MEM_freeN(p->ownercolor);
  if (p->color)
    MEM_freeN(p->color);
  if (p->neighbors)
    MEM_freeN(p->neighbors);

  memset(p, 0, sizeof(*p));
}

void BKE_pbvh_update_proxyvert(PBVH *pbvh, PBVHNode *node, ProxyVertUpdateRec *rec) {}

ProxyVertArray *BKE_pbvh_get_proxyarrays(PBVH *pbvh, PBVHNode *node)
{
  return &node->proxyverts;
}

#endif

/* checks if pbvh needs to sync its flat vcol shading flag with scene tool settings
   scene and ob are allowd to be NULL (in which case nothing is done).
*/
void SCULPT_update_flat_vcol_shading(Object *ob, Scene *scene)
{
  if (!scene || !ob || !ob->sculpt || !ob->sculpt->pbvh) {
    return;
  }

  if (ob->sculpt->pbvh) {
    bool flat_vcol_shading = ((scene->toolsettings->sculpt->flags &
                               SCULPT_DYNTOPO_FLAT_VCOL_SHADING) != 0);

    BKE_pbvh_set_flat_vcol_shading(ob->sculpt->pbvh, flat_vcol_shading);
  }
}

PBVHNode *BKE_pbvh_get_node(PBVH *pbvh, int node)
{
  return pbvh->nodes + node;
}

bool BKE_pbvh_node_mark_update_index_buffer(PBVH *pbvh, PBVHNode *node)
{
  bool split_indexed = pbvh->header.bm &&
                       (pbvh->flags & (PBVH_DYNTOPO_SMOOTH_SHADING | PBVH_FAST_DRAW));

  if (split_indexed) {
    BKE_pbvh_vert_tag_update_normal_triangulation(node);
  }

  return split_indexed;
}

void BKE_pbvh_vert_tag_update_normal_triangulation(PBVHNode *node)
{
  node->flag |= PBVH_UpdateTris;
}

void BKE_pbvh_vert_tag_update_normal_tri_area(PBVHNode *node)
{
  node->flag |= PBVH_UpdateTriAreas;
}

/* must be called outside of threads */
void BKE_pbvh_face_areas_begin(PBVH *pbvh)
{
  pbvh->face_area_i ^= 1;
}

void BKE_pbvh_update_all_tri_areas(PBVH *pbvh)
{
  /* swap read/write face area buffers */
  pbvh->face_area_i ^= 1;

  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;
    if (node->flag & PBVH_Leaf) {
      node->flag |= PBVH_UpdateTriAreas;
#if 0
      // ensure node triangulations are valid
      // so we don't end up doing it inside brush threads
      BKE_pbvh_bmesh_check_tris(pbvh, node);
#endif
    }
  }
}

void BKE_pbvh_check_tri_areas(PBVH *pbvh, PBVHNode *node)
{
  if (!(node->flag & PBVH_UpdateTriAreas)) {
    return;
  }

  if (pbvh->header.type == PBVH_BMESH) {
    if (node->flag & PBVH_UpdateTris) {
      BKE_pbvh_bmesh_check_tris(pbvh, node);
    }

    if (!node->tribuf || !node->tribuf->tottri) {
      return;
    }
  }

  node->flag &= ~PBVH_UpdateTriAreas;

  const int cur_i = pbvh->face_area_i ^ 1;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES: {
      for (int i = 0; i < (int)node->totprim; i++) {
        const MLoopTri *lt = &pbvh->looptri[node->prim_indices[i]];

        if (pbvh->hide_poly && pbvh->hide_poly[lt->poly]) {
          /* Skip hidden faces. */
          continue;
        }

        pbvh->face_areas[lt->poly * 2 + cur_i] = 0.0f;
      }

      for (int i = 0; i < (int)node->totprim; i++) {
        const MLoopTri *lt = &pbvh->looptri[node->prim_indices[i]];

        if (pbvh->hide_poly && pbvh->hide_poly[lt->poly]) {
          /* Skip hidden faces. */
          continue;
        }

        float area = area_tri_v3(pbvh->vert_positions[pbvh->mloop[lt->tri[0]].v],
                                 pbvh->vert_positions[pbvh->mloop[lt->tri[1]].v],
                                 pbvh->vert_positions[pbvh->mloop[lt->tri[2]].v]);

        pbvh->face_areas[lt->poly * 2 + cur_i] += area;

        /* sanity check on read side of read write buffer */
        if (pbvh->face_areas[lt->poly * 2 + (cur_i ^ 1)] == 0.0f) {
          pbvh->face_areas[lt->poly * 2 + (cur_i ^ 1)] = pbvh->face_areas[lt->poly * 2 + cur_i];
        }
      }
      break;
    }
    case PBVH_GRIDS:
      break;
    case PBVH_BMESH: {
      BMFace *f;
      const int cd_face_area = pbvh->cd_face_area;

      TGSET_ITER (f, node->bm_faces) {
        float *areabuf = BM_ELEM_CD_GET_VOID_P(f, cd_face_area);
        areabuf[cur_i] = 0.0f;
      }
      TGSET_ITER_END;

      for (int i = 0; i < node->tribuf->tottri; i++) {
        PBVHTri *tri = node->tribuf->tris + i;

        BMVert *v1 = (BMVert *)(node->tribuf->verts[tri->v[0]].i);
        BMVert *v2 = (BMVert *)(node->tribuf->verts[tri->v[1]].i);
        BMVert *v3 = (BMVert *)(node->tribuf->verts[tri->v[2]].i);
        BMFace *f = (BMFace *)tri->f.i;

        float *areabuf = BM_ELEM_CD_GET_VOID_P(f, cd_face_area);
        areabuf[cur_i] += area_tri_v3(v1->co, v2->co, v3->co);
      }

      TGSET_ITER (f, node->bm_faces) {
        float *areabuf = BM_ELEM_CD_GET_VOID_P(f, cd_face_area);

        /* sanity check on read side of read write buffer */
        if (areabuf[cur_i ^ 1] == 0.0f) {
          areabuf[cur_i ^ 1] = areabuf[cur_i];
        }
      }
      TGSET_ITER_END;

      break;
    }
    default:
      break;
  }
}

static void pbvh_pmap_to_edges_add(PBVH *pbvh,
                                   PBVHVertRef vertex,
                                   int **r_edges,
                                   int *r_edges_size,
                                   bool *heap_alloc,
                                   int e,
                                   int p,
                                   int *len,
                                   int **r_polys)
{
  for (int i = 0; i < *len; i++) {
    if ((*r_edges)[i] == e) {
      if ((*r_polys)[i * 2 + 1] == -1) {
        (*r_polys)[i * 2 + 1] = p;
      }
      return;
    }
  }

  if (*len >= *r_edges_size) {
    int newsize = *len + ((*len) >> 1) + 1;

    int *r_edges_new = MEM_malloc_arrayN(newsize, sizeof(*r_edges_new), "r_edges_new");
    int *r_polys_new = MEM_malloc_arrayN(newsize * 2, sizeof(*r_polys_new), "r_polys_new");

    memcpy((void *)r_edges_new, (void *)*r_edges, sizeof(int) * (*r_edges_size));
    memcpy((void *)r_polys_new, (void *)(*r_polys), sizeof(int) * 2 * (*r_edges_size));

    *r_edges_size = newsize;

    if (*heap_alloc) {
      MEM_freeN(*r_polys);
      MEM_freeN(*r_edges);
    }

    *r_edges = r_edges_new;
    *r_polys = r_polys_new;

    *heap_alloc = true;
  }

  (*r_polys)[*len * 2] = p;
  (*r_polys)[*len * 2 + 1] = -1;

  (*r_edges)[*len] = e;
  (*len)++;
}

void BKE_pbvh_pmap_to_edges(PBVH *pbvh,
                            PBVHVertRef vertex,
                            int **r_edges,
                            int *r_edges_size,
                            bool *r_heap_alloc,
                            int **r_polys)
{
  MeshElemMap *map = pbvh->pmap + vertex.i;
  int len = 0;

  for (int i = 0; i < map->count; i++) {
    const MPoly *mp = pbvh->mpoly + map->indices[i];
    const MLoop *ml = pbvh->mloop + mp->loopstart;

    if (pbvh->hide_poly && pbvh->hide_poly[map->indices[i]]) {
      /* Skip connectivity from hidden faces. */
      continue;
    }

    for (int j = 0; j < mp->totloop; j++, ml++) {
      if (ml->v == vertex.i) {
        pbvh_pmap_to_edges_add(pbvh,
                               vertex,
                               r_edges,
                               r_edges_size,
                               r_heap_alloc,
                               ME_POLY_LOOP_PREV(pbvh->mloop, mp, j)->e,
                               map->indices[i],
                               &len,
                               r_polys);
        pbvh_pmap_to_edges_add(pbvh,
                               vertex,
                               r_edges,
                               r_edges_size,
                               r_heap_alloc,
                               ml->e,
                               map->indices[i],
                               &len,
                               r_polys);
      }
    }
  }

  *r_edges_size = len;
}

void BKE_pbvh_set_vemap(PBVH *pbvh, MeshElemMap *vemap)
{
  pbvh->vemap = vemap;
}

void BKE_pbvh_get_vert_face_areas(PBVH *pbvh, PBVHVertRef vertex, float *r_areas, int valence)
{
  const int cur_i = pbvh->face_area_i;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES: {
      int *edges = BLI_array_alloca(edges, 16);
      int *polys = BLI_array_alloca(polys, 32);
      bool heap_alloc = false;
      int len = 16;

      BKE_pbvh_pmap_to_edges(pbvh, vertex, &edges, &len, &heap_alloc, &polys);
      len = MIN2(len, valence);

      if (pbvh->vemap) {
        /* sort poly references by vemap edge ordering */
        MeshElemMap *emap = pbvh->vemap + vertex.i;

        int *polys_old = BLI_array_alloca(polys, len * 2);
        memcpy((void *)polys_old, (void *)polys, sizeof(int) * len * 2);

        /* note that wire edges will break this, but
           should only result in incorrect weights
           and isn't worth fixing */

        for (int i = 0; i < len; i++) {
          for (int j = 0; j < len; j++) {
            if (emap->indices[i] == edges[j]) {
              polys[i * 2] = polys_old[j * 2];
              polys[i * 2 + 1] = polys_old[j * 2 + 1];
            }
          }
        }
      }
      for (int i = 0; i < len; i++) {
        r_areas[i] = pbvh->face_areas[polys[i * 2] * 2 + cur_i];

        if (polys[i * 2 + 1] != -1) {
          r_areas[i] += pbvh->face_areas[polys[i * 2 + 1] * 2 + cur_i];
          r_areas[i] *= 0.5f;
        }
      }

      if (heap_alloc) {
        MEM_freeN(edges);
        MEM_freeN(polys);
      }

      break;
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      BMEdge *e = v->e;

      if (!e) {
        for (int i = 0; i < valence; i++) {
          r_areas[i] = 1.0f;
        }

        return;
      }

      const int cd_face_area = pbvh->cd_face_area;
      int j = 0;

      do {
        float w = 0.0f;

        if (!e->l) {
          w = 0.0f;
        }
        else {
          float *a1 = BM_ELEM_CD_GET_VOID_P(e->l->f, cd_face_area);
          float *a2 = BM_ELEM_CD_GET_VOID_P(e->l->radial_next->f, cd_face_area);

          w += a1[cur_i] * 0.5f;
          w += a2[cur_i] * 0.5f;
        }

        if (j >= valence) {
          printf("%s: error, corrupt edge cycle\n", __func__);
          break;
        }

        r_areas[j++] = w;

        e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
      } while (e != v->e);

      for (; j < valence; j++) {
        r_areas[j] = 1.0f;
      }

      break;
    }

    case PBVH_GRIDS: { /* estimate from edge lengths */
      int index = (int)vertex.i;

      const CCGKey *key = BKE_pbvh_get_grid_key(pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;

      SubdivCCGCoord coord = {.grid_index = grid_index,
                              .x = vertex_index % key->grid_size,
                              .y = vertex_index / key->grid_size};

      SubdivCCGNeighbors neighbors;
      BKE_subdiv_ccg_neighbor_coords_get(pbvh->subdiv_ccg, &coord, false, &neighbors);

      float *co1 = CCG_elem_co(key, CCG_elem_offset(key, pbvh->grids[grid_index], vertex_index));
      float totw = 0.0f;
      int i = 0;

      for (i = 0; i < neighbors.size; i++) {
        SubdivCCGCoord *coord2 = neighbors.coords + i;

        int vertex_index2 = coord2->y * key->grid_size + coord2->x;

        float *co2 = CCG_elem_co(
            key, CCG_elem_offset(key, pbvh->grids[coord2->grid_index], vertex_index2));
        float w = len_v3v3(co1, co2);

        r_areas[i] = w;
        totw += w;
      }

      if (neighbors.size != valence) {
        printf("%s: error!\n", __func__);
      }
      if (totw < 0.000001f) {
        for (int i = 0; i < neighbors.size; i++) {
          r_areas[i] = 1.0f;
        }
      }

      for (; i < valence; i++) {
        r_areas[i] = 1.0f;
      }

      break;
    }
  }
}

void BKE_pbvh_set_stroke_id(PBVH *pbvh, int stroke_id)
{
  pbvh->stroke_id = stroke_id;
}

static void pbvh_boundaries_flag_update(PBVH *pbvh)
{

  if (pbvh->header.bm) {
    BMVert *v;
    BMIter iter;

    BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
      pbvh_boundary_update_bmesh(pbvh, v);
    }
  }
  else {
    int totvert = pbvh->totvert;

    if (BKE_pbvh_type(pbvh) == PBVH_GRIDS) {
      totvert = BKE_pbvh_get_grid_num_verts(pbvh);
    }

    for (int i = 0; i < totvert; i++) {
      pbvh->boundary_flags[i] |= SCULPT_BOUNDARY_NEEDS_UPDATE;
    }
  }
}

void BKE_pbvh_set_symmetry(PBVH *pbvh, int symmetry, int boundary_symmetry)
{
  if (symmetry == pbvh->symmetry && boundary_symmetry == pbvh->boundary_symmetry) {
    return;
  }

  pbvh->symmetry = symmetry;
  pbvh->boundary_symmetry = boundary_symmetry;

  pbvh_boundaries_flag_update(pbvh);
}

void BKE_pbvh_set_sculpt_verts(PBVH *pbvh, struct MSculptVert *msculptverts)
{
  pbvh->msculptverts = msculptverts;
}

void BKE_pbvh_update_vert_boundary_grids(PBVH *pbvh,
                                         struct SubdivCCG *subdiv_ccg,
                                         PBVHVertRef vertex)
{
  MSculptVert *mv = pbvh->msculptverts + vertex.i;

  int *flags = pbvh->boundary_flags + vertex.i;
  *flags = 0;

  /* TODO: finish this function. */

  int index = (int)vertex.i;

  /* TODO: optimize this. We could fill #SculptVertexNeighborIter directly,
   * maybe provide coordinate and mask pointers directly rather than converting
   * back and forth between #CCGElem and global index. */
  const CCGKey *key = BKE_pbvh_get_grid_key(pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;

  SubdivCCGCoord coord = {.grid_index = grid_index,
                          .x = vertex_index % key->grid_size,
                          .y = vertex_index / key->grid_size};

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, &coord, false, &neighbors);

  mv->valence = neighbors.size;
  mv->flag &= ~SCULPTFLAG_NEED_VALENCE;
}

void BKE_pbvh_update_vert_boundary_faces(int *boundary_flags,
                                         const int *face_sets,
                                         const bool *hide_poly,
                                         const float (*vert_positions)[3],
                                         const MEdge *medge,
                                         const MLoop *mloop,
                                         const MPoly *mpoly,
                                         MSculptVert *msculptverts,
                                         const MeshElemMap *pmap,
                                         PBVHVertRef vertex,
                                         const bool *sharp_edges)
{
  MSculptVert *mv = msculptverts + vertex.i;
  const MeshElemMap *vert_map = &pmap[vertex.i];

  mv->flag &= ~SCULPTFLAG_VERT_FSET_HIDDEN;

  int last_fset = -1;
  int last_fset2 = -1;

  int *flags = boundary_flags + vertex.i;
  *flags = 0;

  int totsharp = 0, totseam = 0;
  int visible = false;

  for (int i = 0; i < vert_map->count; i++) {
    int f_i = vert_map->indices[i];

    const MPoly *mp = mpoly + f_i;
    const MLoop *ml = mloop + mp->loopstart;
    int j = 0;

    for (j = 0; j < mp->totloop; j++, ml++) {
      if (ml->v == (int)vertex.i) {
        break;
      }
    }

    if (j < mp->totloop) {
      const MEdge *me = medge + ml->e;
      if (sharp_edges && sharp_edges[ml->e]) {
        *flags |= SCULPT_BOUNDARY_SHARP_MARK;
        totsharp++;
      }

      if (me->flag & ME_SEAM) {
        *flags |= SCULPT_BOUNDARY_SEAM;
        totseam++;
      }
    }

    int fset = face_sets ? abs(face_sets[f_i]) : 1;

    if (!hide_poly || !hide_poly[f_i]) {
      visible = true;
    }

    if (i > 0 && fset != last_fset) {
      *flags |= SCULPT_BOUNDARY_FACE_SET;

      if (i > 1 && last_fset2 != last_fset && last_fset != -1 && last_fset2 != -1 && fset != -1 &&
          last_fset2 != fset)
      {
        *flags |= SCULPT_CORNER_FACE_SET;
      }
    }

    if (i > 0 && last_fset != fset) {
      last_fset2 = last_fset;
    }

    last_fset = fset;
  }

  if (!visible) {
    mv->flag |= SCULPTFLAG_VERT_FSET_HIDDEN;
  }

  if (totsharp > 2) {
    *flags |= SCULPT_CORNER_SHARP_MARK;
  }

  if (totseam > 2) {
    *flags |= SCULPT_CORNER_SEAM;
  }
}

void BKE_pbvh_ignore_uvs_set(PBVH *pbvh, bool value)
{
  if (!!(pbvh->flags & PBVH_IGNORE_UVS) == value) {
    return;  // no change
  }

  if (value) {
    pbvh->flags |= PBVH_IGNORE_UVS;
  }
  else {
    pbvh->flags &= ~PBVH_IGNORE_UVS;
  }

  pbvh_boundaries_flag_update(pbvh);
}

bool BKE_pbvh_cache(const struct Mesh *me, PBVH *pbvh)
{
  memset(&pbvh->cached_data, 0, sizeof(pbvh->cached_data));

  if (pbvh->invalid) {
    printf("invalid pbvh!\n");
    return false;
  }

  switch (pbvh->header.type) {
    case PBVH_BMESH:
      if (!pbvh->header.bm) {
        return false;
      }

      pbvh->cached_data.bm = pbvh->header.bm;

      pbvh->cached_data.vdata = pbvh->header.bm->vdata;
      pbvh->cached_data.edata = pbvh->header.bm->edata;
      pbvh->cached_data.ldata = pbvh->header.bm->ldata;
      pbvh->cached_data.pdata = pbvh->header.bm->pdata;

      pbvh->cached_data.totvert = pbvh->header.bm->totvert;
      pbvh->cached_data.totedge = pbvh->header.bm->totedge;
      pbvh->cached_data.totloop = pbvh->header.bm->totloop;
      pbvh->cached_data.totpoly = pbvh->header.bm->totface;
      break;
    case PBVH_GRIDS:
      pbvh->cached_data.vdata = me->vdata;
      pbvh->cached_data.edata = me->edata;
      pbvh->cached_data.ldata = me->ldata;
      pbvh->cached_data.pdata = me->pdata;

      int grid_side = pbvh->gridkey.grid_size;

      pbvh->cached_data.totvert = pbvh->totgrid * grid_side * grid_side;
      pbvh->cached_data.totedge = me->totedge;
      pbvh->cached_data.totloop = me->totloop;
      pbvh->cached_data.totpoly = pbvh->totgrid * (grid_side - 1) * (grid_side - 1);
      break;
    case PBVH_FACES:
      pbvh->cached_data.vdata = me->vdata;
      pbvh->cached_data.edata = me->edata;
      pbvh->cached_data.ldata = me->ldata;
      pbvh->cached_data.pdata = me->pdata;

      pbvh->cached_data.totvert = me->totvert;
      pbvh->cached_data.totedge = me->totedge;
      pbvh->cached_data.totloop = me->totloop;
      pbvh->cached_data.totpoly = me->totpoly;
      break;
  }

  return true;
}

static bool customdata_is_same(const CustomData *a, const CustomData *b)
{
  return memcmp(a, b, sizeof(CustomData)) == 0;
}

bool BKE_pbvh_cache_is_valid(const struct Object *ob,
                             const struct Mesh *me,
                             const PBVH *pbvh,
                             int pbvh_type)
{
  if (pbvh->invalid) {
    printf("pbvh invalid!\n");
    return false;
  }

  if (pbvh->header.type != pbvh_type) {
    return false;
  }

  bool ok = true;
  int totvert = 0, totedge = 0, totloop = 0, totpoly = 0;
  const CustomData *vdata, *edata, *ldata, *pdata;

  MultiresModifierData *mmd = NULL;

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Multires) {
      mmd = (MultiresModifierData *)md;
      break;
    }
  }

  if (mmd && (mmd->flags & eModifierMode_Realtime)) {
    // return false;
  }

  switch (pbvh_type) {
    case PBVH_BMESH:
      if (!pbvh->header.bm || pbvh->header.bm != pbvh->cached_data.bm) {
        return false;
      }

      totvert = pbvh->header.bm->totvert;
      totedge = pbvh->header.bm->totedge;
      totloop = pbvh->header.bm->totloop;
      totpoly = pbvh->header.bm->totface;

      vdata = &pbvh->header.bm->vdata;
      edata = &pbvh->header.bm->edata;
      ldata = &pbvh->header.bm->ldata;
      pdata = &pbvh->header.bm->pdata;
      break;
    case PBVH_FACES:
      totvert = me->totvert;
      totedge = me->totedge;
      totloop = me->totloop;
      totpoly = me->totpoly;

      vdata = &me->vdata;
      edata = &me->edata;
      ldata = &me->ldata;
      pdata = &me->pdata;
      break;
    case PBVH_GRIDS: {
      if (!mmd) {
        return false;
      }

      int grid_side = 1 + (1 << (mmd->sculptlvl - 1));

      totvert = me->totloop * grid_side * grid_side;
      totedge = me->totedge;
      totloop = me->totloop;
      totpoly = me->totloop * (grid_side - 1) * (grid_side - 1);

      vdata = &me->vdata;
      edata = &me->edata;
      ldata = &me->ldata;
      pdata = &me->pdata;
      break;
    }
  }

  ok = ok && totvert == pbvh->cached_data.totvert;
  ok = ok && totedge == pbvh->cached_data.totedge;
  ok = ok && totloop == pbvh->cached_data.totloop;
  ok = ok && totpoly == pbvh->cached_data.totpoly;

  ok = ok && customdata_is_same(vdata, &pbvh->cached_data.vdata);
  ok = ok && customdata_is_same(edata, &pbvh->cached_data.edata);
  ok = ok && customdata_is_same(ldata, &pbvh->cached_data.ldata);
  ok = ok && customdata_is_same(pdata, &pbvh->cached_data.pdata);

  return ok;
}

GHash *cached_pbvhs = NULL;
static void pbvh_clear_cached_pbvhs(PBVH *exclude)
{
  PBVH **pbvhs = NULL;
  BLI_array_staticdeclare(pbvhs, 8);

  GHashIterator iter;
  GHASH_ITER (iter, cached_pbvhs) {
    PBVH *pbvh = BLI_ghashIterator_getValue(&iter);

    if (pbvh != exclude) {
      BLI_array_append(pbvhs, pbvh);
    }
  }

  for (int i = 0; i < BLI_array_len(pbvhs); i++) {
    PBVH *pbvh = pbvhs[i];

    if (pbvh->header.bm) {
      BM_mesh_free(pbvh->header.bm);
    }

    BKE_pbvh_free(pbvh);
  }

  BLI_array_free(pbvhs);
  BLI_ghash_clear(cached_pbvhs, MEM_freeN, NULL);
}

void BKE_pbvh_clear_cache(PBVH *preserve)
{
  pbvh_clear_cached_pbvhs(NULL);
}

#define PBVH_CACHE_KEY_SIZE 1024

static void pbvh_make_cached_key(Object *ob, char out[PBVH_CACHE_KEY_SIZE])
{
  sprintf(out, "%s:%p", ob->id.name, G.main);
}

void BKE_pbvh_invalidate_cache(Object *ob)
{
  Object *ob_orig = DEG_get_original_object(ob);

  char key[PBVH_CACHE_KEY_SIZE];
  pbvh_make_cached_key(ob_orig, key);
}

PBVH *BKE_pbvh_get_or_free_cached(Object *ob, Mesh *me, PBVHType pbvh_type)
{
  Object *ob_orig = DEG_get_original_object(ob);

  char key[PBVH_CACHE_KEY_SIZE];
  pbvh_make_cached_key(ob_orig, key);

  PBVH *pbvh = BLI_ghash_lookup(cached_pbvhs, key);

  if (!pbvh) {
    return NULL;
  }

  if (BKE_pbvh_cache_is_valid(ob, me, pbvh, pbvh_type)) {
    switch (pbvh_type) {
      case PBVH_BMESH:
        break;
      case PBVH_FACES:
        pbvh->vert_normals = BKE_mesh_vertex_normals_for_write(me);
      case PBVH_GRIDS:
        if (!pbvh->deformed) {
          pbvh->vert_positions = BKE_mesh_vert_positions_for_write(me);
        }

        pbvh->mloop = me->mloop;
        pbvh->mpoly = me->mpoly;
        pbvh->vdata = &me->vdata;
        pbvh->ldata = &me->ldata;
        pbvh->pdata = &me->pdata;

        pbvh->face_sets = (int *)CustomData_get_layer_named(
            &me->pdata, CD_PROP_INT32, ".sculpt_face_set");

        break;
    }

    BKE_pbvh_update_active_vcol(pbvh, me);

    return pbvh;
  }

  pbvh_clear_cached_pbvhs(NULL);
  return NULL;
}

void BKE_pbvh_set_cached(Object *ob, PBVH *pbvh)
{
  if (!pbvh) {
    return;
  }

  Object *ob_orig = DEG_get_original_object(ob);

  char key[PBVH_CACHE_KEY_SIZE];
  pbvh_make_cached_key(ob_orig, key);

  PBVH *exist = BLI_ghash_lookup(cached_pbvhs, key);

  if (pbvh->invalid) {
    printf("pbvh invalid!");
  }

  if (exist && exist->invalid) {
    printf("pbvh invalid!");
  }

  if (!exist || exist != pbvh) {
    pbvh_clear_cached_pbvhs(pbvh);

    char key[PBVH_CACHE_KEY_SIZE];
    pbvh_make_cached_key(ob_orig, key);

    BLI_ghash_insert(cached_pbvhs, BLI_strdup(key), pbvh);
  }

#ifdef WITH_PBVH_CACHE
  BKE_pbvh_cache(BKE_object_get_original_mesh(ob_orig), pbvh);
#endif
}

struct SculptPMap *BKE_pbvh_get_pmap(PBVH *pbvh)
{
  return pbvh->pmap;
}

void BKE_pbvh_set_pmap(PBVH *pbvh, SculptPMap *pmap)
{
  if (pbvh->pmap != pmap) {
    BKE_pbvh_pmap_aquire(pmap);
  }

  pbvh->pmap = pmap;
}

/** Does not free pbvh itself. */
void BKE_pbvh_cache_remove(PBVH *pbvh)
{
  char **keys = NULL;
  BLI_array_staticdeclare(keys, 32);

  GHashIterator iter;
  GHASH_ITER (iter, cached_pbvhs) {
    PBVH *pbvh2 = BLI_ghashIterator_getValue(&iter);

    if (pbvh2 == pbvh) {
      BLI_array_append(keys, (char *)BLI_ghashIterator_getKey(&iter));
      break;
    }
  }

  for (int i = 0; i < BLI_array_len(keys); i++) {
    BLI_ghash_remove(cached_pbvhs, keys[i], MEM_freeN, NULL);
  }

  BLI_array_free(keys);
}

void BKE_pbvh_set_bmesh(PBVH *pbvh, BMesh *bm)
{
  pbvh->header.bm = bm;
}

void BKE_pbvh_free_bmesh(PBVH *pbvh, BMesh *bm)
{
  if (pbvh) {
    pbvh->header.bm = NULL;
  }

  BM_mesh_free(bm);

  GHashIterator iter;
  char **keys = NULL;
  BLI_array_staticdeclare(keys, 32);

  PBVH **pbvhs = NULL;
  BLI_array_staticdeclare(pbvhs, 8);

  GHASH_ITER (iter, cached_pbvhs) {
    PBVH *pbvh2 = BLI_ghashIterator_getValue(&iter);

    if (pbvh2->header.bm == bm) {
      pbvh2->header.bm = NULL;

      if (pbvh2 != pbvh) {
        bool ok = true;

        for (int i = 0; i < BLI_array_len(pbvhs); i++) {
          if (pbvhs[i] == pbvh2) {
            ok = false;
          }
        }

        if (ok) {
          BLI_array_append(pbvhs, pbvh2);
        }
      }

      BLI_array_append(keys, BLI_ghashIterator_getKey(&iter));
    }
  }

  for (int i = 0; i < BLI_array_len(keys); i++) {
    BLI_ghash_remove(cached_pbvhs, keys[i], MEM_freeN, NULL);
  }

  for (int i = 0; i < BLI_array_len(pbvhs); i++) {
    BKE_pbvh_free(pbvhs[i]);
  }

  BLI_array_free(pbvhs);
  BLI_array_free(keys);
}

BMLog *BKE_pbvh_get_bm_log(PBVH *pbvh)
{
  return pbvh->bm_log;
}

SculptPMap *BKE_pbvh_make_pmap(const struct Mesh *me)
{
  SculptPMap *pmap = MEM_callocN(sizeof(*pmap), "SculptPMap");

  BKE_mesh_vert_poly_map_create(&pmap->pmap,
                                &pmap->pmap_mem,
                                BKE_mesh_polys(me),
                                BKE_mesh_loops(me),
                                me->totvert,
                                me->totpoly,
                                me->totloop);

  pmap->refcount = 1;

  return pmap;
}

void BKE_pbvh_pmap_aquire(SculptPMap *pmap)
{
  pmap->refcount++;
}

bool BKE_pbvh_pmap_release(SculptPMap *pmap)
{
  if (!pmap) {
    return false;
  }

  pmap->refcount--;

  // if (pmap->refcount < 0) {
  //  printf("%s: error!\n", __func__);
  //}

  if (1 && pmap->refcount == 0) {
    MEM_SAFE_FREE(pmap->pmap);
    MEM_SAFE_FREE(pmap->pmap_mem);
    MEM_SAFE_FREE(pmap);

    return true;
  }

  return false;
}

bool BKE_pbvh_is_drawing(const PBVH *pbvh)
{
  return pbvh->is_drawing;
}

bool BKE_pbvh_draw_cache_invalid(const PBVH *pbvh)
{
  return pbvh->draw_cache_invalid;
}

void BKE_pbvh_is_drawing_set(PBVH *pbvh, bool val)
{
  pbvh->is_drawing = val;
}

void BKE_pbvh_node_num_loops(PBVH *pbvh, PBVHNode *node, int *r_totloop)
{
  UNUSED_VARS(pbvh);
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  if (r_totloop) {
    *r_totloop = node->loop_indices_num;
  }
}

void BKE_pbvh_update_active_vcol(PBVH *pbvh, const Mesh *mesh)
{
  CustomDataLayer *last_layer = pbvh->color_layer;

  Mesh me_query;
  const CustomData *vdata, *ldata;

  if (pbvh->header.type == PBVH_BMESH && pbvh->header.bm) {
    vdata = &pbvh->header.bm->vdata;
    ldata = &pbvh->header.bm->ldata;
  }
  else {
    vdata = &mesh->vdata;
    ldata = &mesh->ldata;
  }

  BKE_id_attribute_copy_domains_temp(ID_ME, vdata, NULL, ldata, NULL, NULL, &me_query.id);
  me_query.active_color_attribute = mesh->active_color_attribute;

  BKE_pbvh_get_color_layer(&me_query, &pbvh->color_layer, &pbvh->color_domain);

  if (pbvh->color_layer && pbvh->header.bm) {
    pbvh->cd_vcol_offset = pbvh->color_layer->offset;
  }
  else {
    pbvh->cd_vcol_offset = -1;
  }

  if (pbvh->color_layer != last_layer) {
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if (node->flag & PBVH_Leaf) {
        BKE_pbvh_node_mark_update_color(node);
      }
    }
  }
}

void BKE_pbvh_ensure_node_loops(PBVH *pbvh)
{
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  int totloop = 0;

  /* Check if nodes already have loop indices. */
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    if (node->loop_indices) {
      return;
    }

    totloop += node->totprim * 3;
  }

  BLI_bitmap *visit = BLI_BITMAP_NEW(totloop, __func__);

  /* Create loop indices from node loop triangles. */
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    node->loop_indices = MEM_malloc_arrayN(node->totprim * 3, sizeof(int), __func__);
    node->loop_indices_num = 0;

    for (int j = 0; j < (int)node->totprim; j++) {
      const MLoopTri *mlt = pbvh->looptri + node->prim_indices[j];

      for (int k = 0; k < 3; k++) {
        if (!BLI_BITMAP_TEST(visit, mlt->tri[k])) {
          node->loop_indices[node->loop_indices_num++] = mlt->tri[k];
          BLI_BITMAP_ENABLE(visit, mlt->tri[k]);
        }
      }
    }
  }

  MEM_SAFE_FREE(visit);
}

bool BKE_pbvh_get_origvert(
    PBVH *pbvh, PBVHVertRef vertex, const float **r_co, float **r_no, float **r_color)
{
  MSculptVert *mv;

  switch (pbvh->header.type) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      mv = pbvh->msculptverts + vertex.i;

      if (mv->stroke_id != pbvh->stroke_id) {
        mv->stroke_id = pbvh->stroke_id;
        float *mask = NULL;

        if (pbvh->header.type == PBVH_FACES) {
          copy_v3_v3(mv->origco, pbvh->vert_positions[vertex.i]);
          copy_v3_v3(mv->origno, pbvh->vert_normals[vertex.i]);
          mask = (float *)CustomData_get_layer(pbvh->vdata, CD_PAINT_MASK);

          if (mask) {
            mask += vertex.i;
          }
        }
        else {
          const CCGKey *key = BKE_pbvh_get_grid_key(pbvh);
          const int grid_index = vertex.i / key->grid_area;
          const int vertex_index = vertex.i - grid_index * key->grid_area;
          CCGElem *elem = BKE_pbvh_get_grids(pbvh)[grid_index];

          copy_v3_v3(mv->origco, CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index)));
          copy_v3_v3(mv->origno, CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index)));
          mask = key->has_mask ? CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index)) :
                                 NULL;
        }

        if (mask) {
          mv->origmask = (ushort)(*mask * 65535.0f);
        }

        if (pbvh->color_layer) {
          BKE_pbvh_vertex_color_get(pbvh, vertex, mv->origcolor);
        }
      }
      break;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      mv = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v);

      if (mv->stroke_id != pbvh->stroke_id) {
        mv->stroke_id = pbvh->stroke_id;

        copy_v3_v3(mv->origco, v->co);
        copy_v3_v3(mv->origno, v->no);

        if (pbvh->cd_vert_mask_offset != -1) {
          mv->origmask = (short)(BM_ELEM_CD_GET_FLOAT(v, pbvh->cd_vert_mask_offset) * 65535.0f);
        }

        if (pbvh->cd_vcol_offset != -1) {
          BKE_pbvh_vertex_color_get(pbvh, vertex, mv->origcolor);
        }
      }
      break;
    }
  }

  if (r_co) {
    *r_co = mv->origco;
  }

  if (r_no) {
    *r_no = mv->origno;
  }

  if (r_color) {
    *r_color = mv->origcolor;
  }

  return true;
}

int BKE_pbvh_debug_draw_gen_get(PBVHNode *node)
{
  return node->debug_draw_gen;
}

void BKE_pbvh_set_boundary_flags(PBVH *pbvh, int *boundary_flags)
{
  pbvh->boundary_flags = boundary_flags;
}

static void pbvh_face_iter_verts_reserve(PBVHFaceIter *fd, int verts_num)
{
  if (verts_num >= fd->verts_size_) {
    fd->verts_size_ = (verts_num + 1) << 2;

    if (fd->verts != fd->verts_reserved_) {
      MEM_SAFE_FREE(fd->verts);
    }

    fd->verts = MEM_malloc_arrayN(fd->verts_size_, sizeof(void *), __func__);
  }

  fd->verts_num = verts_num;
}

BLI_INLINE int face_iter_prim_to_face(PBVHFaceIter *fd, int prim_index)
{
  if (fd->subdiv_ccg_) {
    return BKE_subdiv_ccg_grid_to_face_index(fd->subdiv_ccg_, prim_index);
  }

  return fd->looptri_[prim_index].poly;
}

static void pbvh_face_iter_step(PBVHFaceIter *fd, bool do_step)
{
  if (do_step) {
    fd->i++;
  }

  switch (fd->pbvh_type_) {
    case PBVH_BMESH: {
      if (do_step) {
        fd->bm_faces_iter_++;

        while (fd->bm_faces_iter_ < fd->bm_faces_->cur &&
               !fd->bm_faces_->elems[fd->bm_faces_iter_]) {
          fd->bm_faces_iter_++;
        }

        if (fd->bm_faces_iter_ >= fd->bm_faces_->cur) {
          return;
        }
      }

      BMFace *f = (BMFace *)fd->bm_faces_->elems[fd->bm_faces_iter_];
      fd->face.i = (intptr_t)f;
      fd->index = f->head.index;

      if (fd->cd_face_set_ != -1) {
        fd->face_set = (int *)BM_ELEM_CD_GET_VOID_P(f, fd->cd_face_set_);
      }

      /* TODO: BMesh doesn't yet use .hide_poly. */
      fd->hide = nullptr;

      pbvh_face_iter_verts_reserve(fd, f->len);
      int vertex_i = 0;

      BMLoop *l = f->l_first;
      do {
        fd->verts[vertex_i++].i = (intptr_t)l->v;
      } while ((l = l->next) != f->l_first);

      break;
    }
    case PBVH_GRIDS:
    case PBVH_FACES: {
      int face_index = 0;

      if (do_step) {
        fd->prim_index_++;

        while (fd->prim_index_ < fd->node_->totprim) {
          face_index = face_iter_prim_to_face(fd, fd->node_->prim_indices[fd->prim_index_]);

          if (face_index != fd->last_face_index_) {
            break;
          }

          fd->prim_index_++;
        }
      }
      else if (fd->prim_index_ < fd->node_->totprim) {
        face_index = face_iter_prim_to_face(fd, fd->node_->prim_indices[fd->prim_index_]);
      }

      if (fd->prim_index_ >= fd->node_->totprim) {
        return;
      }

      fd->last_face_index_ = face_index;
      const MPoly *mp = fd->mpoly_ + face_index;

      fd->face.i = fd->index = face_index;

      if (fd->face_sets_) {
        fd->face_set = fd->face_sets_ + face_index;
      }
      if (fd->hide_poly_) {
        fd->hide = fd->hide_poly_ + face_index;
      }

      pbvh_face_iter_verts_reserve(fd, mp->totloop);

      const MLoop *ml = fd->mloop_ + mp->loopstart;
      const int grid_area = fd->subdiv_key_.grid_area;

      for (int i = 0; i < mp->totloop; i++, ml++) {
        if (fd->pbvh_type_ == PBVH_GRIDS) {
          /* Grid corners. */
          fd->verts[i].i = (mp->loopstart + i) * grid_area + grid_area - 1;
        }
        else {
          fd->verts[i].i = ml->v;
        }
      }
      break;
    }
  }
}

void BKE_pbvh_face_iter_step(PBVHFaceIter *fd)
{
  pbvh_face_iter_step(fd, true);
}

void BKE_pbvh_face_iter_init(PBVH *pbvh, PBVHNode *node, PBVHFaceIter *fd)
{
  memset(fd, 0, sizeof(*fd));

  fd->node_ = node;
  fd->pbvh_type_ = BKE_pbvh_type(pbvh);
  fd->verts = fd->verts_reserved_;
  fd->verts_size_ = PBVH_FACE_ITER_VERTS_RESERVED;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_GRIDS:
      fd->subdiv_ccg_ = pbvh->subdiv_ccg;
      fd->subdiv_key_ = pbvh->gridkey;
      ATTR_FALLTHROUGH;
    case PBVH_FACES:
      fd->mpoly_ = pbvh->mpoly;
      fd->mloop_ = pbvh->mloop;
      fd->looptri_ = pbvh->looptri;
      fd->hide_poly_ = pbvh->hide_poly;
      fd->face_sets_ = pbvh->face_sets;
      fd->last_face_index_ = -1;

      break;
    case PBVH_BMESH:
      fd->bm = pbvh->header.bm;
      fd->cd_face_set_ = CustomData_get_offset_named(
          &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

      fd->bm_faces_iter_ = 0;
      fd->bm_faces_ = node->bm_faces;
      break;
  }

  if (!BKE_pbvh_face_iter_done(fd)) {
    pbvh_face_iter_step(fd, false);
  }
}

void BKE_pbvh_face_iter_finish(PBVHFaceIter *fd)
{
  if (fd->verts != fd->verts_reserved_) {
    MEM_SAFE_FREE(fd->verts);
  }
}

bool BKE_pbvh_face_iter_done(PBVHFaceIter *fd)
{
  switch (fd->pbvh_type_) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return fd->prim_index_ >= fd->node_->totprim;
    case PBVH_BMESH:
      return fd->bm_faces_iter_ >= fd->bm_faces_->cur;
    default:
      BLI_assert_unreachable();
      return true;
  }
}

void BKE_pbvh_sync_visibility_from_verts(PBVH *pbvh, Mesh *mesh)
{
  switch (pbvh->header.type) {
    case PBVH_FACES: {
      BKE_mesh_flush_hidden_from_verts(mesh);
      BKE_pbvh_update_hide_attributes_from_mesh(pbvh);
      break;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMVert *v;
      BMEdge *e;
      BMFace *f;

      BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (e, &iter, pbvh->header.bm, BM_EDGES_OF_MESH) {
        BM_elem_flag_disable(e, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
          continue;
        }
        BMIter iter_l;
        BMLoop *l;

        BM_ITER_ELEM (l, &iter_l, v, BM_LOOPS_OF_VERT) {
          BM_elem_flag_enable(l->e, BM_ELEM_HIDDEN);
          BM_elem_flag_enable(l->f, BM_ELEM_HIDDEN);
        }
      }
      break;
    }
    case PBVH_GRIDS: {
      const MPoly *mp = BKE_mesh_polys(mesh);
      CCGKey key = pbvh->gridkey;

      bool *hide_poly = (bool *)CustomData_get_layer_named_for_write(
          &mesh->pdata, CD_PROP_BOOL, ".hide_poly", mesh->totpoly);

      bool delete_hide_poly = true;
      for (int face_index = 0; face_index < mesh->totpoly; face_index++, mp++) {
        bool hidden = false;

        for (int loop_index = 0; !hidden && loop_index < mp->totloop; loop_index++) {
          int grid_index = mp->loopstart + loop_index;

          if (pbvh->grid_hidden[grid_index] &&
              BLI_BITMAP_TEST(pbvh->grid_hidden[grid_index], key.grid_area - 1))
          {
            hidden = true;

            break;
          }
        }

        if (hidden && !hide_poly) {
          hide_poly = (bool *)CustomData_get_layer_named_for_write(
              &mesh->pdata, CD_PROP_BOOL, ".hide_poly", mesh->totpoly);

          if (!hide_poly) {
            CustomData_add_layer_named(
                &mesh->pdata, CD_PROP_BOOL, CD_CONSTRUCT, NULL, mesh->totpoly, ".hide_poly");

            hide_poly = (bool *)CustomData_get_layer_named_for_write(
                &mesh->pdata, CD_PROP_BOOL, ".hide_poly", mesh->totpoly);
          }
        }

        if (hide_poly) {
          delete_hide_poly = delete_hide_poly && !hidden;
          hide_poly[face_index] = hidden;
        }
      }

      if (delete_hide_poly) {
        CustomData_free_layer_named(&mesh->pdata, ".hide_poly", mesh->totpoly);
      }

      BKE_mesh_flush_hidden_from_polys(mesh);
      BKE_pbvh_update_hide_attributes_from_mesh(pbvh);
      break;
    }
  }
}
