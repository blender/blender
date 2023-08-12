/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <climits>

#include "BLI_bitmap.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DRW_pbvh.hh"

#include "PIL_time.h"

#include "bmesh.h"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

using blender::float3;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;

#define LEAF_LIMIT 10000

/* Uncomment to test if triangles of the same face are
 * properly clustered into single nodes.
 */
//#define TEST_PBVH_FACE_SPLIT

/* Uncomment to test that faces are only assigned to one PBVHNode */
//#define VALIDATE_UNIQUE_NODE_FACES

//#define PERFCNTRS
#define STACK_FIXED_DEPTH 100

struct PBVHStack {
  PBVHNode *node;
  bool revisiting;
};

struct PBVHIter {
  PBVH *pbvh;
  BKE_pbvh_SearchCallback scb;
  void *search_data;

  PBVHStack *stack;
  int stacksize;

  PBVHStack stackfixed[STACK_FIXED_DEPTH];
  int stackspace;
};

void BB_reset(BB *bb)
{
  bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
  bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

void BB_expand(BB *bb, const float co[3])
{
  for (int i = 0; i < 3; i++) {
    bb->bmin[i] = min_ff(bb->bmin[i], co[i]);
    bb->bmax[i] = max_ff(bb->bmax[i], co[i]);
  }
}

void BB_expand_with_bb(BB *bb, const BB *bb2)
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
static void update_node_vb(PBVH *pbvh, PBVHNode *node)
{
  BB vb;

  BB_reset(&vb);

  if (node->flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_ALL) {
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

static bool face_materials_match(const int *material_indices,
                                 const bool *sharp_faces,
                                 const int a,
                                 const int b)
{
  if (material_indices) {
    if (material_indices[a] != material_indices[b]) {
      return false;
    }
  }
  if (sharp_faces) {
    if (sharp_faces[a] != sharp_faces[b]) {
      return false;
    }
  }
  return true;
}

static bool grid_materials_match(const DMFlagMat *f1, const DMFlagMat *f2)
{
  return (f1->sharp == f2->sharp) && (f1->mat_nr == f2->mat_nr);
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices_faces(blender::MutableSpan<int> prim_indices,
                                   int *prim_scratch,
                                   int lo,
                                   int hi,
                                   int axis,
                                   float mid,
                                   const Span<BBC> prim_bbc,
                                   const Span<int> looptri_faces)
{
  for (int i = lo; i < hi; i++) {
    prim_scratch[i - lo] = prim_indices[i];
  }

  int lo2 = lo, hi2 = hi - 1;
  int i1 = lo, i2 = 0;

  while (i1 < hi) {
    const int face_i = looptri_faces[prim_scratch[i2]];
    bool side = prim_bbc[prim_scratch[i2]].bcentroid[axis] >= mid;

    while (i1 < hi && looptri_faces[prim_scratch[i2]] == face_i) {
      prim_indices[side ? hi2-- : lo2++] = prim_scratch[i2];
      i1++;
      i2++;
    }
  }

  return lo2;
}

static int partition_indices_grids(blender::MutableSpan<int> prim_indices,
                                   int *prim_scratch,
                                   int lo,
                                   int hi,
                                   int axis,
                                   float mid,
                                   const Span<BBC> prim_bbc,
                                   SubdivCCG *subdiv_ccg)
{
  for (int i = lo; i < hi; i++) {
    prim_scratch[i - lo] = prim_indices[i];
  }

  int lo2 = lo, hi2 = hi - 1;
  int i1 = lo, i2 = 0;

  while (i1 < hi) {
    int face_i = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, prim_scratch[i2]);
    bool side = prim_bbc[prim_scratch[i2]].bcentroid[axis] >= mid;

    while (i1 < hi && BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, prim_scratch[i2]) == face_i) {
      prim_indices[side ? hi2-- : lo2++] = prim_scratch[i2];
      i1++;
      i2++;
    }
  }

  return lo2;
}

/* Returns the index of the first element on the right of the partition */
static int partition_indices_material(
    PBVH *pbvh, const int *material_indices, const bool *sharp_faces, int lo, int hi)
{
  const Span<int> looptri_faces = pbvh->looptri_faces;
  const DMFlagMat *flagmats = pbvh->grid_flag_mats;
  MutableSpan<int> indices = pbvh->prim_indices;
  int i = lo, j = hi;

  for (;;) {
    if (!pbvh->looptri_faces.is_empty()) {
      const int first = looptri_faces[pbvh->prim_indices[lo]];
      for (; face_materials_match(material_indices, sharp_faces, first, looptri_faces[indices[i]]);
           i++) {
        /* pass */
      }
      for (;
           !face_materials_match(material_indices, sharp_faces, first, looptri_faces[indices[j]]);
           j--) {
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

    std::swap(pbvh->prim_indices[i], pbvh->prim_indices[j]);
    i++;
  }
}

void pbvh_grow_nodes(PBVH *pbvh, int totnode)
{
  pbvh->nodes.resize(totnode);
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(
    PBVH *pbvh, blender::Map<int, int> &map, int *face_verts, int *uniq_verts, int vertex)
{
  return map.lookup_or_add_cb(vertex, [&]() {
    int value;
    if (!pbvh->vert_bitmap[vertex]) {
      pbvh->vert_bitmap[vertex] = true;
      value = *uniq_verts;
      (*uniq_verts)++;
    }
    else {
      value = ~(*face_verts);
      (*face_verts)++;
    }
    return value;
  });
}

/* Find vertices used by the faces in this node and update the draw buffers */
static void build_mesh_leaf_node(PBVH *pbvh, PBVHNode *node)
{
  bool has_visible = false;

  node->uniq_verts = node->face_verts = 0;
  const int totface = node->prim_indices.size();

  /* reserve size is rough guess */
  blender::Map<int, int> map;
  map.reserve(totface);

  node->face_vert_indices.reinitialize(totface);

  for (int i = 0; i < totface; i++) {
    const MLoopTri *lt = &pbvh->looptri[node->prim_indices[i]];
    for (int j = 0; j < 3; j++) {
      node->face_vert_indices[i][j] = map_insert_vert(
          pbvh, map, &node->face_verts, &node->uniq_verts, pbvh->corner_verts[lt->tri[j]]);
    }

    if (has_visible == false) {
      if (!paint_is_face_hidden(
              pbvh->looptri_faces.data(), pbvh->hide_poly, node->prim_indices[i])) {
        has_visible = true;
      }
    }
  }

  node->vert_indices.reinitialize(node->uniq_verts + node->face_verts);

  /* Build the vertex list, unique verts first */
  for (const blender::MapItem<int, int> item : map.items()) {
    int value = item.value;
    if (value < 0) {
      value = -value + node->uniq_verts - 1;
    }

    node->vert_indices[value] = item.key;
  }

  for (int i = 0; i < totface; i++) {
    const int sides = 3;

    for (int j = 0; j < sides; j++) {
      if (node->face_vert_indices[i][j] < 0) {
        node->face_vert_indices[i][j] = -node->face_vert_indices[i][j] + node->uniq_verts - 1;
      }
    }
  }

  BKE_pbvh_node_mark_rebuild_draw(node);

  BKE_pbvh_node_fully_hidden_set(node, !has_visible);
}

static void update_vb(PBVH *pbvh, PBVHNode *node, const Span<BBC> prim_bbc, int offset, int count)
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

  int depth1 = int(log2(double(gridsize) - 1.0) + DBL_EPSILON);
  int depth2 = int(log2(double(display_gridsize) - 1.0) + DBL_EPSILON);

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
                                           node->prim_indices.data(),
                                           node->prim_indices.size(),
                                           pbvh->gridkey.grid_size,
                                           pbvh->gridkey.grid_size);
  BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
  BKE_pbvh_node_mark_rebuild_draw(node);
}

static void build_leaf(PBVH *pbvh, int node_index, const Span<BBC> prim_bbc, int offset, int count)
{
  pbvh->nodes[node_index].flag |= PBVH_Leaf;

  pbvh->nodes[node_index].prim_indices = pbvh->prim_indices.as_span().slice(offset, count);

  /* Still need vb for searches */
  update_vb(pbvh, &pbvh->nodes[node_index], prim_bbc, offset, count);

  if (!pbvh->looptri.is_empty()) {
    build_mesh_leaf_node(pbvh, &pbvh->nodes[node_index]);
  }
  else {
    build_grid_leaf_node(pbvh, &pbvh->nodes[node_index]);
  }
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static bool leaf_needs_material_split(
    PBVH *pbvh, const int *material_indices, const bool *sharp_faces, int offset, int count)
{
  if (count <= 1) {
    return false;
  }

  if (!pbvh->looptri.is_empty()) {
    const int first = pbvh->looptri_faces[pbvh->prim_indices[offset]];
    for (int i = offset + count - 1; i > offset; i--) {
      int prim = pbvh->prim_indices[i];
      if (!face_materials_match(material_indices, sharp_faces, first, pbvh->looptri_faces[prim])) {
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
          int face_i = pbvh->looptri_faces[node->prim_indices[j]];

          if (node_map[face_i] >= 0 && node_map[face_i] != i) {
            int old_i = node_map[face_i];
            int prim_i = node->prim_indices - pbvh->prim_indices + j;

            printf("PBVH split error; face: %d, prim_i: %d, node1: %d, node2: %d, totprim: %d\n",
                   face_i,
                   prim_i,
                   old_i,
                   i,
                   node->totprim);
          }

          node_map[face_i] = i;
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
                      const int *material_indices,
                      const bool *sharp_faces,
                      int node_index,
                      BB *cb,
                      const Span<BBC> prim_bbc,
                      int offset,
                      int count,
                      int *prim_scratch,
                      int depth)
{
  int end;
  BB cb_backing;

  if (!prim_scratch) {
    prim_scratch = static_cast<int *>(MEM_malloc_arrayN(pbvh->totprim, sizeof(int), __func__));
  }

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = count <= pbvh->leaf_limit || depth >= STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(pbvh, material_indices, sharp_faces, offset, count)) {
      build_leaf(pbvh, node_index, prim_bbc, offset, count);

      if (node_index == 0) {
        MEM_SAFE_FREE(prim_scratch);
      }

      return;
    }
  }

  /* Add two child nodes */
  pbvh->nodes[node_index].children_offset = pbvh->nodes.size();
  pbvh_grow_nodes(pbvh, pbvh->nodes.size() + 2);

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
                                    pbvh->looptri_faces);
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
    end = partition_indices_material(
        pbvh, material_indices, sharp_faces, offset, offset + count - 1);
  }

  /* Build children */
  build_sub(pbvh,
            material_indices,
            sharp_faces,
            pbvh->nodes[node_index].children_offset,
            nullptr,
            prim_bbc,
            offset,
            end - offset,
            prim_scratch,
            depth + 1);
  build_sub(pbvh,
            material_indices,
            sharp_faces,
            pbvh->nodes[node_index].children_offset + 1,
            nullptr,
            prim_bbc,
            end,
            offset + count - end,
            prim_scratch,
            depth + 1);

  if (node_index == 0) {
    MEM_SAFE_FREE(prim_scratch);
  }
}

static void pbvh_build(PBVH *pbvh,
                       const int *material_indices,
                       const bool *sharp_faces,
                       BB *cb,
                       const Span<BBC> prim_bbc,
                       int totprim)
{
  if (totprim != pbvh->totprim) {
    pbvh->totprim = totprim;
    pbvh->nodes.clear_and_shrink();

    pbvh->prim_indices.reinitialize(totprim);
    std::iota(pbvh->prim_indices.begin(), pbvh->prim_indices.end(), 0);
  }

  pbvh->nodes.resize(1);

  build_sub(pbvh, material_indices, sharp_faces, 0, cb, prim_bbc, 0, totprim, nullptr, 0);
}

static void pbvh_draw_args_init(PBVH *pbvh, PBVH_GPU_Args *args, PBVHNode *node)
{
  memset((void *)args, 0, sizeof(*args));

  args->pbvh_type = pbvh->header.type;
  args->mesh_grids_num = pbvh->totgrid;
  args->node = node;

  args->grid_hidden = pbvh->grid_hidden;
  args->face_sets_color_default = pbvh->face_sets_color_default;
  args->face_sets_color_seed = pbvh->face_sets_color_seed;
  args->vert_positions = pbvh->vert_positions;
  if (pbvh->mesh) {
    args->corner_verts = pbvh->corner_verts;
    args->corner_edges = pbvh->mesh->corner_edges();
  }
  args->faces = pbvh->faces;
  args->mlooptri = pbvh->looptri;

  if (ELEM(pbvh->header.type, PBVH_FACES, PBVH_GRIDS)) {
    args->hide_poly = pbvh->face_data ? static_cast<const bool *>(CustomData_get_layer_named(
                                            pbvh->face_data, CD_PROP_BOOL, ".hide_poly")) :
                                        nullptr;
  }

  switch (pbvh->header.type) {
    case PBVH_FACES:
      args->vert_data = pbvh->vert_data;
      args->loop_data = pbvh->loop_data;
      args->face_data = pbvh->face_data;
      args->me = pbvh->mesh;
      args->faces = pbvh->faces;
      args->vert_normals = pbvh->vert_normals;
      args->face_normals = pbvh->face_normals;

      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->prim_indices = node->prim_indices;
      args->face_sets = pbvh->face_sets;
      args->looptri_faces = pbvh->looptri_faces;
      break;
    case PBVH_GRIDS:
      args->vert_data = pbvh->vert_data;
      args->loop_data = pbvh->loop_data;
      args->face_data = pbvh->face_data;
      args->ccg_key = pbvh->gridkey;
      args->me = pbvh->mesh;
      args->grid_indices = node->prim_indices;
      args->subdiv_ccg = pbvh->subdiv_ccg;
      args->face_sets = pbvh->face_sets;
      args->faces = pbvh->faces;

      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->mesh_grids_num = pbvh->totgrid;
      args->grids = pbvh->grids;
      args->grid_flag_mats = pbvh->grid_flag_mats;
      args->vert_normals = pbvh->vert_normals;

      args->face_sets = pbvh->face_sets;
      args->looptri_faces = pbvh->looptri_faces;
      break;
    case PBVH_BMESH:
      args->bm = pbvh->header.bm;
      args->vert_data = &args->bm->vdata;
      args->loop_data = &args->bm->ldata;
      args->face_data = &args->bm->pdata;
      args->bm_faces = node->bm_faces;
      args->bm_other_verts = node->bm_other_verts;
      args->bm_unique_vert = node->bm_unique_verts;
      args->cd_mask_layer = CustomData_get_offset(&pbvh->header.bm->vdata, CD_PAINT_MASK);

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
      int face_i;

      if (pbvh->header.type == PBVH_FACES) {
        face_i = pbvh->looptri_faces[node->prim_indices[j]];
      }
      else {
        face_i = BKE_subdiv_ccg_grid_to_face_index(pbvh->subdiv_ccg, node->prim_indices[j]);
      }

      totface = max_ii(totface, face_i + 1);
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
      int face_i;

      if (pbvh->header.type == PBVH_FACES) {
        face_i = pbvh->looptri_faces[node->prim_indices[j]];
      }
      else {
        face_i = BKE_subdiv_ccg_grid_to_face_index(pbvh->subdiv_ccg, node->prim_indices[j]);
      }

      if (facemap[face_i] != -1 && facemap[face_i] != i) {
        printf("%s: error: face spanned multiple nodes (old: %d new: %d)\n",
               __func__,
               facemap[face_i],
               i);
      }

      facemap[face_i] = i;
    }
  }
  MEM_SAFE_FREE(facemap);
}
#endif

void BKE_pbvh_update_mesh_pointers(PBVH *pbvh, Mesh *mesh)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);

  pbvh->faces = mesh->faces();
  pbvh->corner_verts = mesh->corner_verts();
  pbvh->looptri_faces = mesh->looptri_faces();

  if (!pbvh->deformed) {
    /* Deformed positions not matching the original mesh are owned directly by the PBVH, and are
     * set separately by #BKE_pbvh_vert_coords_apply. */
    pbvh->vert_positions = mesh->vert_positions_for_write();
  }

  pbvh->hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_BOOL, ".hide_poly", mesh->faces_num));
  pbvh->hide_vert = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &mesh->vert_data, CD_PROP_BOOL, ".hide_vert", mesh->totvert));

  /* Make sure cached normals start out calculated. */
  mesh->vert_normals();
  mesh->face_normals();

  pbvh->vert_normals = mesh->runtime->vert_normals;
  pbvh->face_normals = mesh->runtime->face_normals;

  pbvh->vert_data = &mesh->vert_data;
  pbvh->loop_data = &mesh->loop_data;
  pbvh->face_data = &mesh->face_data;
}

void BKE_pbvh_build_mesh(PBVH *pbvh, Mesh *mesh)
{
  const int totvert = mesh->totvert;
  const int looptri_num = poly_to_tri_count(mesh->faces_num, mesh->totloop);
  MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
  const blender::OffsetIndices<int> faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  pbvh->looptri.reinitialize(looptri_num);

  blender::bke::mesh::looptris_calc(vert_positions, faces, corner_verts, pbvh->looptri);

  pbvh->mesh = mesh;
  pbvh->header.type = PBVH_FACES;

  BKE_pbvh_update_mesh_pointers(pbvh, mesh);

  /* Those are not set in #BKE_pbvh_update_mesh_pointers because they are owned by the #PBVH. */
  pbvh->vert_bitmap = blender::Array<bool>(totvert, false);
  pbvh->totvert = totvert;

#ifdef TEST_PBVH_FACE_SPLIT
  /* Use lower limit to increase probability of
   * edge cases.
   */
  pbvh->leaf_limit = 100;
#else
  pbvh->leaf_limit = LEAF_LIMIT;
#endif

  pbvh->faces_num = mesh->faces_num;

  pbvh->face_sets_color_seed = mesh->face_sets_color_seed;
  pbvh->face_sets_color_default = mesh->face_sets_color_default;

  /* For each face, store the AABB and the AABB centroid */
  blender::Array<BBC> prim_bbc(looptri_num);
  BB cb;
  BB_reset(&cb);
  cb = blender::threading::parallel_reduce(
      pbvh->looptri.index_range(),
      1024,
      cb,
      [&](const blender::IndexRange range, const BB &init) {
        BB current = init;
        for (const int i : range) {
          const MLoopTri &lt = pbvh->looptri[i];
          BBC *bbc = &prim_bbc[i];
          BB_reset((BB *)bbc);
          for (int j = 0; j < 3; j++) {
            BB_expand((BB *)bbc, vert_positions[pbvh->corner_verts[lt.tri[j]]]);
          }
          BBC_update_centroid(bbc);
          BB_expand(&current, bbc->bcentroid);
        }
        return current;
      },
      [](const BB &a, const BB &b) {
        BB current = a;
        BB_expand_with_bb(&current, &b);
        return current;
      });

  if (looptri_num) {
    const int *material_indices = static_cast<const int *>(
        CustomData_get_layer_named(&mesh->face_data, CD_PROP_INT32, "material_index"));
    const bool *sharp_faces = (const bool *)CustomData_get_layer_named(
        &mesh->face_data, CD_PROP_BOOL, "sharp_face");
    pbvh_build(pbvh, material_indices, sharp_faces, &cb, prim_bbc, looptri_num);

#ifdef TEST_PBVH_FACE_SPLIT
    test_face_boundaries(pbvh);
#endif
  }

  /* Clear the bitmap so it can be used as an update tag later on. */
  pbvh->vert_bitmap.fill(false);

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
                          Mesh *me,
                          SubdivCCG *subdiv_ccg)
{
  const int gridsize = key->grid_size;

  pbvh->header.type = PBVH_GRIDS;
  pbvh->grids = grids;
  pbvh->gridfaces = gridfaces;
  pbvh->grid_flag_mats = flagmats;
  pbvh->totgrid = totgrid;
  pbvh->gridkey = *key;
  pbvh->grid_hidden = grid_hidden;
  pbvh->subdiv_ccg = subdiv_ccg;
  pbvh->faces_num = me->faces_num;

  /* Find maximum number of grids per face. */
  int max_grids = 1;
  const blender::OffsetIndices faces = me->faces();
  for (const int i : faces.index_range()) {
    max_grids = max_ii(max_grids, faces[i].size());
  }

  /* Ensure leaf limit is at least 4 so there's room
   * to split at original face boundaries.
   * Fixes #102209.
   */
  pbvh->leaf_limit = max_ii(LEAF_LIMIT / (gridsize * gridsize), max_grids);

  /* We need the base mesh attribute layout for PBVH draw. */
  pbvh->vert_data = &me->vert_data;
  pbvh->loop_data = &me->loop_data;
  pbvh->face_data = &me->face_data;

  pbvh->faces = faces;
  pbvh->corner_verts = me->corner_verts();

  /* We also need the base mesh for PBVH draw. */
  pbvh->mesh = me;

  /* For each grid, store the AABB and the AABB centroid */
  blender::Array<BBC> prim_bbc(totgrid);
  BB cb;
  BB_reset(&cb);
  cb = blender::threading::parallel_reduce(
      blender::IndexRange(totgrid),
      1024,
      cb,
      [&](const blender::IndexRange range, const BB &init) {
        BB current = init;
        for (const int i : range) {
          CCGElem *grid = grids[i];
          BBC *bbc = &prim_bbc[i];
          BB_reset((BB *)bbc);
          for (int j = 0; j < gridsize * gridsize; j++) {
            BB_expand((BB *)bbc, CCG_elem_offset_co(key, grid, j));
          }
          BBC_update_centroid(bbc);
          BB_expand(&current, bbc->bcentroid);
        }
        return current;
      },
      [](const BB &a, const BB &b) {
        BB current = a;
        BB_expand_with_bb(&current, &b);
        return current;
      });

  if (totgrid) {
    const int *material_indices = static_cast<const int *>(
        CustomData_get_layer_named(&me->face_data, CD_PROP_INT32, "material_index"));
    const bool *sharp_faces = (const bool *)CustomData_get_layer_named(
        &me->face_data, CD_PROP_BOOL, "sharp_face");
    pbvh_build(pbvh, material_indices, sharp_faces, &cb, prim_bbc, totgrid);

#ifdef TEST_PBVH_FACE_SPLIT
    test_face_boundaries(pbvh);
#endif
  }

#ifdef VALIDATE_UNIQUE_NODE_FACES
  pbvh_validate_node_prims(pbvh);
#endif
}

PBVH *BKE_pbvh_new(PBVHType type)
{
  PBVH *pbvh = MEM_new<PBVH>(__func__);
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
  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_Leaf) {
      if (node.draw_batches) {
        DRW_pbvh_node_free(node.draw_batches);
      }

      if (node.bm_faces) {
        BLI_gset_free(node.bm_faces, nullptr);
      }
      if (node.bm_unique_verts) {
        BLI_gset_free(node.bm_unique_verts, nullptr);
      }
      if (node.bm_other_verts) {
        BLI_gset_free(node.bm_other_verts, nullptr);
      }
    }

    if (node.flag & (PBVH_Leaf | PBVH_TexLeaf)) {
      pbvh_node_pixels_free(&node);
    }
  }

  pbvh_pixels_free(pbvh);

  MEM_delete(pbvh);
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

  iter->stack[0].node = &pbvh->nodes.first();
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
      iter->stack = static_cast<PBVHStack *>(
          MEM_reallocN(iter->stack, sizeof(PBVHStack) * iter->stackspace));
    }
    else {
      iter->stack = static_cast<PBVHStack *>(
          MEM_mallocN(sizeof(PBVHStack) * iter->stackspace, "PBVHStack"));
      memcpy(iter->stack, iter->stackfixed, sizeof(PBVHStack) * iter->stacksize);
    }
  }

  iter->stack[iter->stacksize].node = node;
  iter->stack[iter->stacksize].revisiting = revisiting;
  iter->stacksize++;
}

static PBVHNode *pbvh_iter_next(PBVHIter *iter, PBVHNodeFlags leaf_flag)
{
  /* purpose here is to traverse tree, visiting child nodes before their
   * parents, this order is necessary for e.g. computing bounding boxes */

  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    PBVHNode *node = iter->stack[iter->stacksize].node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == nullptr) {
      return nullptr;
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
    pbvh_stack_push(iter, &iter->pbvh->nodes[node->children_offset + 1], false);
    pbvh_stack_push(iter, &iter->pbvh->nodes[node->children_offset], false);
  }

  return nullptr;
}

static PBVHNode *pbvh_iter_next_occluded(PBVHIter *iter)
{
  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    PBVHNode *node = iter->stack[iter->stacksize].node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == nullptr) {
      return nullptr;
    }

    if (iter->scb && !iter->scb(node, iter->search_data)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag & PBVH_Leaf) {
      /* immediately hit leaf node */
      return node;
    }

    pbvh_stack_push(iter, &iter->pbvh->nodes[node->children_offset + 1], false);
    pbvh_stack_push(iter, &iter->pbvh->nodes[node->children_offset], false);
  }

  return nullptr;
}

struct node_tree {
  PBVHNode *data;

  node_tree *left;
  node_tree *right;
};

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
    tree->left = nullptr;
  }

  if (tree->right) {
    free_tree(tree->right);
    tree->right = nullptr;
  }

  free(tree);
}

float BKE_pbvh_node_get_tmin(PBVHNode *node)
{
  return node->tmin;
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

  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
    if (node->flag & PBVH_Leaf) {
      hcb(node, hit_data);
    }
  }

  pbvh_iter_end(&iter);
}

static void BKE_pbvh_search_callback_occluded(PBVH *pbvh,
                                              BKE_pbvh_SearchCallback scb,
                                              void *search_data,
                                              BKE_pbvh_HitOccludedCallback hcb,
                                              void *hit_data)
{
  PBVHIter iter;
  PBVHNode *node;
  node_tree *tree = nullptr;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

  while ((node = pbvh_iter_next_occluded(&iter))) {
    if (node->flag & PBVH_Leaf) {
      node_tree *new_node = static_cast<node_tree *>(malloc(sizeof(node_tree)));

      new_node->data = node;

      new_node->left = nullptr;
      new_node->right = nullptr;

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

static void pbvh_faces_update_normals(PBVH *pbvh, Span<PBVHNode *> nodes)
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = pbvh->vert_positions;
  const OffsetIndices faces = pbvh->faces;
  const Span<int> corner_verts = pbvh->corner_verts;

  MutableSpan<bool> update_tags = pbvh->vert_bitmap;

  VectorSet<int> faces_to_update;
  for (const PBVHNode *node : nodes) {
    for (const int vert : node->vert_indices.as_span().take_front(node->uniq_verts)) {
      if (update_tags[vert]) {
        faces_to_update.add_multiple(pbvh->pmap[vert]);
      }
    }
  }

  if (faces_to_update.is_empty()) {
    return;
  }

  MutableSpan<float3> vert_normals = pbvh->vert_normals;
  MutableSpan<float3> face_normals = pbvh->face_normals;

  VectorSet<int> verts_to_update;
  threading::parallel_invoke(
      [&]() {
        threading::parallel_for(faces_to_update.index_range(), 512, [&](const IndexRange range) {
          for (const int i : faces_to_update.as_span().slice(range)) {
            face_normals[i] = mesh::face_normal_calc(positions, corner_verts.slice(faces[i]));
          }
        });
      },
      [&]() {
        /* Update all normals connected to affected faces, even if not explicitly tagged. */
        verts_to_update.reserve(faces_to_update.size());
        for (const int face : faces_to_update) {
          verts_to_update.add_multiple(corner_verts.slice(faces[face]));
        }

        for (const int vert : verts_to_update) {
          update_tags[vert] = false;
        }
        for (PBVHNode *node : nodes) {
          node->flag &= ~PBVH_UpdateNormals;
        }
      });

  threading::parallel_for(verts_to_update.index_range(), 1024, [&](const IndexRange range) {
    for (const int vert : verts_to_update.as_span().slice(range)) {
      float3 normal(0.0f);
      for (const int face : pbvh->pmap[vert]) {
        normal += face_normals[face];
      }
      vert_normals[vert] = math::normalize(normal);
    }
  });
}

static void node_update_mask_redraw(PBVH &pbvh, PBVHNode &node)
{
  if (!(node.flag & PBVH_UpdateMask)) {
    return;
  }
  node.flag &= ~PBVH_UpdateMask;

  bool has_unmasked = false;
  bool has_masked = true;
  if (node.flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin (&pbvh, &node, vd, PBVH_ITER_ALL) {
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
  BKE_pbvh_node_fully_masked_set(&node, !has_unmasked);
  BKE_pbvh_node_fully_unmasked_set(&node, has_masked);
}

static void node_update_visibility_redraw(PBVH &pbvh, PBVHNode &node)
{
  if (!(node.flag & PBVH_UpdateVisibility)) {
    return;
  }
  node.flag &= ~PBVH_UpdateVisibility;

  BKE_pbvh_node_fully_hidden_set(&node, true);
  if (node.flag & PBVH_Leaf) {
    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin (&pbvh, &node, vd, PBVH_ITER_ALL) {
      if (vd.visible) {
        BKE_pbvh_node_fully_hidden_set(&node, false);
        return;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void node_update_bounds(PBVH &pbvh, PBVHNode &node, const PBVHNodeFlags flag)
{
  if ((flag & PBVH_UpdateBB) && (node.flag & PBVH_UpdateBB)) {
    /* don't clear flag yet, leave it for flushing later */
    /* Note that bvh usage is read-only here, so no need to thread-protect it. */
    update_node_vb(&pbvh, &node);
  }

  if ((flag & PBVH_UpdateOriginalBB) && (node.flag & PBVH_UpdateOriginalBB)) {
    node.orig_vb = node.vb;
  }

  if ((flag & PBVH_UpdateRedraw) && (node.flag & PBVH_UpdateRedraw)) {
    node.flag &= ~PBVH_UpdateRedraw;
  }
}

static void pbvh_update_BB_redraw(PBVH *pbvh, Span<PBVHNode *> nodes, int flag)
{
  using namespace blender;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_bounds(*pbvh, *node, PBVHNodeFlags(flag));
    }
  });
}

bool BKE_pbvh_get_color_layer(Mesh *me, CustomDataLayer **r_layer, eAttrDomain *r_domain)
{
  *r_layer = BKE_id_attribute_search(
      &me->id, me->active_color_attribute, CD_MASK_COLOR_ALL, ATTR_DOMAIN_MASK_COLOR);
  *r_domain = *r_layer ? BKE_id_attribute_domain(&me->id, *r_layer) : ATTR_DOMAIN_POINT;
  return *r_layer != nullptr;
}

static void node_update_draw_buffers(PBVH &pbvh, PBVHNode &node)
{
  /* Create and update draw buffers. The functions called here must not
   * do any OpenGL calls. Flags are not cleared immediately, that happens
   * after GPU_pbvh_buffer_flush() which does the final OpenGL calls. */
  if (node.flag & PBVH_RebuildDrawBuffers) {
    PBVH_GPU_Args args;
    pbvh_draw_args_init(&pbvh, &args, &node);
    node.draw_batches = DRW_pbvh_node_create(args);
  }

  if (node.flag & PBVH_UpdateDrawBuffers) {
    node.debug_draw_gen++;

    if (node.draw_batches) {
      PBVH_GPU_Args args;
      pbvh_draw_args_init(&pbvh, &args, &node);
      DRW_pbvh_node_update(node.draw_batches, args);
    }
  }
}

void pbvh_free_draw_buffers(PBVH * /*pbvh*/, PBVHNode *node)
{
  if (node->draw_batches) {
    DRW_pbvh_node_free(node->draw_batches);
    node->draw_batches = nullptr;
  }
}

static void pbvh_update_draw_buffers(PBVH *pbvh, Span<PBVHNode *> nodes, int update_flag)
{
  using namespace blender;
  if (pbvh->header.type == PBVH_BMESH && !pbvh->header.bm) {
    /* BMesh hasn't been created yet */
    return;
  }

  if ((update_flag & PBVH_RebuildDrawBuffers) || ELEM(pbvh->header.type, PBVH_GRIDS, PBVH_BMESH)) {
    /* Free buffers uses OpenGL, so not in parallel. */
    for (PBVHNode *node : nodes) {
      if (node->flag & PBVH_RebuildDrawBuffers) {
        pbvh_free_draw_buffers(pbvh, node);
      }
      else if ((node->flag & PBVH_UpdateDrawBuffers) && node->draw_batches) {
        PBVH_GPU_Args args;

        pbvh_draw_args_init(pbvh, &args, node);
        DRW_pbvh_update_pre(node->draw_batches, args);
      }
    }
  }

  /* Parallel creation and update of draw buffers. */

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_draw_buffers(*pbvh, *node);
    }
  });

  /* Flush buffers uses OpenGL, so not in parallel. */
  for (PBVHNode *node : nodes) {
    if (node->flag & PBVH_UpdateDrawBuffers) {

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

  update |= pbvh_flush_bb(pbvh, &pbvh->nodes[node->children_offset], flag);
  update |= pbvh_flush_bb(pbvh, &pbvh->nodes[node->children_offset + 1], flag);

  if (update & PBVH_UpdateBB) {
    update_node_vb(pbvh, node);
  }
  if (update & PBVH_UpdateOriginalBB) {
    node->orig_vb = node->vb;
  }

  return update;
}

void BKE_pbvh_update_bounds(PBVH *pbvh, int flag)
{
  if (pbvh->nodes.is_empty()) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(flag));

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw)) {
    pbvh_update_BB_redraw(pbvh, nodes, flag);
  }

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB)) {
    pbvh_flush_bb(pbvh, &pbvh->nodes.first(), flag);
  }
}

void BKE_pbvh_update_vertex_data(PBVH *pbvh, int flag)
{
  using namespace blender;
  if (pbvh->nodes.is_empty()) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(flag));

  if (flag & (PBVH_UpdateMask)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (PBVHNode *node : nodes.as_span().slice(range)) {
        node_update_mask_redraw(*pbvh, *node);
      }
    });
  }

  if (flag & (PBVH_UpdateColor)) {
    for (PBVHNode *node : nodes) {
      node->flag |= PBVH_UpdateRedraw | PBVH_UpdateDrawBuffers | PBVH_UpdateColor;
    }
  }

  if (flag & (PBVH_UpdateVisibility)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (PBVHNode *node : nodes.as_span().slice(range)) {
        node_update_visibility_redraw(*pbvh, *node);
      }
    });
  }
}

static void pbvh_faces_node_visibility_update(PBVH *pbvh, PBVHNode *node)
{
  if (pbvh->hide_vert == nullptr) {
    BKE_pbvh_node_fully_hidden_set(node, false);
    return;
  }
  for (const int vert : node->vert_indices) {
    if (!(pbvh->hide_vert[vert])) {
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
  const int *grid_indices;
  int totgrid, i;

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, nullptr, &grids);
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
    BMVert *v = static_cast<BMVert *>(BLI_gsetIterator_getKey(&gs_iter));
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  GSET_ITER (gs_iter, other) {
    BMVert *v = static_cast<BMVert *>(BLI_gsetIterator_getKey(&gs_iter));
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  BKE_pbvh_node_fully_hidden_set(node, true);
}

static void node_update_visibility(PBVH &pbvh, PBVHNode &node)
{
  if (!(node.flag & PBVH_UpdateVisibility)) {
    return;
  }
  node.flag &= ~PBVH_UpdateVisibility;
  switch (BKE_pbvh_type(&pbvh)) {
    case PBVH_FACES:
      pbvh_faces_node_visibility_update(&pbvh, &node);
      break;
    case PBVH_GRIDS:
      pbvh_grids_node_visibility_update(&pbvh, &node);
      break;
    case PBVH_BMESH:
      pbvh_bmesh_node_visibility_update(&node);
      break;
  }
}

void BKE_pbvh_update_visibility(PBVH *pbvh)
{
  using namespace blender;
  if (pbvh->nodes.is_empty()) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateVisibility));

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.as_span().slice(range)) {
      node_update_visibility(*pbvh, *node);
    }
  });
}

void BKE_pbvh_redraw_BB(PBVH *pbvh, float bb_min[3], float bb_max[3])
{
  PBVHIter iter;
  PBVHNode *node;
  BB bb;

  BB_reset(&bb);

  pbvh_iter_begin(&iter, pbvh, nullptr, nullptr);

  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
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

  pbvh_iter_begin(&iter, pbvh, nullptr, nullptr);

  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
    if (node->flag & PBVH_UpdateNormals) {
      for (const int grid : node->prim_indices) {
        void *face = pbvh->gridfaces[grid];
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
    *r_gridfaces = nullptr;
    BLI_gset_free(face_set, nullptr);
    return;
  }

  void **faces = static_cast<void **>(MEM_mallocN(sizeof(*faces) * tot, __func__));

  GSetIterator gs_iter;
  int i;
  GSET_ITER_INDEX (gs_iter, face_set, i) {
    faces[i] = BLI_gsetIterator_getKey(&gs_iter);
  }

  BLI_gset_free(face_set, nullptr);

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
  if (pbvh->nodes.is_empty()) {
    zero_v3(min);
    zero_v3(max);
    return;
  }
  const BB *bb = &pbvh->nodes[0].vb;
  copy_v3_v3(min, bb->bmin);
  copy_v3_v3(max, bb->bmax);
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

CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh)
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

void BKE_pbvh_node_mark_update(PBVHNode *node)
{
  node->flag |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateOriginalBB |
                PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw | PBVH_RebuildPixels;
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
  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_Leaf) {
      node.flag |= PBVH_RebuildPixels;
    }
  }
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
                             const int **r_corner_verts)
{
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  if (r_loop_indices) {
    *r_loop_indices = node->loop_indices.data();
  }

  if (r_corner_verts) {
    *r_corner_verts = pbvh->corner_verts.data();
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
  return node->vert_indices.data();
}

void BKE_pbvh_node_num_verts(PBVH *pbvh, PBVHNode *node, int *r_uniquevert, int *r_totvert)
{
  int tot;

  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      tot = node->prim_indices.size() * pbvh->gridkey.grid_area;
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
                             const int **r_grid_indices,
                             int *r_totgrid,
                             int *r_maxgrid,
                             int *r_gridsize,
                             CCGElem ***r_griddata)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      if (r_grid_indices) {
        *r_grid_indices = node->prim_indices.data();
      }
      if (r_totgrid) {
        *r_totgrid = node->prim_indices.size();
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
        *r_grid_indices = nullptr;
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
        *r_griddata = nullptr;
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
      *proxies = nullptr;
    }
    if (proxy_count) {
      *proxy_count = 0;
    }
  }
}

void BKE_pbvh_node_get_bm_orco_data(PBVHNode *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3],
                                    BMVert ***r_orco_verts)
{
  *r_orco_tris = node->bm_ortri;
  *r_orco_tris_num = node->bm_tot_ortri;
  *r_orco_coords = node->bm_orco;

  if (r_orco_verts) {
    *r_orco_verts = node->bm_orvert;
  }
}

bool BKE_pbvh_node_has_vert_with_normal_update_tag(PBVH *pbvh, PBVHNode *node)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  for (const int vert : node->vert_indices) {
    if (pbvh->vert_bitmap[vert]) {
      return true;
    }
  }
  return false;
}

/********************************* Ray-cast ***********************************/

struct RaycastData {
  IsectRayAABB_Precalc ray;
  bool original;
};

static bool ray_aabb_intersect(PBVHNode *node, void *data_v)
{
  RaycastData *rcd = static_cast<RaycastData *>(data_v);
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
                                IsectRayPrecalc *isect_precalc,
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth)
{
  float depth_test;

  if ((isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, nullptr) &&
       (depth_test < *depth)) ||
      (isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t2, t3, &depth_test, nullptr) &&
       (depth_test < *depth)))
  {
    *depth = depth_test;
    return true;
  }

  return false;
}

bool ray_face_intersection_tri(const float ray_start[3],
                               IsectRayPrecalc *isect_precalc,
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth)
{
  float depth_test;
  if (isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, nullptr) &&
      (depth_test < *depth))
  {
    *depth = depth_test;
    return true;
  }

  return false;
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
                                    IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    PBVHVertRef *r_active_vertex,
                                    int *r_active_face_index,
                                    float *r_face_normal)
{
  const Span<float3> positions = pbvh->vert_positions;
  const Span<int> corner_verts = pbvh->corner_verts;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  for (const int i : node->prim_indices.index_range()) {
    const int looptri_i = node->prim_indices[i];
    const MLoopTri *lt = &pbvh->looptri[looptri_i];
    const blender::int3 face_verts = node->face_vert_indices[i];

    if (paint_is_face_hidden(pbvh->looptri_faces.data(), pbvh->hide_poly, looptri_i)) {
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
      co[0] = positions[corner_verts[lt->tri[0]]];
      co[1] = positions[corner_verts[lt->tri[1]]];
      co[2] = positions[corner_verts[lt->tri[2]]];
    }

    if (ray_face_intersection_tri(ray_start, isect_precalc, co[0], co[1], co[2], depth)) {
      hit = true;

      if (r_face_normal) {
        normal_tri_v3(r_face_normal, co[0], co[1], co[2]);
      }

      if (r_active_vertex) {
        float location[3] = {0.0f};
        madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);
        for (int j = 0; j < 3; j++) {
          /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
           * uninitialized values. This stores the closest vertex in the current intersecting
           * triangle. */
          if (j == 0 ||
              len_squared_v3v3(location, co[j]) < len_squared_v3v3(location, nearest_vertex_co)) {
            copy_v3_v3(nearest_vertex_co, co[j]);
            r_active_vertex->i = corner_verts[lt->tri[j]];
            *r_active_face_index = pbvh->looptri_faces[looptri_i];
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
                                    IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    PBVHVertRef *r_active_vertex,
                                    int *r_active_grid_index,
                                    float *r_face_normal)
{
  const int totgrid = node->prim_indices.size();
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

        if (ray_face_intersection_quad(
                ray_start, isect_precalc, co[0], co[1], co[2], co[3], depth)) {
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
                           IsectRayPrecalc *isect_precalc,
                           float *depth,
                           PBVHVertRef *active_vertex,
                           int *active_face_grid_index,
                           float *face_normal)
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
                                     depth,
                                     active_vertex,
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
                                     active_vertex,
                                     active_face_grid_index,
                                     face_normal);
      break;
    case PBVH_BMESH:
      BM_mesh_elem_index_ensure(pbvh->header.bm, BM_VERT);
      hit = pbvh_bmesh_node_raycast(node,
                                    ray_start,
                                    ray_normal,
                                    isect_precalc,
                                    depth,
                                    use_origco,
                                    active_vertex,
                                    face_normal);
      break;
  }

  return hit;
}

void BKE_pbvh_clip_ray_ortho(
    PBVH *pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (pbvh->nodes.is_empty()) {
    return;
  }
  float rootmin_start, rootmin_end;
  float bb_min_root[3], bb_max_root[3], bb_center[3], bb_diff[3];
  IsectRayAABB_Precalc ray;
  float ray_normal_inv[3];
  float offset = 1.0f + 1e-3f;
  const float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

  if (original) {
    BKE_pbvh_node_get_original_BB(&pbvh->nodes.first(), bb_min_root, bb_max_root);
  }
  else {
    BKE_pbvh_node_get_BB(&pbvh->nodes.first(), bb_min_root, bb_max_root);
  }

  /* Calc rough clipping to avoid overflow later. See #109555. */
  float mat[3][3];
  axis_dominant_v3_to_m3(mat, ray_normal);
  float a[3], b[3], min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {FLT_MIN, FLT_MIN, FLT_MIN};

  /* Compute AABB bounds rotated along ray_normal.*/
  copy_v3_v3(a, bb_min_root);
  copy_v3_v3(b, bb_max_root);
  mul_m3_v3(mat, a);
  mul_m3_v3(mat, b);
  minmax_v3v3_v3(min, max, a);
  minmax_v3v3_v3(min, max, b);

  float cent[3];

  /* Find midpoint of aabb on ray. */
  mid_v3_v3v3(cent, bb_min_root, bb_max_root);
  float t = line_point_factor_v3(cent, ray_start, ray_end);
  interp_v3_v3v3(cent, ray_start, ray_end, t);

  /* Compute rough interval. */
  float dist = max[2] - min[2];
  madd_v3_v3v3fl(ray_start, cent, ray_normal, -dist);
  madd_v3_v3v3fl(ray_end, cent, ray_normal, dist);

  /* Slightly offset min and max in case we have a zero width node
   * (due to a plane mesh for instance), or faces very close to the bounding box boundary. */
  mid_v3_v3v3(bb_center, bb_max_root, bb_min_root);
  /* Diff should be same for both min/max since it's calculated from center. */
  sub_v3_v3v3(bb_diff, bb_max_root, bb_center);
  /* Handles case of zero width bb. */
  add_v3_v3(bb_diff, offset_vec);
  madd_v3_v3v3fl(bb_max_root, bb_center, bb_diff, offset);
  madd_v3_v3v3fl(bb_min_root, bb_center, bb_diff, -offset);

  /* Final projection of start ray. */
  isect_ray_aabb_v3_precalc(&ray, ray_start, ray_normal);
  if (!isect_ray_aabb_v3(&ray, bb_min_root, bb_max_root, &rootmin_start)) {
    return;
  }

  /* Final projection of end ray. */
  mul_v3_v3fl(ray_normal_inv, ray_normal, -1.0);
  isect_ray_aabb_v3_precalc(&ray, ray_end, ray_normal_inv);
  /* Unlikely to fail exiting if entering succeeded, still keep this here. */
  if (!isect_ray_aabb_v3(&ray, bb_min_root, bb_max_root, &rootmin_end)) {
    return;
  }

  /*
   * As a last-ditch effort to correct floating point overflow compute
   * and add an epsilon if rootmin_start == rootmin_end.
   */

  float epsilon = (std::nextafter(rootmin_start, rootmin_start + 1000.0f) - rootmin_start) *
                  5000.0f;

  if (rootmin_start == rootmin_end) {
    rootmin_start -= epsilon;
    rootmin_end += epsilon;
  }

  madd_v3_v3v3fl(ray_start, ray_start, ray_normal, rootmin_start);
  madd_v3_v3v3fl(ray_end, ray_end, ray_normal_inv, rootmin_end);
}

/* -------------------------------------------------------------------- */

struct FindNearestRayData {
  DistRayAABB_Precalc dist_ray_to_aabb_precalc;
  bool original;
};

static bool nearest_to_ray_aabb_dist_sq(PBVHNode *node, void *data_v)
{
  FindNearestRayData *rcd = static_cast<FindNearestRayData *>(data_v);
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
  const Span<float3> positions = pbvh->vert_positions;
  const Span<int> corner_verts = pbvh->corner_verts;
  bool hit = false;

  for (const int i : node->prim_indices.index_range()) {
    const int looptri_i = node->prim_indices[i];
    const MLoopTri *lt = &pbvh->looptri[looptri_i];
    const blender::int3 face_verts = node->face_vert_indices[i];

    if (paint_is_face_hidden(pbvh->looptri_faces.data(), pbvh->hide_poly, looptri_i)) {
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
                                  positions[corner_verts[lt->tri[0]]],
                                  positions[corner_verts[lt->tri[1]]],
                                  positions[corner_verts[lt->tri[2]]],
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
  const int totgrid = node->prim_indices.size();
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
          node, ray_start, ray_normal, depth, dist_sq, use_origco);
      break;
  }

  return hit;
}

enum PlaneAABBIsect {
  ISECT_INSIDE,
  ISECT_OUTSIDE,
  ISECT_INTERSECT,
};

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

  return test_frustum_aabb(bb_min, bb_max, static_cast<PBVHFrustumPlanes *>(data)) !=
         ISECT_OUTSIDE;
}

bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, void *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.bmin;
  bb_max = node->vb.bmax;

  return test_frustum_aabb(bb_min, bb_max, static_cast<PBVHFrustumPlanes *>(data)) != ISECT_INSIDE;
}

void BKE_pbvh_update_normals(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  /* Update normals */
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      pbvh, update_search_cb, POINTER_FROM_INT(PBVH_UpdateNormals));

  if (!nodes.is_empty()) {
    if (pbvh->header.type == PBVH_BMESH) {
      pbvh_bmesh_normals_update(nodes);
    }
    else if (pbvh->header.type == PBVH_FACES) {
      pbvh_faces_update_normals(pbvh, nodes);
    }
    else if (pbvh->header.type == PBVH_GRIDS) {
      CCGFace **faces;
      int num_faces;
      BKE_pbvh_get_grid_updates(pbvh, true, (void ***)&faces, &num_faces);
      if (num_faces > 0) {
        BKE_subdiv_ccg_update_normals(subdiv_ccg, faces, num_faces);
        MEM_freeN(faces);
      }
    }
  }
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
struct PBVHDrawSearchData {
  PBVHFrustumPlanes *frustum;
  int accum_update_flag;
  PBVHAttrReq *attrs;
  int attrs_num;
};

static bool pbvh_draw_search_cb(PBVHNode *node, void *data_v)
{
  PBVHDrawSearchData *data = static_cast<PBVHDrawSearchData *>(data_v);
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
                      void (*draw_fn)(void *user_data,
                                      PBVHBatches *batches,
                                      const PBVH_GPU_Args &args),
                      void *user_data,
                      bool /*full_render*/,
                      PBVHAttrReq *attrs,
                      int attrs_num)
{
  Vector<PBVHNode *> nodes;
  int update_flag = 0;

  pbvh->draw_cache_invalid = false;

  /* Search for nodes that need updates. */
  if (update_only_visible) {
    /* Get visible nodes with draw updates. */
    PBVHDrawSearchData data{};
    data.frustum = update_frustum;
    data.accum_update_flag = 0;
    data.attrs = attrs;
    data.attrs_num = attrs_num;
    nodes = blender::bke::pbvh::search_gather(pbvh, pbvh_draw_search_cb, &data);
    update_flag = data.accum_update_flag;
  }
  else {
    /* Get all nodes with draw updates, also those outside the view. */
    const int search_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
    nodes = blender::bke::pbvh::search_gather(
        pbvh, update_search_cb, POINTER_FROM_INT(search_flag));
    update_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
  }

  /* Update draw buffers. */
  if (!nodes.is_empty() && (update_flag & (PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers))) {
    pbvh_update_draw_buffers(pbvh, nodes, update_flag);
  }

  /* Draw visible nodes. */
  PBVHDrawSearchData draw_data{};
  draw_data.frustum = draw_frustum;
  draw_data.accum_update_flag = 0;
  nodes = blender::bke::pbvh::search_gather(pbvh, pbvh_draw_search_cb, &draw_data);

  PBVH_GPU_Args args;

  for (PBVHNode *node : nodes) {
    if (!(node->flag & PBVH_FullyHidden)) {
      pbvh_draw_args_init(pbvh, &args, node);
      draw_fn(user_data, node->draw_batches, args);
    }
  }
}

void BKE_pbvh_draw_debug_cb(PBVH *pbvh,
                            void (*draw_fn)(PBVHNode *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data)
{
  PBVHNodeFlags flag = PBVH_Leaf;

  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_TexLeaf) {
      flag = PBVH_TexLeaf;
      break;
    }
  }

  for (PBVHNode &node : pbvh->nodes) {
    if (!(node.flag & flag)) {
      continue;
    }

    draw_fn(&node, user_data, node.vb.bmin, node.vb.bmax, node.flag);
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

    for (PBVHNode &node : pbvh->nodes) {
      BKE_pbvh_node_mark_rebuild_draw(&node);
    }
  }
}

float (*BKE_pbvh_vert_coords_alloc(PBVH *pbvh))[3]
{
  float(*vertCos)[3] = nullptr;

  if (!pbvh->vert_positions.is_empty()) {
    vertCos = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(pbvh->totvert, sizeof(float[3]), __func__));
    memcpy(vertCos, pbvh->vert_positions.data(), sizeof(float[3]) * pbvh->totvert);
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
    if (!pbvh->vert_positions.is_empty()) {
      /* if pbvh is not already deformed, verts/faces points to the */
      /* original data and applying new coords to this arrays would lead to */
      /* unneeded deformation -- duplicate verts/faces to avoid this */
      pbvh->vert_positions_deformed = blender::Array<float3>(pbvh->vert_positions.as_span());
      pbvh->vert_positions = pbvh->vert_positions_deformed;

      /* No need to dupalloc pbvh->looptri, this one is 'totally owned' by pbvh,
       * it's never some mesh data. */

      pbvh->deformed = true;
    }
  }

  if (!pbvh->vert_positions.is_empty()) {
    MutableSpan<float3> positions = pbvh->vert_positions;
    /* copy new verts coords */
    for (int a = 0; a < pbvh->totvert; a++) {
      /* no need for float comparison here (memory is exactly equal or not) */
      if (memcmp(positions[a], vertCos[a], sizeof(float[3])) != 0) {
        copy_v3_v3(positions[a], vertCos[a]);
        BKE_pbvh_vert_tag_update_normal(pbvh, BKE_pbvh_make_vref(a));
      }
    }

    for (PBVHNode &node : pbvh->nodes) {
      BKE_pbvh_node_mark_update(&node);
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
    node->proxies = static_cast<PBVHProxyNode *>(
        MEM_reallocN(node->proxies, node->proxy_count * sizeof(PBVHProxyNode)));
  }
  else {
    node->proxies = static_cast<PBVHProxyNode *>(MEM_mallocN(sizeof(PBVHProxyNode), __func__));
  }

  BKE_pbvh_node_num_verts(pbvh, node, &totverts, nullptr);
  node->proxies[index].co = static_cast<float(*)[3]>(
      MEM_callocN(sizeof(float[3]) * totverts, __func__));

  return node->proxies + index;
}

void BKE_pbvh_node_free_proxies(PBVHNode *node)
{
  for (int p = 0; p < node->proxy_count; p++) {
    MEM_freeN(node->proxies[p].co);
    node->proxies[p].co = nullptr;
  }

  MEM_freeN(node->proxies);
  node->proxies = nullptr;

  node->proxy_count = 0;
}

PBVHColorBufferNode *BKE_pbvh_node_color_buffer_get(PBVHNode *node)
{

  if (!node->color_buffer.color) {
    node->color_buffer.color = static_cast<float(*)[4]>(
        MEM_callocN(sizeof(float[4]) * node->uniq_verts, "Color buffer"));
  }
  return &node->color_buffer;
}

void BKE_pbvh_node_color_buffer_free(PBVH *pbvh)
{
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, nullptr, nullptr);

  for (PBVHNode *node : nodes) {
    MEM_SAFE_FREE(node->color_buffer.color);
  }
}

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode)
{
  CCGElem **grids;
  const int *grid_indices;
  int totgrid, gridsize, uniq_verts, totvert;

  vi->grid = nullptr;
  vi->no = nullptr;
  vi->fno = nullptr;
  vi->vert_positions = {};
  vi->vertex.i = 0LL;

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, &gridsize, &grids);
  BKE_pbvh_node_num_verts(pbvh, node, &uniq_verts, &totvert);
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
  vi->vert_indices = node->vert_indices.data();
  vi->vert_positions = pbvh->vert_positions;
  vi->is_mesh = !pbvh->vert_positions.is_empty();

  if (pbvh->header.type == PBVH_BMESH) {
    BLI_gsetIterator_init(&vi->bm_unique_verts, node->bm_unique_verts);
    BLI_gsetIterator_init(&vi->bm_other_verts, node->bm_other_verts);
    vi->bm_vdata = &pbvh->header.bm->vdata;
    vi->cd_vert_mask_offset = CustomData_get_offset(vi->bm_vdata, CD_PAINT_MASK);
  }

  vi->gh = nullptr;
  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = pbvh->grid_hidden;
  }

  vi->mask = nullptr;
  if (pbvh->header.type == PBVH_FACES) {
    vi->vert_normals = pbvh->vert_normals;
    vi->hide_vert = pbvh->hide_vert;

    vi->vmask = static_cast<float *>(
        CustomData_get_layer_for_write(pbvh->vert_data, CD_PAINT_MASK, pbvh->mesh->totvert));
  }
}

bool pbvh_has_mask(const PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      return (pbvh->gridkey.has_mask != 0);
    case PBVH_FACES:
      return (pbvh->vert_data && CustomData_get_layer(pbvh->vert_data, CD_PAINT_MASK));
    case PBVH_BMESH:
      return (pbvh->header.bm &&
              (CustomData_get_offset(&pbvh->header.bm->vdata, CD_PAINT_MASK) != -1));
  }

  return false;
}

bool pbvh_has_face_sets(PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
    case PBVH_FACES:
      return pbvh->face_data && CustomData_get_layer_named(
                                    pbvh->face_data, CD_PROP_INT32, ".sculpt_face_set") != nullptr;
    case PBVH_BMESH:
      return false;
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

void BKE_pbvh_parallel_range_settings(TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode)
{
  memset(settings, 0, sizeof(*settings));
  settings->use_threading = use_threading && totnode > 1;
}

float (*BKE_pbvh_get_vert_positions(const PBVH *pbvh))[3]
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return reinterpret_cast<float(*)[3]>(pbvh->vert_positions.data());
}

const float (*BKE_pbvh_get_vert_normals(const PBVH *pbvh))[3]
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return reinterpret_cast<const float(*)[3]>(pbvh->vert_normals.data());
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
  pbvh->hide_vert = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &pbvh->mesh->vert_data, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->totvert));
  if (pbvh->hide_vert) {
    return pbvh->hide_vert;
  }
  pbvh->hide_vert = static_cast<bool *>(CustomData_add_layer_named(
      &pbvh->mesh->vert_data, CD_PROP_BOOL, CD_SET_DEFAULT, pbvh->mesh->totvert, ".hide_vert"));
  return pbvh->hide_vert;
}

void BKE_pbvh_subdiv_cgg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  pbvh->subdiv_ccg = subdiv_ccg;
}

void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets)
{
  pbvh->face_sets = face_sets;
}

void BKE_pbvh_update_hide_attributes_from_mesh(PBVH *pbvh)
{
  if (pbvh->header.type == PBVH_FACES) {
    pbvh->hide_vert = static_cast<bool *>(CustomData_get_layer_named_for_write(
        &pbvh->mesh->vert_data, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->totvert));
    pbvh->hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
        &pbvh->mesh->face_data, CD_PROP_BOOL, ".hide_poly", pbvh->mesh->faces_num));
  }
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
    *r_totloop = node->loop_indices.size();
  }
}

void BKE_pbvh_update_active_vcol(PBVH *pbvh, Mesh *mesh)
{
  BKE_pbvh_get_color_layer(mesh, &pbvh->color_layer, &pbvh->color_domain);
}

void BKE_pbvh_pmap_set(PBVH *pbvh, const blender::GroupedSpan<int> pmap)
{
  pbvh->pmap = pmap;
}

void BKE_pbvh_ensure_node_loops(PBVH *pbvh)
{
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  int totloop = 0;

  /* Check if nodes already have loop indices. */
  for (PBVHNode &node : pbvh->nodes) {
    if (!(node.flag & PBVH_Leaf)) {
      continue;
    }

    if (!node.loop_indices.is_empty()) {
      return;
    }

    totloop += node.prim_indices.size() * 3;
  }

  BLI_bitmap *visit = BLI_BITMAP_NEW(totloop, __func__);

  /* Create loop indices from node loop triangles. */
  Vector<int> loop_indices;
  for (PBVHNode &node : pbvh->nodes) {
    if (!(node.flag & PBVH_Leaf)) {
      continue;
    }

    loop_indices.clear();

    for (const int i : node.prim_indices) {
      const MLoopTri &mlt = pbvh->looptri[i];

      for (int k = 0; k < 3; k++) {
        if (!BLI_BITMAP_TEST(visit, mlt.tri[k])) {
          loop_indices.append(mlt.tri[k]);
          BLI_BITMAP_ENABLE(visit, mlt.tri[k]);
        }
      }
    }

    node.loop_indices.reinitialize(loop_indices.size());
    node.loop_indices.as_mutable_span().copy_from(loop_indices);
  }

  MEM_SAFE_FREE(visit);
}

int BKE_pbvh_debug_draw_gen_get(PBVHNode *node)
{
  return node->debug_draw_gen;
}

static void pbvh_face_iter_verts_reserve(PBVHFaceIter *fd, int verts_num)
{
  if (verts_num >= fd->verts_size_) {
    fd->verts_size_ = (verts_num + 1) << 2;

    if (fd->verts != fd->verts_reserved_) {
      MEM_SAFE_FREE(fd->verts);
    }

    fd->verts = static_cast<PBVHVertRef *>(
        MEM_malloc_arrayN(fd->verts_size_, sizeof(void *), __func__));
  }

  fd->verts_num = verts_num;
}

BLI_INLINE int face_iter_prim_to_face(PBVHFaceIter *fd, int prim_index)
{
  if (fd->subdiv_ccg_) {
    return BKE_subdiv_ccg_grid_to_face_index(fd->subdiv_ccg_, prim_index);
  }

  return fd->looptri_faces_[prim_index];
}

static void pbvh_face_iter_step(PBVHFaceIter *fd, bool do_step)
{
  if (do_step) {
    fd->i++;
  }

  switch (fd->pbvh_type_) {
    case PBVH_BMESH: {
      if (do_step) {
        BLI_gsetIterator_step(&fd->bm_faces_iter_);
        if (BLI_gsetIterator_done(&fd->bm_faces_iter_)) {
          return;
        }
      }

      BMFace *f = (BMFace *)BLI_gsetIterator_getKey(&fd->bm_faces_iter_);
      fd->face.i = intptr_t(f);
      fd->index = f->head.index;

      if (fd->cd_face_set_ != -1) {
        fd->face_set = (int *)BM_ELEM_CD_GET_VOID_P(f, fd->cd_face_set_);
      }

      if (fd->cd_hide_poly_ != -1) {
        fd->hide = (bool *)BM_ELEM_CD_GET_VOID_P(f, fd->cd_hide_poly_);
      }

      pbvh_face_iter_verts_reserve(fd, f->len);
      int vertex_i = 0;

      BMLoop *l = f->l_first;
      do {
        fd->verts[vertex_i++].i = intptr_t(l->v);
      } while ((l = l->next) != f->l_first);

      break;
    }
    case PBVH_GRIDS:
    case PBVH_FACES: {
      int face_i = 0;

      if (do_step) {
        fd->prim_index_++;

        while (fd->prim_index_ < fd->node_->prim_indices.size()) {
          face_i = face_iter_prim_to_face(fd, fd->node_->prim_indices[fd->prim_index_]);

          if (face_i != fd->last_face_index_) {
            break;
          }

          fd->prim_index_++;
        }
      }
      else if (fd->prim_index_ < fd->node_->prim_indices.size()) {
        face_i = face_iter_prim_to_face(fd, fd->node_->prim_indices[fd->prim_index_]);
      }

      if (fd->prim_index_ >= fd->node_->prim_indices.size()) {
        return;
      }

      fd->last_face_index_ = face_i;
      const int poly_start = fd->face_offsets_[face_i].start();
      const int poly_size = fd->face_offsets_[face_i].size();

      fd->face.i = fd->index = face_i;

      if (fd->face_sets_) {
        fd->face_set = fd->face_sets_ + face_i;
      }
      if (fd->hide_poly_) {
        fd->hide = fd->hide_poly_ + face_i;
      }

      pbvh_face_iter_verts_reserve(fd, poly_size);

      const int *face_verts = &fd->corner_verts_[poly_start];
      const int grid_area = fd->subdiv_key_.grid_area;

      for (int i = 0; i < poly_size; i++) {
        if (fd->pbvh_type_ == PBVH_GRIDS) {
          /* Grid corners. */
          fd->verts[i].i = (poly_start + i) * grid_area + grid_area - 1;
        }
        else {
          fd->verts[i].i = face_verts[i];
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
  *fd = {};

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
      fd->face_offsets_ = pbvh->faces;
      fd->corner_verts_ = pbvh->corner_verts;
      fd->looptri_faces_ = pbvh->looptri_faces;
      fd->hide_poly_ = pbvh->hide_poly;
      fd->face_sets_ = pbvh->face_sets;
      fd->last_face_index_ = -1;

      break;
    case PBVH_BMESH:
      fd->bm = pbvh->header.bm;
      fd->cd_face_set_ = CustomData_get_offset_named(
          &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
      fd->cd_hide_poly_ = CustomData_get_offset_named(
          &pbvh->header.bm->pdata, CD_PROP_INT32, ".hide_poly");

      BLI_gsetIterator_init(&fd->bm_faces_iter_, node->bm_faces);
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
      return fd->prim_index_ >= fd->node_->prim_indices.size();
    case PBVH_BMESH:
      return BLI_gsetIterator_done(&fd->bm_faces_iter_);
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
      const blender::OffsetIndices faces = mesh->faces();
      CCGKey key = pbvh->gridkey;

      bool *hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
          &mesh->face_data, CD_PROP_BOOL, ".hide_poly", mesh->faces_num));

      bool delete_hide_poly = true;
      for (const int face_i : faces.index_range()) {
        const blender::IndexRange face = faces[face_i];
        bool hidden = false;

        for (int loop_index = 0; !hidden && loop_index < face.size(); loop_index++) {
          int grid_index = face[loop_index];

          if (pbvh->grid_hidden[grid_index] &&
              BLI_BITMAP_TEST(pbvh->grid_hidden[grid_index], key.grid_area - 1))
          {
            hidden = true;

            break;
          }
        }

        if (hidden && !hide_poly) {
          hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
              &mesh->face_data, CD_PROP_BOOL, ".hide_poly", mesh->faces_num));

          if (!hide_poly) {
            hide_poly = static_cast<bool *>(CustomData_add_layer_named(
                &mesh->face_data, CD_PROP_BOOL, CD_CONSTRUCT, mesh->faces_num, ".hide_poly"));
          }
        }

        if (hide_poly) {
          delete_hide_poly = delete_hide_poly && !hidden;
          hide_poly[face_i] = hidden;
        }
      }

      if (delete_hide_poly) {
        CustomData_free_layer_named(&mesh->face_data, ".hide_poly", mesh->faces_num);
      }

      BKE_mesh_flush_hidden_from_faces(mesh);
      BKE_pbvh_update_hide_attributes_from_mesh(pbvh);
      break;
    }
  }
}

namespace blender::bke::pbvh {
Vector<PBVHNode *> search_gather(PBVH *pbvh,
                                 BKE_pbvh_SearchCallback scb,
                                 void *search_data,
                                 PBVHNodeFlags leaf_flag)
{
  PBVHIter iter;
  Vector<PBVHNode *> nodes;

  pbvh_iter_begin(&iter, pbvh, scb, search_data);

  PBVHNode *node;
  while ((node = pbvh_iter_next(&iter, leaf_flag))) {
    if (node->flag & leaf_flag) {
      nodes.append(node);
    }
  }

  pbvh_iter_end(&iter);
  return nodes;
}

Vector<PBVHNode *> gather_proxies(PBVH *pbvh)
{
  Vector<PBVHNode *> array;

  for (PBVHNode &node : pbvh->nodes) {
    if (node.proxy_count > 0) {
      array.append(&node);
    }
  }

  return array;
}
}  // namespace blender::bke::pbvh
