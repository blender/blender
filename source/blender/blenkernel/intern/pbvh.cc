/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bitmap.h"
#include "BLI_bounds.hh"
#include "BLI_bounds_types.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_timeit.hh"

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_time.h"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph_query.hh"

#include "DRW_pbvh.hh"

#include "bmesh.hh"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

#include <limits.h>
#include <utility>

using blender::bke::AttrDomain;
using blender::bke::dyntopo::DyntopoSet;
using namespace blender;
using namespace blender::bke;
using blender::BitGroupVector;

#define LEAF_LIMIT 10000

/** Create invalid bounds for use with #math::min_max. */
static Bounds<float3> negative_bounds()
{
  return {float3(std::numeric_limits<float>::max()), float3(std::numeric_limits<float>::lowest())};
}

/* Uncomment to test if triangles of the same face are
 * properly clustered into single nodes.
 */
// #define TEST_PBVH_FACE_SPLIT

/* Uncomment to test that faces are only assigned to one PBVHNode */
// #define VALIDATE_UNIQUE_NODE_FACES

// #define PERFCNTRS
#define STACK_FIXED_DEPTH 100

typedef struct PBVHStack {
  PBVHNode *node;
  bool revisiting;
} PBVHStack;

typedef struct PBVHIter {
  PBVH *pbvh;
  blender::FunctionRef<bool(PBVHNode &)> scb;

  PBVHStack *stack;
  int stacksize;

  PBVHStack stackfixed[PBVH_STACK_FIXED_DEPTH];
  int stackspace;
} PBVHIter;

void BB_zero(BB *bb)
{
  bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = 0.0f;
  bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = 0.0f;
}

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
static void update_node_vb(PBVH *pbvh, PBVHNode *node, int updateflag)
{
  auto not_leaf_or_has_faces = [&](PBVHNode *node) {
    if (!(node->flag & PBVH_Leaf)) {
      return true;
    }

    return bool(node->bm_faces ? node->bm_faces->size() : node->prim_indices.size());
  };

  if (!(updateflag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB))) {
    return;
  }

  /* cannot clear flag here, causes leaky pbvh */
  // node->flag &= ~(updateflag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB));

  Bounds<float3> vb = {float3(FLT_MAX, FLT_MAX, FLT_MAX), float3(-FLT_MAX, -FLT_MAX, -FLT_MAX)};
  Bounds<float3> orig_vb = {float3(FLT_MAX, FLT_MAX, FLT_MAX),
                            float3(-FLT_MAX, -FLT_MAX, -FLT_MAX)};

  bool do_orig = true;    // XXX updateflag & PBVH_UpdateOriginalBB;
  bool do_normal = true;  // XXX updateflag & PBVH_UpdateBB;

  if (node->flag & PBVH_Leaf) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_ALL) {
      if (do_normal) {
        vb.min = math::min(vb.min, float3(vd.co));
        vb.max = math::max(vb.max, float3(vd.co));
      }

      if (do_orig) {
        const float *origco = pbvh->header.type == PBVH_BMESH ?
                                  BM_ELEM_CD_PTR<const float *>(vd.bm_vert, pbvh->cd_origco) :
                                  reinterpret_cast<const float *>(&pbvh->origco[vd.index]);

        /* XXX check stroke id here and use v->co? */
        orig_vb.min = math::min(orig_vb.min, float3(origco));
        orig_vb.max = math::max(orig_vb.max, float3(origco));
      }
    }
    BKE_pbvh_vertex_iter_end;

    if (!not_leaf_or_has_faces(node)) {
      vb.min = float3();
      vb.max = float3();
      orig_vb.min = float3();
      orig_vb.max = float3();
    }
  }
  else {
    bool ok = false;

    if (not_leaf_or_has_faces(&pbvh->nodes[node->children_offset])) {
      if (do_normal) {
        vb = bounds::merge(vb, pbvh->nodes[node->children_offset].vb);
      }
      if (do_orig) {
        orig_vb = bounds::merge(orig_vb, pbvh->nodes[node->children_offset].orig_vb);
      }

      ok = true;
    }

    if (not_leaf_or_has_faces(&pbvh->nodes[node->children_offset + 1])) {
      if (do_normal) {
        vb = bounds::merge(vb, pbvh->nodes[node->children_offset + 1].vb);
      }
      if (do_orig) {
        orig_vb = bounds::merge(orig_vb, pbvh->nodes[node->children_offset + 1].orig_vb);
      }

      ok = true;
    }

    if (!ok) {
      vb.min = float3();
      vb.max = float3();
      orig_vb.min = float3();
      orig_vb.max = float3();
    }
  }

  if (do_normal) {
    node->vb = vb;
  }

  if (do_orig) {
    node->orig_vb = orig_vb;
  }
}

static bool face_materials_match(const Span<int> material_indices,
                                 const Span<bool> sharp_faces,
                                 const int a,
                                 const int b)
{
  if (!material_indices.is_empty()) {
    if (material_indices[a] != material_indices[b]) {
      return false;
    }
  }
  if (!sharp_faces.is_empty()) {
    if (sharp_faces[a] != sharp_faces[b]) {
      return false;
    }
  }
  return true;
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_prim_indices(MutableSpan<int> prim_indices,
                                  int *prim_scratch,
                                  int lo,
                                  int hi,
                                  int axis,
                                  float mid,
                                  const Span<Bounds<float3>> prim_bounds,
                                  const Span<int> prim_to_face_map)
{
  for (int i = lo; i < hi; i++) {
    prim_scratch[i - lo] = prim_indices[i];
  }

  int lo2 = lo, hi2 = hi - 1;
  int i1 = lo, i2 = 0;

  while (i1 < hi) {
    const int face_i = prim_to_face_map[prim_scratch[i2]];
    const Bounds<float3> &bounds = prim_bounds[prim_scratch[i2]];
    const bool side = math::midpoint(bounds.min[axis], bounds.max[axis]) >= mid;

    while (i1 < hi && prim_to_face_map[prim_scratch[i2]] == face_i) {
      prim_indices[side ? hi2-- : lo2++] = prim_scratch[i2];
      i1++;
      i2++;
    }
  }

  return lo2;
}

/* Returns the index of the first element on the right of the partition */
static int partition_indices_material_faces(MutableSpan<int> indices,
                                            const Span<int> prim_to_face_map,
                                            const Span<int> material_indices,
                                            const Span<bool> sharp_faces,
                                            const int lo,
                                            const int hi)
{
  int i = lo, j = hi;
  for (;;) {
    const int first = prim_to_face_map[indices[lo]];
    for (;
         face_materials_match(material_indices, sharp_faces, first, prim_to_face_map[indices[i]]);
         i++)
    {
      /* pass */
    }
    for (;
         !face_materials_match(material_indices, sharp_faces, first, prim_to_face_map[indices[j]]);
         j--)
    {
      /* pass */
    }
    if (!(i < j)) {
      return i;
    }
    std::swap(indices[i], indices[j]);
    i++;
  }
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(Map<int, int> &map,
                           MutableSpan<bool> vert_bitmap,
                           int *face_verts,
                           int *uniq_verts,
                           int vertex)
{
  return map.lookup_or_add_cb(vertex, [&]() {
    int value;
    if (!vert_bitmap[vertex]) {
      vert_bitmap[vertex] = true;
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
static void build_mesh_leaf_node(const Span<int> corner_verts,
                                 const Span<int3> corner_tris,
                                 const Span<int> tri_faces,
                                 const Span<bool> hide_poly,
                                 MutableSpan<bool> vert_bitmap,
                                 PBVHNode *node)
{
  node->uniq_verts = node->face_verts = 0;
  const Span<int> prim_indices = node->prim_indices;

  /* reserve size is rough guess */
  Map<int, int> map;
  map.reserve(prim_indices.size());

  node->face_vert_indices.reinitialize(prim_indices.size());

  for (const int i : prim_indices.index_range()) {
    const int3 &tri = corner_tris[prim_indices[i]];
    for (int j = 0; j < 3; j++) {
      node->face_vert_indices[i][j] = map_insert_vert(
          map, vert_bitmap, &node->face_verts, &node->uniq_verts, corner_verts[tri[j]]);
    }
  }

  node->vert_indices.reinitialize(node->uniq_verts + node->face_verts);

  /* Build the vertex list, unique verts first */
  for (const MapItem<int, int> item : map.items()) {
    int value = item.value;
    if (value < 0) {
      value = -value + node->uniq_verts - 1;
    }

    node->vert_indices[value] = item.key;
  }

  for (const int i : prim_indices.index_range()) {
    for (int j = 0; j < 3; j++) {
      if (node->face_vert_indices[i][j] < 0) {
        node->face_vert_indices[i][j] = -node->face_vert_indices[i][j] + node->uniq_verts - 1;
      }
    }
  }

  const bool fully_hidden = !hide_poly.is_empty() &&
                            std::all_of(prim_indices.begin(),
                                        prim_indices.end(),
                                        [&](const int tri) { return hide_poly[tri_faces[tri]]; });
  BKE_pbvh_node_fully_hidden_set(node, fully_hidden);
  BKE_pbvh_node_mark_rebuild_draw(node);
}

static void update_vb(const Span<int> prim_indices,
                      PBVHNode *node,
                      const Span<Bounds<float3>> prim_bounds,
                      int offset,
                      int count)
{
  node->vb = prim_bounds[prim_indices[offset]];
  for (const int i : IndexRange(offset, count).drop_front(1)) {
    node->vb = bounds::merge(node->vb, prim_bounds[prim_indices[i]]);
  }
  node->orig_vb = node->vb;
}

namespace blender::bke::pbvh {
int count_grid_quads(const BitGroupVector<> &grid_hidden,
                     const Span<int> grid_indices,
                     int gridsize,
                     int display_gridsize)
{
  const int gridarea = (gridsize - 1) * (gridsize - 1);
  if (grid_hidden.is_empty()) {
    return gridarea * grid_indices.size();
  }

  /* grid hidden layer is present, so have to check each grid for
   * visibility */

  int depth1 = int(log2(double(gridsize) - 1.0) + DBL_EPSILON);
  int depth2 = int(log2(double(display_gridsize) - 1.0) + DBL_EPSILON);

  int skip = depth2 < depth1 ? 1 << (depth1 - depth2 - 1) : 1;

  int totquad = 0;
  for (const int grid : grid_indices) {
    const blender::BoundedBitSpan gh = grid_hidden[grid];
    /* grid hidden are present, have to check each element */
    for (int y = 0; y < gridsize - skip; y += skip) {
      for (int x = 0; x < gridsize - skip; x += skip) {
        if (!paint_is_grid_face_hidden(gh, gridsize, x, y)) {
          totquad++;
        }
      }
    }
  }

  return totquad;
}
}  // namespace blender::bke::pbvh

static void build_grid_leaf_node(PBVH *pbvh, PBVHNode *node)
{
  int totquads = bke::pbvh::count_grid_quads(pbvh->subdiv_ccg->grid_hidden,
                                             node->prim_indices,
                                             pbvh->gridkey.grid_size,
                                             pbvh->gridkey.grid_size);
  BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
  BKE_pbvh_node_mark_rebuild_draw(node);
}

static void build_leaf(PBVH *pbvh,
                       const Span<int> corner_verts,
                       const Span<int3> corner_tris,
                       const Span<int> tri_faces,
                       const Span<bool> hide_poly,
                       MutableSpan<bool> vert_bitmap,
                       int node_index,
                       const Span<Bounds<float3>> prim_bounds,
                       int offset,
                       int count)
{
  PBVHNode &node = pbvh->nodes[node_index];
  node.flag |= PBVH_Leaf;

  node.prim_indices = pbvh->prim_indices.as_span().slice(offset, count);

  /* Still need vb for searches */
  update_vb(pbvh->prim_indices, &node, prim_bounds, offset, count);

  if (!pbvh->corner_tris.is_empty()) {
    build_mesh_leaf_node(corner_verts, corner_tris, tri_faces, hide_poly, vert_bitmap, &node);
  }
  else {
    build_grid_leaf_node(pbvh, &node);
  }
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static bool leaf_needs_material_split(PBVH *pbvh,
                                      const Span<int> prim_to_face_map,
                                      const Span<int> material_indices,
                                      const Span<bool> sharp_faces,
                                      int offset,
                                      int count)
{
  if (count <= 1) {
    return false;
  }

  const int first = prim_to_face_map[pbvh->prim_indices[offset]];
  for (int i = offset + count - 1; i > offset; i--) {
    int prim = pbvh->prim_indices[i];
    if (!face_materials_match(material_indices, sharp_faces, first, prim_to_face_map[prim])) {
      return true;
    }
  }

  return false;
}

#ifdef TEST_PBVH_FACE_SPLIT
static void test_face_boundaries(PBVH *pbvh, const Mesh &mesh)
{
  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    int faces_num = mesh.faces_num;
    Array<int> node_map(faces_num, -1);
    for (int i = 0; i < pbvh->totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;
      if (!(node->flag & PBVH_Leaf)) {
        continue;
      }

      for (int j = 0; j < node->totprim; j++) {
        int face_i = mesh.corner_tri_faces()[node->prim_indices[j]];

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
    }
  }
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
                      const Span<int> corner_verts,
                      const Span<int3> corner_tris,
                      const Span<int> tri_faces,
                      const Span<bool> hide_poly,
                      const Span<int> material_indices,
                      const Span<bool> sharp_faces,
                      MutableSpan<bool> vert_bitmap,
                      int node_index,
                      const Bounds<float3> *cb,
                      const Span<Bounds<float3>> prim_bounds,
                      int offset,
                      int count,
                      int *prim_scratch,
                      int depth)
{
  const Span<int> prim_to_face_map = pbvh->header.type == PBVH_FACES ?
                                         tri_faces :
                                         pbvh->subdiv_ccg->grid_to_face_map;
  int end;

  if (!prim_scratch) {
    prim_scratch = static_cast<int *>(MEM_malloc_arrayN(pbvh->totprim, sizeof(int), __func__));
  }

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = count <= pbvh->leaf_limit || depth >= PBVH_STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(
            pbvh, prim_to_face_map, material_indices, sharp_faces, offset, count))
    {
      build_leaf(pbvh,
                 corner_verts,
                 corner_tris,
                 tri_faces,
                 hide_poly,
                 vert_bitmap,
                 node_index,
                 prim_bounds,
                 offset,
                 count);

      if (node_index == 0) {
        MEM_SAFE_FREE(prim_scratch);
      }

      return;
    }
  }

  /* Add two child nodes */
  pbvh->nodes[node_index].children_offset = pbvh->nodes.size();
  pbvh->nodes.resize(pbvh->nodes.size() + 2);

  /* Update parent node bounding box */
  update_vb(pbvh->prim_indices, &pbvh->nodes[node_index], prim_bounds, offset, count);

  Bounds<float3> cb_backing;
  if (!below_leaf_limit) {
    /* Find axis with widest range of primitive centroids */
    if (!cb) {
      cb_backing = negative_bounds();
      for (int i = offset + count - 1; i >= offset; i--) {
        const int prim = pbvh->prim_indices[i];
        const float3 center = math::midpoint(prim_bounds[prim].min, prim_bounds[prim].max);
        math::min_max(center, cb_backing.min, cb_backing.max);
      }
      cb = &cb_backing;
    }
    const int axis = math::dominant_axis(cb->max - cb->min);

    /* Partition primitives along that axis */
    end = partition_prim_indices(pbvh->prim_indices,
                                 prim_scratch,
                                 offset,
                                 offset + count,
                                 axis,
                                 math::midpoint(cb->min[axis], cb->max[axis]),
                                 prim_bounds,
                                 prim_to_face_map);
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material_faces(pbvh->prim_indices,
                                           prim_to_face_map,
                                           material_indices,
                                           sharp_faces,
                                           offset,
                                           offset + count - 1);
  }

  /* Build children */
  build_sub(pbvh,
            corner_verts,
            corner_tris,
            tri_faces,
            hide_poly,
            material_indices,
            sharp_faces,
            vert_bitmap,
            pbvh->nodes[node_index].children_offset,
            nullptr,
            prim_bounds,
            offset,
            end - offset,
            prim_scratch,
            depth + 1);
  build_sub(pbvh,
            corner_verts,
            corner_tris,
            tri_faces,
            hide_poly,
            material_indices,
            sharp_faces,
            vert_bitmap,
            pbvh->nodes[node_index].children_offset + 1,
            nullptr,
            prim_bounds,
            end,
            offset + count - end,
            prim_scratch,
            depth + 1);

  if (node_index == 0) {
    MEM_SAFE_FREE(prim_scratch);
  }
}

static void pbvh_build(PBVH *pbvh,
                       const Span<int> corner_verts,
                       const Span<int3> corner_tris,
                       const Span<int> tri_faces,
                       const Span<bool> hide_poly,
                       const Span<int> material_indices,
                       const Span<bool> sharp_faces,
                       MutableSpan<bool> vert_bitmap,
                       const Bounds<float3> *cb,
                       const Span<Bounds<float3>> prim_bounds,
                       int totprim)
{
  if (totprim != pbvh->totprim) {
    pbvh->totprim = totprim;
    pbvh->nodes.clear_and_shrink();

    pbvh->prim_indices.reinitialize(totprim);
    array_utils::fill_index_range<int>(pbvh->prim_indices);
  }

  pbvh->nodes.resize(1);

  build_sub(pbvh,
            corner_verts,
            corner_tris,
            tri_faces,
            hide_poly,
            material_indices,
            sharp_faces,
            vert_bitmap,
            0,
            cb,
            prim_bounds,
            0,
            totprim,
            nullptr,
            0);
}

void BKE_pbvh_set_face_areas(PBVH *pbvh, float *face_areas)
{
  pbvh->face_areas = face_areas;
}

void BKE_pbvh_show_orig_set(PBVH *pbvh, bool show_orig)
{
  pbvh->show_orig = show_orig;
}

bool BKE_pbvh_show_orig_get(PBVH *pbvh)
{
  return pbvh->show_orig;
}

#if 0
static void pbvh_draw_args_init(const Mesh &mesh, PBVH *pbvh, PBVH_GPU_Args *args, PBVHNode *node)
{
  memset((void *)args, 0, sizeof(*args));

  args->pbvh_type = pbvh->header.type;
  args->mesh_grids_num = pbvh->totgrid;
  args->node = node;
  args->origco = pbvh->origco;
  args->origno = pbvh->origno;

  args->grid_hidden = pbvh->grid_hidden;
  args->face_sets_color_default = mesh.face_sets_color_default;
  args->face_sets_color_seed = mesh.face_sets_color_seed;
  args->vert_positions = pbvh->vert_positions;
  if (pbvh->mesh) {
    args->corner_verts = pbvh->corner_verts;
    args->corner_edges = pbvh->mesh->corner_edges();
  }
  args->faces = pbvh->faces;
  args->updategen = node->updategen;

  if (ELEM(pbvh->header.type, PBVH_FACES, PBVH_GRIDS)) {
    args->hide_poly = pbvh->face_data ? static_cast<const bool *>(CustomData_get_layer_named(
                                            pbvh->face_data, CD_PROP_BOOL, ".hide_poly")) :
                                        nullptr;
  }

  args->active_color = mesh.active_color_attribute;
  args->render_color = mesh.default_color_attribute;

  switch (pbvh->header.type) {
    case PBVH_FACES:
      args->vert_data = pbvh->vert_data;
      args->corner_data = pbvh->corner_data;
      args->face_data = pbvh->face_data;
      args->me = pbvh->mesh;
      args->faces = pbvh->faces;
      args->vert_normals = pbvh->vert_normals;
      args->face_normals = pbvh->face_normals;

      args->prim_indices = node->prim_indices;
      args->corner_tris = pbvh->corner_tris;
      args->tri_faces = pbvh->corner_tri_faces;
      break;
    case PBVH_GRIDS:
      args->vert_data = pbvh->vert_data;
      args->corner_data = pbvh->corner_data;
      args->face_data = pbvh->face_data;
      args->ccg_key = pbvh->gridkey;
      args->me = pbvh->mesh;
      args->grid_indices = node->prim_indices;
      args->subdiv_ccg = pbvh->subdiv_ccg;
      args->faces = pbvh->faces;
      args->corner_tris = pbvh->corner_tris;
      args->tri_faces = pbvh->corner_tri_faces;

      args->mesh_grids_num = pbvh->totgrid;
      args->grids = pbvh->grids;
      args->vert_normals = pbvh->vert_normals;
      break;
    case PBVH_BMESH:
      args->bm = pbvh->header.bm;
      args->active_color = pbvh->mesh->active_color_attribute;
      args->render_color = pbvh->mesh->default_color_attribute;

      args->vert_data = &args->bm->vdata;
      args->corner_data = &args->bm->ldata;
      args->face_data = &args->bm->pdata;
      args->bm_faces = node->bm_faces;
      args->bm_other_verts = node->bm_other_verts;
      args->bm_unique_verts = node->bm_unique_verts;
      args->cd_mask_layer = CustomData_get_offset_named(
          &pbvh->header.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

      args->tribuf = node->tribuf;
      args->tri_buffers = node->tri_buffers->data();
      args->tot_tri_buffers = node->tri_buffers->size();

      args->show_orig = pbvh->show_orig;
      break;
  }
}
#endif

static blender::draw::pbvh::PBVH_GPU_Args pbvh_draw_args_init(const Mesh &mesh,
                                                              PBVH &pbvh,
                                                              const PBVHNode &node)
{
  /* TODO: Use an explicit argument for the original mesh to avoid relying on #PBVH::mesh. */
  blender::draw::pbvh::PBVH_GPU_Args args{};

  args.pbvh_type = pbvh.header.type;

  args.face_sets_color_default = pbvh.mesh ? pbvh.mesh->face_sets_color_default :
                                             mesh.face_sets_color_default;
  args.face_sets_color_seed = pbvh.mesh ? pbvh.mesh->face_sets_color_seed :
                                          mesh.face_sets_color_seed;

  args.active_color = mesh.active_color_attribute;
  args.render_color = mesh.default_color_attribute;

  switch (pbvh.header.type) {
    case PBVH_FACES:
      args.vert_data = &mesh.vert_data;
      args.corner_data = &mesh.corner_data;
      args.face_data = &mesh.face_data;
      args.mesh = pbvh.mesh;
      args.vert_positions = pbvh.vert_positions;
      args.corner_verts = mesh.corner_verts();
      args.corner_edges = mesh.corner_edges();
      args.corner_tris = pbvh.corner_tris;
      args.vert_normals = pbvh.vert_normals;
      args.face_normals = pbvh.face_normals;
      /* Retrieve data from the original mesh. Ideally that would be passed to this function to
       * make it clearer when each is used. */
      args.hide_poly = *pbvh.mesh->attributes().lookup<bool>(".hide_poly", AttrDomain::Face);

      args.prim_indices = node.prim_indices;
      args.tri_faces = mesh.corner_tri_faces();
      break;
    case PBVH_GRIDS:
      args.vert_data = &pbvh.mesh->vert_data;
      args.corner_data = &pbvh.mesh->corner_data;
      args.face_data = &pbvh.mesh->face_data;
      args.ccg_key = pbvh.gridkey;
      args.mesh = pbvh.mesh;
      args.grid_indices = node.prim_indices;
      args.subdiv_ccg = pbvh.subdiv_ccg;
      args.grids = pbvh.subdiv_ccg->grids;
      args.vert_normals = pbvh.vert_normals;
      break;
    case PBVH_BMESH:
      args.bm = pbvh.header.bm;
      args.vert_data = &args.bm->vdata;
      args.corner_data = &args.bm->ldata;
      args.face_data = &args.bm->pdata;
      args.bm_faces = node.bm_faces;
      args.cd_mask_layer = CustomData_get_offset_named(
          &pbvh.header.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

      args.tribuf = node.tribuf;
      args.tri_buffers = *node.tri_buffers;

      break;
  }

  args.show_orig = pbvh.show_orig;

  return args;
}

#ifdef VALIDATE_UNIQUE_NODE_FACES
static void pbvh_validate_node_prims(PBVH *pbvh)
{
  int faces_face = 0;

  if (pbvh->header.type == PBVH_BMESH) {
    return;
  }

  for (int i = 0; i < pbvh->nodes.size(); i++) {
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

      faces_face = max_ii(faces_face, face_i + 1);
    }
  }

  int *facemap = (int *)MEM_malloc_arrayN(faces_face, sizeof(*facemap), __func__);

  for (int i = 0; i < faces_face; i++) {
    facemap[i] = -1;
  }

  for (int i = 0; i < pbvh->nodes.size(); i++) {
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
  pbvh->edges = mesh->edges();
  pbvh->corner_verts = mesh->corner_verts();
  pbvh->corner_edges = mesh->corner_edges();
  pbvh->corner_tri_faces = mesh->corner_tri_faces();

  pbvh->seam_edges = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->edge_data, CD_PROP_BOOL, ".uv_seam"));
  pbvh->sharp_edges = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->edge_data, CD_PROP_BOOL, "sharp_edge"));

  if (!pbvh->deformed) {
    /* Deformed data not matching the original mesh are owned directly by the PBVH, and are
     * set separately by #BKE_pbvh_vert_coords_apply. */
    pbvh->vert_positions = mesh->vert_positions_for_write();
    pbvh->vert_normals = mesh->vert_normals();
    pbvh->face_normals = mesh->face_normals();
  }

  pbvh->face_areas = static_cast<float *>(CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_FLOAT2, SCULPT_ATTRIBUTE_NAME(face_areas), mesh->faces_num));

  BKE_pbvh_update_hide_attributes_from_mesh(pbvh);

  pbvh->vert_data = &mesh->vert_data;
  pbvh->corner_data = &mesh->corner_data;
  pbvh->face_data = &mesh->face_data;
}

namespace blender::bke::pbvh {
PBVH *build_mesh(Mesh *mesh)
{
  PBVH *pbvh = MEM_new<PBVH>(__func__);
  pbvh->header.type = PBVH_FACES;

  const int totvert = mesh->verts_num;
  const int corner_tris_num = poly_to_tri_count(mesh->faces_num, mesh->corners_num);
  MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
  const OffsetIndices<int> faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  pbvh->corner_tris.reinitialize(corner_tris_num);
  mesh::corner_tris_calc(vert_positions, faces, corner_verts, pbvh->corner_tris);
  const Span<int3> corner_tris = pbvh->corner_tris;

  pbvh->mesh = mesh;

  BKE_pbvh_update_mesh_pointers(pbvh, mesh);
  const Span<int> tri_faces = pbvh->corner_tri_faces;

  Array<bool> vert_bitmap(totvert, false);
  pbvh->totvert = totvert;

#ifdef TEST_PBVH_FACE_SPLIT
  /* Use lower limit to increase probability of
   * edge cases.
   */
  pbvh->leaf_limit = 100;
#else
  pbvh->leaf_limit = LEAF_LIMIT;
#endif

  /* For each face, store the AABB and the AABB centroid */
  Array<Bounds<float3>> prim_bounds(corner_tris_num);
  const Bounds<float3> cb = threading::parallel_reduce(
      corner_tris.index_range(),
      1024,
      negative_bounds(),
      [&](const IndexRange range, const Bounds<float3> &init) {
        Bounds<float3> current = init;
        for (const int i : range) {
          const int3 &tri = corner_tris[i];
          Bounds<float3> &bounds = prim_bounds[i];
          bounds = {vert_positions[corner_verts[tri[0]]]};
          math::min_max(vert_positions[corner_verts[tri[1]]], bounds.min, bounds.max);
          math::min_max(vert_positions[corner_verts[tri[2]]], bounds.min, bounds.max);
          const float3 center = math::midpoint(prim_bounds[i].min, prim_bounds[i].max);
          math::min_max(center, current.min, current.max);
        }
        return current;
      },
      [](const Bounds<float3> &a, const Bounds<float3> &b) { return bounds::merge(a, b); });

  if (corner_tris_num) {
    const AttributeAccessor attributes = mesh->attributes();
    const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", AttrDomain::Face);
    const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);
    const VArraySpan sharp_face = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
    pbvh_build(pbvh,
               corner_verts,
               corner_tris,
               tri_faces,
               hide_poly,
               material_index,
               sharp_face,
               vert_bitmap,
               &cb,
               prim_bounds,
               corner_tris_num);

#ifdef TEST_PBVH_FACE_SPLIT
    test_face_boundaries(pbvh, tri_faces);
#endif
  }

  BKE_pbvh_update_active_vcol(pbvh, mesh);

#ifdef VALIDATE_UNIQUE_NODE_FACES
  pbvh_validate_node_prims(pbvh);
#endif

  return pbvh;
}

PBVH *build_grids(const CCGKey *key, Mesh *mesh, SubdivCCG *subdiv_ccg)
{
  PBVH *pbvh = MEM_new<PBVH>(__func__);
  pbvh->header.type = PBVH_GRIDS;

  pbvh->gridkey = *key;
  pbvh->subdiv_ccg = subdiv_ccg;

  /* Find maximum number of grids per face. */
  int max_grids = 1;
  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    max_grids = max_ii(max_grids, faces[i].size());
  }

  const Span<CCGElem *> grids = subdiv_ccg->grids;

  /* Ensure leaf limit is at least 4 so there's room
   * to split at original face boundaries.
   * Fixes #102209.
   */
  pbvh->leaf_limit = max_ii(LEAF_LIMIT / (key->grid_area), max_grids);

  /* We also need the base mesh for PBVH draw. */
  pbvh->mesh = mesh;

  /* For each grid, store the AABB and the AABB centroid */
  Array<Bounds<float3>> prim_bounds(grids.size());
  const Bounds<float3> cb = threading::parallel_reduce(
      grids.index_range(),
      1024,
      negative_bounds(),
      [&](const IndexRange range, const Bounds<float3> &init) {
        Bounds<float3> current = init;
        for (const int i : range) {
          CCGElem *grid = grids[i];
          prim_bounds[i] = negative_bounds();
          for (const int j : IndexRange(key->grid_area)) {
            const float3 position = float3(CCG_elem_offset_co(key, grid, j));
            math::min_max(position, prim_bounds[i].min, prim_bounds[i].max);
          }
          const float3 center = math::midpoint(prim_bounds[i].min, prim_bounds[i].max);
          math::min_max(center, current.min, current.max);
        }
        return current;
      },
      [](const Bounds<float3> &a, const Bounds<float3> &b) { return bounds::merge(a, b); });

  if (!grids.is_empty()) {
    const AttributeAccessor attributes = mesh->attributes();
    const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);
    const VArraySpan sharp_face = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
    pbvh_build(
        pbvh, {}, {}, {}, {}, material_index, sharp_face, {}, &cb, prim_bounds, grids.size());
  }

#ifdef VALIDATE_UNIQUE_NODE_FACES
  pbvh_validate_node_prims(pbvh);
#endif

  return pbvh;
}
}  // namespace blender::bke::pbvh

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

namespace blender::bke::pbvh {
void free(PBVH *pbvh)
{
  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_Leaf) {
      if (node.draw_batches) {
        draw::pbvh::node_free(node.draw_batches);
      }
      if (node.bm_faces) {
        MEM_delete<DyntopoSet<BMFace>>(node.bm_faces);
      }
      if (node.bm_unique_verts) {
        MEM_delete<DyntopoSet<BMVert>>(node.bm_unique_verts);
      }
      if (node.bm_other_verts) {
        MEM_delete<DyntopoSet<BMVert>>(node.bm_other_verts);
      }

      if (node.tribuf || node.tri_buffers) {
        BKE_pbvh_bmesh_free_tris(pbvh, &node);
      }

      if (node.flag & (PBVH_Leaf | PBVH_TexLeaf)) {
        blender::bke::pbvh::node_pixels_free(&node);
      }
    }
  }

  blender::bke::pbvh::pixels_free(pbvh);
  MEM_delete(pbvh);
}
}  // namespace blender::bke::pbvh

static void pbvh_iter_begin(PBVHIter *iter, PBVH *pbvh, blender::FunctionRef<bool(PBVHNode &)> scb)
{
  iter->pbvh = pbvh;
  iter->scb = scb;

  iter->stack = iter->stackfixed;
  iter->stackspace = PBVH_STACK_FIXED_DEPTH;

  iter->stack[0].node = &pbvh->nodes.first();
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
      iter->stack = (PBVHStack *)MEM_reallocN(iter->stack, sizeof(PBVHStack) * iter->stackspace);
    }
    else {
      iter->stack = (PBVHStack *)MEM_mallocN(sizeof(PBVHStack) * iter->stackspace, "PBVHStack");
      memcpy((void *)iter->stack, (void *)iter->stackfixed, sizeof(PBVHStack) * iter->stacksize);
    }
  }

  iter->stack[iter->stacksize].node = node;
  iter->stack[iter->stacksize].revisiting = revisiting;
  iter->stacksize++;
}

static PBVHNode *pbvh_iter_next(PBVHIter *iter, PBVHNodeFlags leaf_flag = PBVH_Leaf)
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

    if (iter->scb && !iter->scb(*node)) {
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

    float ff = dot_v3v3(node->vb.min, node->vb.max);
    if (isnan(ff) || !isfinite(ff)) {
      printf("%s: nan! totf: %d totv: %d\n",
             __func__,
             node->bm_faces ? node->bm_faces->size() : 0,
             node->bm_unique_verts ? node->bm_unique_verts->size() : 0);
    }

    if (iter->scb && !iter->scb(*node)) {
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

  struct node_tree *left;
  struct node_tree *right;
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
                          const FunctionRef<void(PBVHNode &node, float *tmin)> hit_fn,
                          float *tmin)
{
  if (tree->left) {
    traverse_tree(tree->left, hit_fn, tmin);
  }

  hit_fn(*tree->data, tmin);

  if (tree->right) {
    traverse_tree(tree->right, hit_fn, tmin);
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

namespace blender::bke::pbvh {
void search_callback(PBVH &pbvh,
                     FunctionRef<bool(PBVHNode &)> filter_fn,
                     FunctionRef<void(PBVHNode &)> hit_fn)
{
  if (pbvh.nodes.is_empty()) {
    return;
  }
  PBVHIter iter;
  PBVHNode *node;

  pbvh_iter_begin(&iter, &pbvh, filter_fn);

  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
    if (node->flag & PBVH_Leaf) {
      hit_fn(*node);
    }
  }

  pbvh_iter_end(&iter);
}
}  // namespace blender::bke::pbvh

static void search_callback_occluded(PBVH *pbvh,
                                     const FunctionRef<bool(PBVHNode &)> scb,
                                     const FunctionRef<void(PBVHNode &node, float *tmin)> hit_fn)
{
  if (pbvh->nodes.is_empty()) {
    return;
  }
  PBVHIter iter;
  PBVHNode *node;
  node_tree *tree = nullptr;

  pbvh_iter_begin(&iter, pbvh, scb);

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
    traverse_tree(tree, hit_fn, &tmin);
    free_tree(tree);
  }
}

static bool update_search(PBVHNode *node, const int flag)
{
  if (node->flag & PBVH_Leaf) {
    return (node->flag & flag) != 0;
  }

  return true;
}

struct PBVHUpdateData {
  PBVH *pbvh;
  Mesh *mesh;
  Span<PBVHNode *> nodes;

  int flag = 0;
  bool show_sculpt_face_sets = false;
  PBVHAttrReq *attrs = nullptr;
  int attrs_num = 0;

  PBVHUpdateData(PBVH *pbvh_, Span<PBVHNode *> nodes_) : pbvh(pbvh_), nodes(nodes_) {}
};

static void normals_calc_faces(const Span<float3> positions,
                               const OffsetIndices<int> faces,
                               const Span<int> corner_verts,
                               const Span<int> face_indices,
                               MutableSpan<float3> face_normals)
{
  for (const int i : face_indices) {
    face_normals[i] = mesh::face_normal_calc(positions, corner_verts.slice(faces[i]));
  }
}

static void calc_boundary_face_normals(const Span<float3> positions,
                                       const OffsetIndices<int> faces,
                                       const Span<int> corner_verts,
                                       const Span<int> face_indices,
                                       MutableSpan<float3> face_normals)
{
  threading::parallel_for(face_indices.index_range(), 512, [&](const IndexRange range) {
    normals_calc_faces(positions, faces, corner_verts, face_indices.slice(range), face_normals);
  });
}

static void calc_node_face_normals(const Span<float3> positions,
                                   const OffsetIndices<int> faces,
                                   const Span<int> corner_verts,
                                   const PBVH &pbvh,
                                   const Span<const PBVHNode *> nodes,
                                   MutableSpan<float3> face_normals)
{
  threading::EnumerableThreadSpecific<Vector<int>> all_index_data;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<int> &node_faces = all_index_data.local();
    for (const PBVHNode *node : nodes.slice(range)) {
      normals_calc_faces(positions,
                         faces,
                         corner_verts,
                         pbvh::node_face_indices_calc_mesh(pbvh, *node, node_faces),
                         face_normals);
    }
  });
}

static void normals_calc_verts_simple(const GroupedSpan<int> vert_to_face_map,
                                      const Span<float3> face_normals,
                                      const Span<int> verts,
                                      MutableSpan<float3> vert_normals)
{
  for (const int vert : verts) {
    float3 normal(0.0f);
    for (const int face : vert_to_face_map[vert]) {
      normal += face_normals[face];
    }
    vert_normals[vert] = math::normalize(normal);
  }
}

static void calc_boundary_vert_normals(const GroupedSpan<int> vert_to_face_map,
                                       const Span<float3> face_normals,
                                       const Span<int> verts,
                                       MutableSpan<float3> vert_normals)
{
  threading::parallel_for(verts.index_range(), 1024, [&](const IndexRange range) {
    normals_calc_verts_simple(vert_to_face_map, face_normals, verts.slice(range), vert_normals);
  });
}

static void calc_node_vert_normals(const GroupedSpan<int> vert_to_face_map,
                                   const Span<float3> face_normals,
                                   const Span<PBVHNode *> nodes,
                                   MutableSpan<float3> vert_normals)
{
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const PBVHNode *node : nodes.slice(range)) {
      normals_calc_verts_simple(vert_to_face_map,
                                face_normals,
                                node->vert_indices.as_span().take_front(node->uniq_verts),
                                vert_normals);
    }
  });
}

static void update_normals_faces(PBVH &pbvh, Span<PBVHNode *> nodes, Mesh &mesh)
{
  /* Position changes are tracked on a per-node level, so all the vertex and face normals for every
   * affected node are recalculated. However, the additional complexity comes from the fact that
   * changing vertex normals also changes surrounding face normals. Those changed face normals then
   * change the normals of all connected vertices, which can be in other nodes. So the set of
   * vertices that need recalculated normals can propagate into unchanged/untagged PBVH nodes.
   *
   * Currently we have no good way of finding neighboring PBVH nodes, so we use the vertex to
   * face topology map to find the neighboring vertices that need normal recalculation.
   *
   * Those boundary face and vertex indices are deduplicated with #VectorSet in order to avoid
   * duplicate work recalculation for the same vertex, and to make parallel storage for vertices
   * during recalculation thread-safe. */
  const Span<float3> positions = pbvh.vert_positions;
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  VectorSet<int> boundary_faces;
  for (const PBVHNode *node : nodes) {
    for (const int vert : node->vert_indices.as_span().drop_front(node->uniq_verts)) {
      boundary_faces.add_multiple(pbvh.vert_to_face_map[vert]);
    }
  }

  VectorSet<int> boundary_verts;
  threading::parallel_invoke(
      [&]() {
        if (pbvh.deformed) {
          calc_node_face_normals(
              positions, faces, corner_verts, pbvh, nodes, pbvh.face_normals_deformed);
          calc_boundary_face_normals(
              positions, faces, corner_verts, boundary_faces, pbvh.face_normals_deformed);
        }
        else {
          mesh.runtime->face_normals_cache.update([&](Vector<float3> &r_data) {
            calc_node_face_normals(positions, faces, corner_verts, pbvh, nodes, r_data);
            calc_boundary_face_normals(positions, faces, corner_verts, boundary_faces, r_data);
          });
          /* #SharedCache::update() reallocates cached vectors if they were shared initially. */
          pbvh.face_normals = mesh.runtime->face_normals_cache.data();
        }
      },
      [&]() {
        /* Update all normals connected to affected faces, even if not explicitly tagged. */
        boundary_verts.reserve(boundary_faces.size());
        for (const int face : boundary_faces) {
          boundary_verts.add_multiple(corner_verts.slice(faces[face]));
        }
      });

  if (pbvh.deformed) {
    calc_node_vert_normals(
        pbvh.vert_to_face_map, pbvh.face_normals, nodes, pbvh.vert_normals_deformed);
    calc_boundary_vert_normals(
        pbvh.vert_to_face_map, pbvh.face_normals, boundary_verts, pbvh.vert_normals_deformed);
  }
  else {
    mesh.runtime->vert_normals_cache.update([&](Vector<float3> &r_data) {
      calc_node_vert_normals(pbvh.vert_to_face_map, pbvh.face_normals, nodes, r_data);
      calc_boundary_vert_normals(pbvh.vert_to_face_map, pbvh.face_normals, boundary_verts, r_data);
    });
    pbvh.vert_normals = mesh.runtime->vert_normals_cache.data();
  }

  for (PBVHNode *node : nodes) {
    node->flag &= ~PBVH_UpdateNormals;
  }
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
      if (vd.mask < 1.0f) {
        has_unmasked = true;
      }
      if (vd.mask > 0.0f) {
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

static void node_update_bounds(PBVH &pbvh, PBVHNode &node, const PBVHNodeFlags flag)
{
  if ((flag & PBVH_UpdateBB) && (node.flag & PBVH_UpdateBB)) {
    /* don't clear flag yet, leave it for flushing later */
    /* Note that bvh usage is read-only here, so no need to thread-protect it. */
    update_node_vb(&pbvh, &node, flag);
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

bool BKE_pbvh_get_color_layer(PBVH *pbvh,
                              Mesh *me,
                              CustomDataLayer **r_layer,
                              AttrDomain *r_domain)
{
  *r_layer = BKE_id_attribute_search_for_write(
      &me->id, me->active_color_attribute, CD_MASK_COLOR_ALL, ATTR_DOMAIN_MASK_COLOR);

  if (!*r_layer || !ELEM((*r_layer)->type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
    *r_layer = nullptr;
    *r_domain = AttrDomain::Point;
    return false;
  }

  AttrDomain domain = BKE_id_attribute_domain(&me->id, *r_layer);

  if (!ELEM(domain, AttrDomain::Point, AttrDomain::Corner)) {
    *r_layer = nullptr;
    *r_domain = AttrDomain::Point;
    return false;
  }

  if (pbvh && BKE_pbvh_type(pbvh) == PBVH_BMESH) {
    CustomData *data;

    if (domain == AttrDomain::Point) {
      data = &pbvh->header.bm->vdata;
    }
    else if (domain == AttrDomain::Corner) {
      data = &pbvh->header.bm->ldata;
    }
    else {
      *r_layer = nullptr;
      *r_domain = AttrDomain::Point;

      BLI_assert_unreachable();
      return false;
    }

    int layer_i = CustomData_get_named_layer_index(
        data, eCustomDataType((*r_layer)->type), (*r_layer)->name);
    if (layer_i == -1) {
      printf("%s: bmesh lacks color attribute %s\n", __func__, (*r_layer)->name);

      *r_layer = nullptr;
      *r_domain = AttrDomain::Point;
      return false;
    }

    *r_layer = &data->layers[layer_i];
  }

  *r_domain = domain;

  return true;
}

static void node_update_draw_buffers(const Mesh &mesh, PBVH &pbvh, PBVHNode &node)
{
  /* Create and update draw buffers. The functions called here must not
   * do any OpenGL calls. Flags are not cleared immediately, that happens
   * after GPU_pbvh_buffer_flush() which does the final OpenGL calls. */

  if (node.flag & PBVH_RebuildDrawBuffers) {
    PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, node);
    node.draw_batches = draw::pbvh::node_create(args);
  }

  if (node.flag & PBVH_UpdateDrawBuffers) {
    node.updategen++;
    node.debug_draw_gen++;

    if (node.draw_batches) {
      PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, node);
      draw::pbvh::node_update(node.draw_batches, args);
    }
  }
}

namespace blender::bke::pbvh {
void free_draw_buffers(PBVH * /* pbvh */, PBVHNode *node)
{
  if (node->draw_batches) {
    blender::draw::pbvh::node_free(node->draw_batches);
    node->draw_batches = nullptr;
  }
}
}  // namespace blender::bke::pbvh

static void pbvh_update_draw_buffers(const Mesh &mesh,
                                     PBVH *pbvh,
                                     Span<PBVHNode *> nodes,
                                     int update_flag)
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
        blender::bke::pbvh::free_draw_buffers(pbvh, node);
      }
      else if ((node->flag & PBVH_UpdateDrawBuffers) && node->draw_batches) {
        PBVH_GPU_Args args = pbvh_draw_args_init(mesh, *pbvh, *node);
        blender::draw::pbvh::update_pre(node->draw_batches, args);
      }
    }
  }

  /* Parallel creation and update of draw buffers. */

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_draw_buffers(mesh, *pbvh, *node);
    }
  });

  /* Flush buffers uses OpenGL, so not in parallel. */
  for (PBVHNode *node : nodes) {
    if (node->flag & PBVH_UpdateDrawBuffers) {
      if (node->draw_batches) {
        draw::pbvh::node_gpu_flush(node->draw_batches);
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

  update_node_vb(pbvh, node, update);

  return update;
}

namespace blender::bke::pbvh {
void update_bounds(PBVH &pbvh, int flag)
{
  if (pbvh.nodes.is_empty()) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      &pbvh, [&](PBVHNode &node) { return update_search(&node, flag); });

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw)) {
    pbvh_update_BB_redraw(&pbvh, nodes, flag);
  }

  if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB)) {
    pbvh_flush_bb(&pbvh, &pbvh.nodes.first(), flag);
  }
}
}  // namespace blender::bke::pbvh

namespace blender::bke::pbvh {
void node_update_mask_mesh(const Span<float> mask, PBVHNode &node)
{
  const bool fully_masked = std::all_of(node.vert_indices.begin(),
                                        node.vert_indices.end(),
                                        [&](const int vert) { return mask[vert] == 1.0f; });
  const bool fully_unmasked = std::all_of(node.vert_indices.begin(),
                                          node.vert_indices.end(),
                                          [&](const int vert) { return mask[vert] <= 0.0f; });
  SET_FLAG_FROM_TEST(node.flag, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag, fully_unmasked, PBVH_FullyUnmasked);
  node.flag &= ~PBVH_UpdateMask;
}

void node_update_mask_grids(const CCGKey &key, const Span<CCGElem *> grids, PBVHNode &node)
{
  BLI_assert(key.has_mask);
  bool fully_masked = true;
  bool fully_unmasked = true;
  for (const int grid : node.prim_indices) {
    CCGElem *elem = grids[grid];
    for (const int i : IndexRange(key.grid_area)) {
      const float mask = *CCG_elem_offset_mask(&key, elem, i);
      fully_masked &= mask == 1.0f;
      fully_unmasked &= mask <= 0.0f;
    }
  }
  SET_FLAG_FROM_TEST(node.flag, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag, fully_unmasked, PBVH_FullyUnmasked);
  node.flag &= ~PBVH_UpdateMask;
}

void node_update_mask_bmesh(const int mask_offset, PBVHNode &node)
{
  BLI_assert(mask_offset != -1);
  bool fully_masked = true;
  bool fully_unmasked = true;
  for (const BMVert *vert : *node.bm_unique_verts) {
    fully_masked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) == 1.0f;
    fully_unmasked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) <= 0.0f;
  }
  for (const BMVert *vert : *node.bm_other_verts) {
    fully_masked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) == 1.0f;
    fully_unmasked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) <= 0.0f;
  }
  SET_FLAG_FROM_TEST(node.flag, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag, fully_unmasked, PBVH_FullyUnmasked);
  node.flag &= ~PBVH_UpdateMask;
}

void update_mask(PBVH &pbvh)
{
  using namespace blender;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      &pbvh, [&](PBVHNode &node) { return update_search(&node, PBVH_UpdateMask); });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.as_span().slice(range)) {
      node_update_mask_redraw(pbvh, *node);
    }
  });
}

void update_vertex_data(PBVH &pbvh, int flag)
{
  using namespace blender;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      &pbvh, [&](PBVHNode &node) { return update_search(&node, flag); });

  if (flag & (PBVH_UpdateColor)) {
    for (PBVHNode *node : nodes) {
      node->flag |= PBVH_UpdateRedraw | PBVH_UpdateDrawBuffers | PBVH_UpdateColor;
    }
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
  const int *grid_indices;
  int totgrid, i;

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, nullptr, &grids);
  BitGroupVector<> &grid_hidden = *BKE_pbvh_grid_hidden(pbvh);
  CCGKey key = *BKE_pbvh_get_grid_key(pbvh);

  for (i = 0; i < totgrid; i++) {
    int g = grid_indices[i], x, y;
    blender::BoundedBitSpan gh = grid_hidden[g];

    if (gh.is_empty()) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }

    for (y = 0; y < key.grid_size; y++) {
      for (x = 0; x < key.grid_size; x++) {
        if (!gh[y * key.grid_size + x]) {
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
  for (BMVert *v : *node->bm_unique_verts) {
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_fully_hidden_set(node, false);
      return;
    }
  }

  for (BMVert *v : *node->bm_other_verts) {
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

void node_update_visibility_mesh(const Span<bool> hide_vert, PBVHNode &node)
{
  BLI_assert(!hide_vert.is_empty());
  const bool fully_hidden = std::all_of(node.vert_indices.begin(),
                                        node.vert_indices.end(),
                                        [&](const int vert) { return hide_vert[vert]; });
  SET_FLAG_FROM_TEST(node.flag, fully_hidden, PBVH_FullyHidden);
  node.flag &= ~PBVH_UpdateVisibility;
}

static void update_visibility_faces(const Mesh &mesh, const Span<PBVHNode *> nodes)
{
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
  if (hide_vert.is_empty()) {
    for (PBVHNode *node : nodes) {
      node->flag &= ~PBVH_FullyHidden;
      node->flag &= ~PBVH_UpdateVisibility;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_visibility_mesh(hide_vert, *node);
    }
  });
}

void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, PBVHNode &node)
{
  BLI_assert(!grid_hidden.is_empty());
  const bool fully_hidden = std::none_of(
      node.prim_indices.begin(), node.prim_indices.end(), [&](const int grid) {
        return bits::any_bit_unset(grid_hidden[grid]);
      });
  SET_FLAG_FROM_TEST(node.flag, fully_hidden, PBVH_FullyHidden);
  node.flag &= ~PBVH_UpdateVisibility;
}

static void update_visibility_grids(PBVH &pbvh, const Span<PBVHNode *> nodes)
{
  const BitGroupVector<> &grid_hidden = pbvh.subdiv_ccg->grid_hidden;
  if (grid_hidden.is_empty()) {
    for (PBVHNode *node : nodes) {
      node->flag &= ~PBVH_FullyHidden;
      node->flag &= ~PBVH_UpdateVisibility;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_visibility_grids(grid_hidden, *node);
    }
  });
}

void node_update_visibility_bmesh(PBVHNode &node)
{
  const bool unique_hidden = std::all_of(
      node.bm_unique_verts->begin(), node.bm_unique_verts->end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  const bool other_hidden = std::all_of(
      node.bm_other_verts->begin(), node.bm_other_verts->end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  SET_FLAG_FROM_TEST(node.flag, unique_hidden && other_hidden, PBVH_FullyHidden);
  node.flag &= ~PBVH_UpdateVisibility;
}

static void update_visibility_bmesh(const Span<PBVHNode *> nodes)
{
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      node_update_visibility_bmesh(*node);
    }
  });
}

void update_visibility(PBVH &pbvh)
{
  Vector<PBVHNode *> nodes = search_gather(
      &pbvh, [&](PBVHNode &node) { return update_search(&node, PBVH_UpdateVisibility); });

  switch (BKE_pbvh_type(&pbvh)) {
    case PBVH_FACES:
      update_visibility_faces(*pbvh.mesh, nodes);
      break;
    case PBVH_GRIDS:
      update_visibility_grids(pbvh, nodes);
      break;
    case PBVH_BMESH:
      update_visibility_bmesh(nodes);
      break;
  }
}
}  // namespace blender::bke::pbvh

Bounds<float3> BKE_pbvh_redraw_BB(PBVH *pbvh)
{
  if (pbvh->nodes.is_empty()) {
    return Bounds<float3>();
  }
  PBVHIter iter;
  PBVHNode *node;
  Bounds<float3> bb = negative_bounds();

  pbvh_iter_begin(&iter, pbvh, {});

  while ((node = pbvh_iter_next(&iter))) {
    if (node->flag & PBVH_UpdateRedraw) {
      bb = bounds::merge(bb, node->vb);
    }
  }

  pbvh_iter_end(&iter);

  return bb;
}

namespace blender::bke::pbvh {

IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        const Span<const PBVHNode *> nodes,
                                        IndexMaskMemory &memory)
{
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  /* Using a #VectorSet for index deduplication would also work, but the performance gets much
   * worse with large selections since the loop would be single-threaded. A boolean array has an
   * overhead regardless of selection size, but that is small. */
  Array<bool> faces_to_update(subdiv_ccg.faces.size(), false);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const PBVHNode *node : nodes.slice(range)) {
      for (const int grid : node->prim_indices) {
        faces_to_update[grid_to_face_map[grid]] = true;
      }
    }
  });
  return IndexMask::from_bools(faces_to_update, memory);
}

}  // namespace blender::bke::pbvh

/***************************** PBVH Access ***********************************/

bool BKE_pbvh_has_faces(const PBVH *pbvh)
{
  if (pbvh->header.type == PBVH_BMESH) {
    return (pbvh->header.bm->totface != 0);
  }

  return (pbvh->totprim != 0);
}

Bounds<float3> BKE_pbvh_bounding_box(const PBVH *pbvh)
{
  if (pbvh->nodes.is_empty()) {
    return Bounds<float3>();
  }

  return pbvh->nodes[0].vb;
}

BitGroupVector<> *BKE_pbvh_grid_hidden(const PBVH *pbvh)
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

BitGroupVector<> *BKE_pbvh_get_grid_visibility(const PBVH *pbvh)
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
  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_Leaf) {
      node.flag |= PBVH_RebuildPixels;
    }
  }
}

void BKE_pbvh_node_mark_update_visibility(PBVHNode *node)
{
  node->flag |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                PBVH_UpdateRedraw | PBVH_UpdateTris | PBVH_UpdateTriAreas;
}

void BKE_pbvh_vert_tag_update_normal_visibility(PBVHNode *node)
{
  node->flag |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                PBVH_UpdateRedraw | PBVH_UpdateCurvatureDir | PBVH_UpdateTris |
                PBVH_UpdateTriAreas;
}

void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node)
{
  node->flag |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw |
                PBVH_UpdateCurvatureDir | PBVH_UpdateTriAreas;
}

void BKE_pbvh_node_mark_redraw(PBVHNode *node)
{
  node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_positions_update(PBVHNode *node)
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

bool BKE_pbvh_node_fully_hidden_get(const PBVHNode *node)
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

bool BKE_pbvh_node_fully_masked_get(const PBVHNode *node)
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

blender::Span<int> BKE_pbvh_node_get_loops(const PBVHNode *node)
{
  return node->loop_indices;
}

blender::Span<int> BKE_pbvh_node_get_vert_indices(const PBVHNode *node)
{
  return node->vert_indices;
}

blender::Span<int> BKE_pbvh_node_get_unique_vert_indices(const PBVHNode *node)
{
  return node->vert_indices.as_span().take_front(node->uniq_verts);
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

namespace blender::bke::pbvh {

Span<int> node_face_indices_calc_mesh(const PBVH &pbvh, const PBVHNode &node, Vector<int> &faces)
{
  faces.clear();
  const Span<int> tri_faces = pbvh.corner_tri_faces;
  int prev_face = -1;
  for (const int tri : node.prim_indices) {
    const int face = tri_faces[tri];
    if (face != prev_face) {
      faces.append(face);
      prev_face = face;
    }
  }
  return faces.as_span();
}

Span<int> node_face_indices_calc_grids(const PBVH &pbvh, const PBVHNode &node, Vector<int> &faces)
{
  faces.clear();
  const Span<int> grid_to_face_map = pbvh.subdiv_ccg->grid_to_face_map;
  int prev_face = -1;
  for (const int prim : node.prim_indices) {
    const int face = grid_to_face_map[prim];
    if (face != prev_face) {
      faces.append(face);
      prev_face = face;
    }
  }
  return faces.as_span();
}

}  // namespace blender::bke::pbvh

blender::Vector<int> BKE_pbvh_node_calc_face_indices(const PBVH &pbvh, const PBVHNode &node)
{
  Vector<int> faces;
  switch (pbvh.header.type) {
    case PBVH_FACES: {
      const Span<int> looptri_faces = pbvh.looptri_faces;
      int prev_face = -1;
      for (const int tri : node.prim_indices) {
        const int face = looptri_faces[tri];
        if (face != prev_face) {
          faces.append(face);
          prev_face = face;
        }
      }
      break;
    }
    case PBVH_GRIDS: {
      const SubdivCCG &subdiv_ccg = *pbvh.subdiv_ccg;
      int prev_face = -1;
      for (const int prim : node.prim_indices) {
        const int face = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, prim);
        if (face != prev_face) {
          faces.append(face);
          prev_face = face;
        }
      }
      break;
    }
    case PBVH_BMESH:
      BLI_assert_unreachable();
      break;
  }

  return faces;
}

void BKE_pbvh_node_num_verts(const PBVH *pbvh,
                             const PBVHNode *node,
                             int *r_uniquevert,
                             int *r_verts_num)
{
  int tot;

  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      tot = node->prim_indices.size() * pbvh->gridkey.grid_area;
      if (r_verts_num) {
        *r_verts_num = tot;
      }
      if (r_uniquevert) {
        *r_uniquevert = tot;
      }
      break;
    case PBVH_FACES:
      if (r_verts_num) {
        *r_verts_num = node->uniq_verts + node->face_verts;
      }
      if (r_uniquevert) {
        *r_uniquevert = node->uniq_verts;
      }
      break;
    case PBVH_BMESH:
      // not a leaf? return zero
      if (!(node->flag & PBVH_Leaf)) {
        if (r_verts_num) {
          *r_verts_num = 0;
        }

        if (r_uniquevert) {
          *r_uniquevert = 0;
        }

        return;
      }

      tot = node->bm_unique_verts->size();
      if (r_verts_num) {
        *r_verts_num = tot + node->bm_other_verts->size();
      }
      if (r_uniquevert) {
        *r_uniquevert = tot;
      }
      break;
  }
}

int BKE_pbvh_node_num_unique_verts(const PBVH &pbvh, const PBVHNode &node)
{
  switch (pbvh.header.type) {
    case PBVH_GRIDS:
      return node.prim_indices.size() * pbvh.gridkey.grid_area;
    case PBVH_FACES:
      return node.uniq_verts;
    case PBVH_BMESH:
      return node.bm_unique_verts->size();
  }
  BLI_assert_unreachable();
  return 0;
}

Span<int> BKE_pbvh_node_get_grid_indices(const PBVHNode &node)
{
  return node.prim_indices;
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

Bounds<float3> BKE_pbvh_node_get_BB(const PBVHNode *node)
{
  return node->vb;
}

Bounds<float3> BKE_pbvh_node_get_original_BB(const PBVHNode *node)
{

  return node->orig_vb;
}

blender::MutableSpan<PBVHProxyNode> BKE_pbvh_node_get_proxies(PBVHNode *node)
{
  return node->proxies;
}

/********************************* Ray-cast ***********************************/

typedef struct {
  struct IsectRayAABB_Precalc ray;
  bool original;
  int stroke_id;
} RaycastData;

static bool ray_aabb_intersect(PBVHNode &node, const RaycastData &rcd)
{
  if (rcd.original) {
    return isect_ray_aabb_v3(&rcd.ray, node.orig_vb.min, node.orig_vb.max, &node.tmin);
  }
  return isect_ray_aabb_v3(&rcd.ray, node.vb.min, node.vb.max, &node.tmin);
}

namespace blender::bke::pbvh {
void raycast(PBVH *pbvh,
             const FunctionRef<void(PBVHNode &node, float *tmin)> hit_fn,
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

  search_callback_occluded(
      pbvh, [&](PBVHNode &node) { return ray_aabb_intersect(node, rcd); }, hit_fn);
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

static bool ray_update_depth_and_hit_count(const float depth_test, float *r_depth, int *hit_count)
{
  (*hit_count)++;
  if (depth_test < *r_depth) {
    *r_depth = depth_test;
    return true;
  }

  return false;
}

static bool ray_face_intersection_depth_quad(const float ray_start[3],
                                             struct IsectRayPrecalc *isect_precalc,
                                             const float t0[3],
                                             const float t1[3],
                                             const float t2[3],
                                             const float t3[3],
                                             float *r_depth,
                                             int *hit_count)
{
  float depth_test;
  if (!(isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, nullptr) ||
        isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t2, t3, &depth_test, nullptr)))
  {
    return false;
  }
  return ray_update_depth_and_hit_count(depth_test, r_depth, hit_count);
}

bool ray_face_intersection_depth_tri(const float ray_start[3],
                                     struct IsectRayPrecalc *isect_precalc,
                                     const float t0[3],
                                     const float t1[3],
                                     const float t2[3],
                                     float *r_depth,
                                     int *hit_count)
{
  float depth_test;

  if (!isect_ray_tri_watertight_v3(ray_start, isect_precalc, t0, t1, t2, &depth_test, nullptr)) {
    return false;
  }
  return ray_update_depth_and_hit_count(depth_test, r_depth, hit_count);
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
                                    const Span<int> corner_verts,
                                    const Span<bool> hide_poly,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    PBVHVertRef *r_active_vertex,
                                    PBVHFaceRef *r_active_face_index,
                                    float *r_face_normal)
{
  using namespace blender;
  const Span<float3> positions = pbvh->vert_positions;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  for (const int i : node->prim_indices.index_range()) {
    const int tri_i = node->prim_indices[i];
    const int3 &tri = pbvh->corner_tris[tri_i];
    const int3 face_verts = node->face_vert_indices[i];

    if (!hide_poly.is_empty() && hide_poly[pbvh->corner_tri_faces[tri_i]]) {
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
      co[0] = positions[corner_verts[tri[0]]];
      co[1] = positions[corner_verts[tri[1]]];
      co[2] = positions[corner_verts[tri[2]]];
    }

    if (pbvh::ray_face_intersection_tri(ray_start, isect_precalc, co[0], co[1], co[2], depth)) {
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
              len_squared_v3v3(location, co[j]) < len_squared_v3v3(location, nearest_vertex_co))
          {
            copy_v3_v3(nearest_vertex_co, co[j]);
            r_active_vertex->i = corner_verts[tri[j]];
            r_active_face_index->i = pbvh->corner_tri_faces[tri_i];
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
                                    PBVHFaceRef *r_active_grid_index,
                                    float *r_face_normal)
{
  const int totgrid = node->prim_indices.size();
  const int gridsize = pbvh->gridkey.grid_size;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0};
  const CCGKey *gridkey = &pbvh->gridkey;
  const BitGroupVector<> &grid_hidden = pbvh->subdiv_ccg->grid_hidden;
  const Span<CCGElem *> grids = pbvh->subdiv_ccg->grids;

  for (int i = 0; i < totgrid; i++) {
    const int grid_index = node->prim_indices[i];
    CCGElem *grid = grids[grid_index];
    if (!grid) {
      continue;
    }

    for (int y = 0; y < gridsize - 1; y++) {
      for (int x = 0; x < gridsize - 1; x++) {
        /* check if grid face is hidden */
        if (!grid_hidden.is_empty()) {
          if (paint_is_grid_face_hidden(grid_hidden[grid_index], gridsize, x, y)) {
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
                ray_start, isect_precalc, co[0], co[1], co[2], co[3], depth))
        {
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
                                len_squared_v3v3(location, nearest_vertex_co))
              {
                copy_v3_v3(nearest_vertex_co, co[j]);

                r_active_vertex->i = gridkey->grid_area * grid_index +
                                     (y + y_it[j]) * gridkey->grid_size + (x + x_it[j]);
              }
            }
          }
          if (r_active_grid_index) {
            r_active_grid_index->i = grid_index;
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

bool raycast_node(SculptSession *ss,
                  PBVH *pbvh,
                  PBVHNode *node,
                  float (*origco)[3],
                  bool use_origco,
                  const Span<int> corner_verts,
                  const Span<bool> hide_poly,
                  const float ray_start[3],
                  const float ray_normal[3],
                  struct IsectRayPrecalc *isect_precalc,
                  int *hit_count,
                  float *depth,
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
                                     corner_verts,
                                     hide_poly,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     depth,
                                     active_vertex,
                                     active_face_grid,
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
                                     active_face_grid,
                                     face_normal);
      break;
    case PBVH_BMESH:
      hit = pbvh_bmesh_node_raycast(ss,
                                    pbvh,
                                    node,
                                    ray_start,
                                    ray_normal,
                                    isect_precalc,
                                    hit_count,
                                    depth,
                                    use_origco,
                                    active_vertex,
                                    active_face_grid,
                                    face_normal,
                                    stroke_id);
      break;
  }

  return hit;
}

void clip_ray_ortho(
    PBVH *pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (pbvh->nodes.is_empty()) {
    return;
  }
  float rootmin_start, rootmin_end;
  float3 bb_min_root, bb_max_root, bb_center, bb_diff;
  IsectRayAABB_Precalc ray;
  float ray_normal_inv[3];
  float offset = 1.0f + 1e-3f;
  const float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

  Bounds<float3> bb;

  if (original) {
    bb = BKE_pbvh_node_get_original_BB(&pbvh->nodes.first());
  }
  else {
    bb = BKE_pbvh_node_get_BB(&pbvh->nodes.first());
  }

  bb_min_root = bb.min;
  bb_max_root = bb.max;

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
   * (due to a plane mesh for instance), or faces very close to the bounding box
   * boundary. */
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
}  // namespace blender::bke::pbvh

/* -------------------------------------------------------------------- */

static bool nearest_to_ray_aabb_dist_sq(PBVHNode *node,
                                        const DistRayAABB_Precalc &dist_ray_to_aabb_precalc,
                                        const bool original)
{
  const float *bb_min, *bb_max;

  if (original) {
    /* BKE_pbvh_node_get_original_BB */
    bb_min = node->orig_vb.min;
    bb_max = node->orig_vb.max;
  }
  else {
    /* BKE_pbvh_node_get_BB */
    bb_min = node->vb.min;
    bb_max = node->vb.max;
  }

  float co_dummy[3], depth;
  node->tmin = dist_squared_ray_to_aabb_v3(
      &dist_ray_to_aabb_precalc, bb_min, bb_max, co_dummy, &depth);
  /* Ideally we would skip distances outside the range. */
  return depth > 0.0f;
}

namespace blender::bke::pbvh {
void find_nearest_to_ray(PBVH *pbvh,
                         const FunctionRef<void(PBVHNode &node, float *tmin)> fn,
                         const float ray_start[3],
                         const float ray_normal[3],
                         const bool original)
{
  const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(ray_start,
                                                                                   ray_normal);

  search_callback_occluded(
      pbvh,
      [&](PBVHNode &node) {
        return nearest_to_ray_aabb_dist_sq(&node, ray_dist_precalc, original);
      },
      fn);
}
}  // namespace blender::bke::pbvh

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
    const int tri_i = node->prim_indices[i];
    const int3 tri = pbvh->corner_tris[tri_i];
    const int3 face_verts = node->face_vert_indices[i];

    if (pbvh->hide_poly && pbvh->hide_poly[pbvh->looptri_faces[tri_i]]) {
      continue;
    }

    if (origco) {
      /* Intersect with backed-up original coordinates. */
      hit |= pbvh::ray_face_nearest_tri(ray_start,
                                        ray_normal,
                                        origco[face_verts[0]],
                                        origco[face_verts[1]],
                                        origco[face_verts[2]],
                                        depth,
                                        dist_sq);
    }
    else {
      /* intersect with current coordinates */
      hit |= pbvh::ray_face_nearest_tri(ray_start,
                                        ray_normal,
                                        positions[corner_verts[tri[0]]],
                                        positions[corner_verts[tri[1]]],
                                        positions[corner_verts[tri[2]]],
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
  const BitGroupVector<> &grid_hidden = pbvh->subdiv_ccg->grid_hidden;
  const Span<CCGElem *> grids = pbvh->subdiv_ccg->grids;

  for (int i = 0; i < totgrid; i++) {
    CCGElem *grid = grids[node->prim_indices[i]];
    if (!grid) {
      continue;
    }

    for (int y = 0; y < gridsize - 1; y++) {
      for (int x = 0; x < gridsize - 1; x++) {
        /* check if grid face is hidden */
        if (!grid_hidden.is_empty()) {
          if (paint_is_grid_face_hidden(grid_hidden[node->prim_indices[i]], gridsize, x, y)) {
            continue;
          }
        }

        if (origco) {
          hit |= pbvh::ray_face_nearest_quad(ray_start,
                                             ray_normal,
                                             origco[y * gridsize + x],
                                             origco[y * gridsize + x + 1],
                                             origco[(y + 1) * gridsize + x + 1],
                                             origco[(y + 1) * gridsize + x],
                                             depth,
                                             dist_sq);
        }
        else {
          hit |= pbvh::ray_face_nearest_quad(ray_start,
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

namespace blender::bke::pbvh {
bool find_nearest_to_ray_node(SculptSession *ss,
                              PBVH *pbvh,
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
          ss, pbvh, node, ray_start, ray_normal, depth, dist_sq, use_origco, stroke_id);
      break;
  }

  return hit;
}
}  // namespace blender::bke::pbvh

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

bool BKE_pbvh_node_frustum_contain_AABB(PBVHNode *node, PBVHFrustumPlanes *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.min;
  bb_max = node->vb.max;

  return test_frustum_aabb(bb_min, bb_max, data) != ISECT_OUTSIDE;
}

bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, PBVHFrustumPlanes *data)
{
  const float *bb_min, *bb_max;
  /* BKE_pbvh_node_get_BB */
  bb_min = node->vb.min;
  bb_max = node->vb.max;

  return test_frustum_aabb(bb_min, bb_max, data) != ISECT_INSIDE;
}

namespace blender::bke::pbvh {
void update_normals(PBVH &pbvh, struct SubdivCCG *subdiv_ccg)
{
  /* Update normals */

  if (pbvh.header.type == PBVH_BMESH) {
    for (int i = 0; i < pbvh.nodes.size(); i++) {
      if (pbvh.nodes[i].flag & PBVH_Leaf) {
        BKE_pbvh_bmesh_check_tris(&pbvh, &pbvh.nodes[i]);
      }
    }
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      &pbvh, [&](PBVHNode &node) { return update_search(&node, PBVH_UpdateNormals); });

  if (pbvh.header.type == PBVH_BMESH) {
    pbvh_bmesh_normals_update(&pbvh, nodes);
  }
  else if (pbvh.header.type == PBVH_FACES) {
    update_normals_faces(pbvh, nodes, *pbvh.mesh);
  }
  else if (pbvh.header.type == PBVH_GRIDS) {
    IndexMaskMemory memory;
    const IndexMask faces_to_update = blender::bke::pbvh::nodes_to_face_selection_grids(
        *subdiv_ccg, nodes, memory);
    BKE_subdiv_ccg_update_normals(*subdiv_ccg, faces_to_update);
    for (PBVHNode *node : nodes) {
      node->flag &= ~PBVH_UpdateNormals;
    }
  }
}
}  // namespace blender::bke::pbvh

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

static bool pbvh_draw_search(PBVHNode *node, PBVHDrawSearchData *data)
{
  if (data->frustum && !BKE_pbvh_node_frustum_contain_AABB(node, data->frustum)) {
    return false;
  }

  data->accum_update_flag |= node->flag;
  return true;
}

namespace blender::bke::pbvh {
void draw_cb(const Mesh &mesh,
             PBVH *pbvh,
             bool update_only_visible,
             PBVHFrustumPlanes &update_frustum,
             PBVHFrustumPlanes &draw_frustum,
             const FunctionRef<void(draw::pbvh::PBVHBatches *batches,
                                    const draw::pbvh::PBVH_GPU_Args &args)> draw_fn)
{
  Vector<PBVHNode *> nodes;
  int update_flag = 0;

  pbvh->draw_cache_invalid = false;

  /* Search for nodes that need updates. */
  if (update_only_visible) {
    /* Get visible nodes with draw updates. */
    PBVHDrawSearchData data = {};
    data.frustum = &update_frustum;
    data.accum_update_flag = 0;
    nodes = blender::bke::pbvh::search_gather(
        pbvh, [&](PBVHNode &node) { return pbvh_draw_search(&node, &data); });
    update_flag = data.accum_update_flag;
  }
  else {
    /* Get all nodes with draw updates, also those outside the view. */
    const int search_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
    nodes = blender::bke::pbvh::search_gather(
        pbvh, [&](PBVHNode &node) { return update_search(&node, search_flag); });
    update_flag = PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers;
  }

  /* Update draw buffers. */
  if (!nodes.is_empty() && (update_flag & (PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers))) {
    pbvh_update_draw_buffers(mesh, pbvh, nodes, update_flag);
  }

  /* Draw visible nodes. */
  PBVHDrawSearchData draw_data = {};
  draw_data.frustum = &draw_frustum;
  draw_data.accum_update_flag = 0;
  nodes = blender::bke::pbvh::search_gather(
      pbvh, [&](PBVHNode &node) { return pbvh_draw_search(&node, &draw_data); });

  for (PBVHNode *node : nodes) {
    if (!(node->flag & PBVH_FullyHidden)) {
      PBVH_GPU_Args args = pbvh_draw_args_init(mesh, *pbvh, *node);
      draw_fn(node->draw_batches, args);
    }
  }
}
}  // namespace blender::bke::pbvh

ATTR_NO_OPT void BKE_pbvh_draw_debug_cb(PBVH *pbvh,
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

    draw_fn(&node, user_data, node.vb.min, node.vb.max, node.flag);
  }
}

void BKE_pbvh_grids_update(PBVH *pbvh, const CCGKey *key)
{
  pbvh->gridkey = *key;
  if (pbvh->subdiv_ccg) {
    pbvh->grid_hidden = &pbvh->subdiv_ccg->grid_hidden;

    pbvh->grids = pbvh->subdiv_ccg->grids.data();
    pbvh->grid_to_face_map = pbvh->subdiv_ccg->grid_to_face_map;
  }
}

void BKE_pbvh_vert_coords_apply(PBVH *pbvh, const Span<float3> vert_positions)
{
  BLI_assert(vert_positions.size() == pbvh->verts_num);

  if (!pbvh->deformed) {
    if (!pbvh->vert_positions.is_empty()) {
      /* When the PBVH is deformed, it creates a separate vertex position array that it
       * owns directly. Conceptually these copies often aren't and often adds extra
       * indirection, but:
       *  - Sculpting shape keys, the deformations are flushed back to the keys as a
       * separate step.
       *  - Sculpting on a deformed mesh, deformations are also flushed to original
       * positions separately.
       *  - The PBVH currently always assumes we want to change positions, and has no
       * way to
       * avoid calculating normals if it's only used for painting, for example. */
      pbvh->vert_positions_deformed = pbvh->vert_positions.as_span();
      pbvh->vert_positions = pbvh->vert_positions_deformed;

      pbvh->vert_normals_deformed = pbvh->vert_normals;
      pbvh->vert_normals = pbvh->vert_normals_deformed;

      pbvh->face_normals_deformed = pbvh->face_normals;
      pbvh->face_normals = pbvh->face_normals_deformed;

      pbvh->deformed = true;
    }
  }

  if (!pbvh->vert_positions.is_empty()) {
    MutableSpan<float3> positions = pbvh->vert_positions;
    /* copy new verts coords */
    for (int a = 0; a < pbvh->totvert; a++) {
      /* no need for float comparison here (memory is exactly equal or not) */
      if (memcmp(positions[a], vert_positions[a], sizeof(float[3])) != 0) {
        positions[a] = vert_positions[a];
      }
    }

    for (PBVHNode &node : pbvh->nodes) {
      BKE_pbvh_node_mark_update(&node);
    }

    blender::bke::pbvh::update_bounds(*pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
  }
}

bool BKE_pbvh_is_deformed(PBVH *pbvh)
{
  return pbvh->deformed;
}
/* Proxies */

PBVHProxyNode &BKE_pbvh_node_add_proxy(PBVH &pbvh, PBVHNode &node)
{
  node.proxies.append_as(PBVHProxyNode{});

  /* It is fine to access pointer of the back element, since node is never handled from
   * multiple threads, and the brush handler only requests a single proxy from the
   * node, and never holds pointers to multiple proxies. */
  PBVHProxyNode &proxy_node = node.proxies.last();

  const int num_unique_verts = BKE_pbvh_node_num_unique_verts(pbvh, node);

  /* Brushes expect proxies to be zero-initialized, so that they can do additive
   * operation to them.
   */
  proxy_node.co.resize(num_unique_verts, float3(0, 0, 0));

  return proxy_node;
}

void BKE_pbvh_node_free_proxies(PBVHNode *node)
{
  node->proxies.clear_and_shrink();
}

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode)
{
  CCGElem **grids;
  const int *grid_indices;
  int totgrid, gridsize, uniq_verts, verts_num;

  vi->grid = nullptr;
  vi->no = nullptr;
  vi->fno = nullptr;
  vi->vert_positions = {};
  vi->vertex.i = 0LL;
  vi->index = 0;

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, &gridsize, &grids);
  BKE_pbvh_node_num_verts(pbvh, node, &uniq_verts, &verts_num);
  vi->key = pbvh->gridkey;

  vi->grids = grids;
  vi->grid_indices = grid_indices;
  vi->totgrid = (grids) ? totgrid : 1;
  vi->gridsize = gridsize;

  if (mode == PBVH_ITER_ALL) {
    vi->totvert = verts_num;
  }
  else {
    vi->totvert = uniq_verts;
  }
  vi->vert_indices = node->vert_indices.data();
  vi->vert_positions = pbvh->vert_positions;
  vi->is_mesh = !pbvh->vert_positions.is_empty();

  if (pbvh->header.type == PBVH_BMESH) {
    if (mode == PBVH_ITER_ALL) {
      pbvh_bmesh_check_other_verts(node);
    }

    vi->vert_positions = {};

    vi->bi = 0;
    vi->bm_cur_set = 0;
    vi->bm_unique_verts = node->bm_unique_verts;
    vi->bm_other_verts = node->bm_other_verts;
    vi->bm_iter = node->bm_unique_verts->begin();
    vi->bm_iter_end = node->bm_unique_verts->end();

    vi->bm_vdata = &pbvh->header.bm->vdata;
    vi->bm_vert = nullptr;

    vi->cd_vert_mask_offset = CustomData_get_offset_named(
        vi->bm_vdata, CD_PROP_FLOAT, ".sculpt_mask");
  }

  vi->gh = BoundedBitSpan();

  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = pbvh->grid_hidden;
  }

  vi->mask = 0.0f;
  if (pbvh->header.type == PBVH_FACES) {
    vi->vert_normals = pbvh->vert_normals;
    vi->hide_vert = pbvh->hide_vert;

    vi->vmask = static_cast<const float *>(
        CustomData_get_layer_named(pbvh->vert_data, CD_PROP_FLOAT, ".sculpt_mask"));
  }
}

bool pbvh_has_mask(const PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
      return (pbvh->gridkey.has_mask != 0);
    case PBVH_FACES:
      return pbvh->mesh->attributes().contains(".sculpt_mask");
    case PBVH_BMESH:
      return pbvh->header.bm &&
             (CustomData_has_layer_named(&pbvh->header.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask"));
  }

  return false;
}

bool pbvh_has_face_sets(PBVH *pbvh)
{
  switch (pbvh->header.type) {
    case PBVH_GRIDS:
    case PBVH_FACES:
      return pbvh->mesh->attributes().contains(".sculpt_face_set");
    case PBVH_BMESH:
      return CustomData_get_offset_named(
                 &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set") != -1;
  }

  return false;
}

namespace blender::bke::pbvh {
void set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes)
{
  pbvh->num_planes = planes->num_planes;
  for (int i = 0; i < pbvh->num_planes; i++) {
    copy_v4_v4(pbvh->planes[i], planes->planes[i]);
  }
}

void get_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes)
{
  planes->num_planes = pbvh->num_planes;
  for (int i = 0; i < planes->num_planes; i++) {
    copy_v4_v4(planes->planes[i], pbvh->planes[i]);
  }
}
}  // namespace blender::bke::pbvh

#include "BKE_global.hh"
void BKE_pbvh_parallel_range_settings(TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode)
{
  memset(settings, 0, sizeof(*settings));
  settings->use_threading = use_threading && totnode > 1 && G.debug_value != 890;
}

Mesh *BKE_pbvh_get_mesh(PBVH *pbvh)
{
  return pbvh->mesh;
}

MutableSpan<float3> BKE_pbvh_get_vert_positions(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return pbvh->vert_positions;
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
      &pbvh->mesh->vert_data, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->verts_num));
  if (pbvh->hide_vert) {
    return pbvh->hide_vert;
  }
  pbvh->hide_vert = static_cast<bool *>(CustomData_add_layer_named(
      &pbvh->mesh->vert_data, CD_PROP_BOOL, CD_SET_DEFAULT, pbvh->mesh->verts_num, ".hide_vert"));
  return pbvh->hide_vert;
}

void BKE_pbvh_subdiv_ccg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  pbvh->subdiv_ccg = subdiv_ccg;
  pbvh->grid_to_face_map = subdiv_ccg->grid_to_face_map;
  pbvh->grid_hidden = &subdiv_ccg->grid_hidden;
  pbvh->grids = subdiv_ccg->grids.data();
}

void BKE_pbvh_update_hide_attributes_from_mesh(PBVH *pbvh)
{
  if (pbvh->header.type == PBVH_FACES) {
    pbvh->hide_vert = static_cast<bool *>(CustomData_get_layer_named_for_write(
        &pbvh->mesh->vert_data, CD_PROP_BOOL, ".hide_vert", pbvh->mesh->verts_num));
    pbvh->hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
        &pbvh->mesh->face_data, CD_PROP_BOOL, ".hide_poly", pbvh->mesh->faces_num));
  }
}

int BKE_pbvh_get_node_index(PBVH *pbvh, PBVHNode *node)
{
  return (int)(node - &pbvh->nodes[0]);
}

int BKE_pbvh_get_totnodes(PBVH *pbvh)
{
  return pbvh->nodes.size();
}

int BKE_pbvh_get_node_id(PBVH * /*pbvh*/, PBVHNode *node)
{
  return node->id;
}

PBVHNode *BKE_pbvh_node_from_index(PBVH *pbvh, int node_i)
{
  return &pbvh->nodes[node_i];
}

PBVHNode *BKE_pbvh_get_node(PBVH *pbvh, int node)
{
  return &pbvh->nodes[node];
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

  for (int i = 0; i < pbvh->nodes.size(); i++) {
    PBVHNode *node = &pbvh->nodes[i];
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

    if (!node->tribuf || !node->tribuf->tris.size()) {
      return;
    }
  }

  node->flag &= ~PBVH_UpdateTriAreas;

  const int cur_i = pbvh->face_area_i ^ 1;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES: {
      for (int i = 0; i < (int)node->prim_indices.size(); i++) {
        const int poly = pbvh->looptri_faces[node->prim_indices[i]];

        if (pbvh->hide_poly && pbvh->hide_poly[poly]) {
          /* Skip hidden faces. */
          continue;
        }

        pbvh->face_areas[poly * 2 + cur_i] = 0.0f;
      }

      for (int i = 0; i < (int)node->prim_indices.size(); i++) {
        const int3 &lt = pbvh->corner_tris[node->prim_indices[i]];
        const int poly = pbvh->looptri_faces[node->prim_indices[i]];

        if (pbvh->hide_poly && pbvh->hide_poly[poly]) {
          /* Skip hidden faces. */
          continue;
        }

        float area = area_tri_v3(pbvh->vert_positions[pbvh->corner_verts[lt[0]]],
                                 pbvh->vert_positions[pbvh->corner_verts[lt[1]]],
                                 pbvh->vert_positions[pbvh->corner_verts[lt[2]]]);

        pbvh->face_areas[poly * 2 + cur_i] += area;

        /* sanity check on read side of read write buffer */
        if (pbvh->face_areas[poly * 2 + (cur_i ^ 1)] == 0.0f) {
          pbvh->face_areas[poly * 2 + (cur_i ^ 1)] = pbvh->face_areas[poly * 2 + cur_i];
        }
      }
      break;
    }
    case PBVH_GRIDS:
      break;
    case PBVH_BMESH: {
      const int cd_face_area = pbvh->cd_face_area;

      for (BMFace *f : *node->bm_faces) {
        float *areabuf = (float *)BM_ELEM_CD_GET_VOID_P(f, cd_face_area);
        areabuf[cur_i] = 0.0f;
      }

      for (PBVHTri &tri : node->tribuf->tris) {
        BMVert *v1 = (BMVert *)(node->tribuf->verts[tri.v[0]].i);
        BMVert *v2 = (BMVert *)(node->tribuf->verts[tri.v[1]].i);
        BMVert *v3 = (BMVert *)(node->tribuf->verts[tri.v[2]].i);
        BMFace *f = (BMFace *)tri.f.i;

        float *areabuf = (float *)BM_ELEM_CD_GET_VOID_P(f, cd_face_area);
        areabuf[cur_i] += area_tri_v3(v1->co, v2->co, v3->co);
      }

      for (BMFace *f : *node->bm_faces) {
        float *areabuf = (float *)BM_ELEM_CD_GET_VOID_P(f, cd_face_area);

        /* sanity check on read side of read write buffer */
        if (areabuf[cur_i ^ 1] == 0.0f) {
          areabuf[cur_i ^ 1] = areabuf[cur_i];
        }
      }

      break;
    }
    default:
      break;
  }
}

static void pbvh_pmap_to_edges_add(PBVH * /*pbvh*/,
                                   PBVHVertRef /*vertex*/,
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

    int *r_edges_new = (int *)MEM_malloc_arrayN(newsize, sizeof(*r_edges_new), "r_edges_new");
    int *r_polys_new = (int *)MEM_malloc_arrayN(newsize * 2, sizeof(*r_polys_new), "r_polys_new");

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
  Span<int> map = pbvh->vert_to_face_map[vertex.i];
  int len = 0;

  for (int i : IndexRange(map.index_range())) {
    int loopstart = pbvh->faces[map[i]].start();
    int loop_count = pbvh->faces[map[i]].size();

    const Span<int> corner_verts(&pbvh->corner_verts[loopstart], loop_count);
    const Span<int> corner_edges(&pbvh->corner_edges[loopstart], loop_count);

    if (pbvh->hide_poly && pbvh->hide_poly[map[i]]) {
      /* Skip connectivity from hidden faces. */
      continue;
    }

    for (int j : IndexRange(loop_count)) {
      if (corner_verts[j] == vertex.i) {
        pbvh_pmap_to_edges_add(pbvh,
                               vertex,
                               r_edges,
                               r_edges_size,
                               r_heap_alloc,
                               corner_edges[(j + loop_count - 1) % loop_count],
                               map[i],
                               &len,
                               r_polys);
        pbvh_pmap_to_edges_add(pbvh,
                               vertex,
                               r_edges,
                               r_edges_size,
                               r_heap_alloc,
                               corner_edges[j],
                               map[i],
                               &len,
                               r_polys);
      }
    }
  }

  *r_edges_size = len;
}

void BKE_pbvh_get_vert_face_areas(PBVH *pbvh, PBVHVertRef vertex, float *r_areas, int valence)
{
  const int cur_i = pbvh->face_area_i;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES: {
      int *edges = (int *)BLI_array_alloca(edges, 16);
      int *faces = (int *)BLI_array_alloca(faces, 32);
      bool heap_alloc = false;
      int len = 16;

      BKE_pbvh_pmap_to_edges(pbvh, vertex, &edges, &len, &heap_alloc, &faces);
      len = std::min(len, valence);

      if (!pbvh->vert_to_edge_map.is_empty()) {
        /* sort face references by vemap edge ordering */
        Span<int> emap = pbvh->vert_to_edge_map[vertex.i];

        int *faces_old = (int *)BLI_array_alloca(faces, len * 2);
        memcpy((void *)faces_old, (void *)faces, sizeof(int) * len * 2);

        /* note that wire edges will break this, but
           should only result in incorrect weights
           and isn't worth fixing */

        for (int i = 0; i < len; i++) {
          for (int j = 0; j < len; j++) {
            if (emap[i] == edges[j]) {
              faces[i * 2] = faces_old[j * 2];
              faces[i * 2 + 1] = faces_old[j * 2 + 1];
            }
          }
        }
      }
      for (int i = 0; i < len; i++) {
        r_areas[i] = pbvh->face_areas[faces[i * 2] * 2 + cur_i];

        if (faces[i * 2 + 1] != -1) {
          r_areas[i] += pbvh->face_areas[faces[i * 2 + 1] * 2 + cur_i];
          r_areas[i] *= 0.5f;
        }
      }

      if (heap_alloc) {
        MEM_freeN(edges);
        MEM_freeN(faces);
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
        BMVert *v2 = BM_edge_other_vert(e, v);

        if (*BM_ELEM_CD_PTR<uint8_t *>(v2, pbvh->cd_flag) & SCULPTFLAG_VERT_FSET_HIDDEN) {
          continue;
        }

        if (!e->l) {
          w = 0.0f;
        }
        else {
          float *a1 = (float *)BM_ELEM_CD_GET_VOID_P(e->l->f, cd_face_area);
          float *a2 = (float *)BM_ELEM_CD_GET_VOID_P(e->l->radial_next->f, cd_face_area);

          w += a1[cur_i] * 0.5f;
          w += a2[cur_i] * 0.5f;
        }

        if (j >= valence) {
          printf("%s: error, corrupt edge cycle, valence was %d expected %d\n",
                 __func__,
                 j + 1,
                 valence);
          uint8_t *flags = BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag);
          *flags |= SCULPTFLAG_NEED_VALENCE;
          break;
        }

        r_areas[j++] = w;
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

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

      SubdivCCGCoord coord = {};

      coord.grid_index = grid_index;
      coord.x = short(vertex_index % key->grid_size);
      coord.y = short(vertex_index / key->grid_size);

      SubdivCCGNeighbors neighbors;
      BKE_subdiv_ccg_neighbor_coords_get(*pbvh->subdiv_ccg, coord, false, neighbors);

      float *co1 = CCG_elem_co(key, CCG_elem_offset(key, pbvh->grids[grid_index], vertex_index));
      float totw = 0.0f;
      int i = 0;

      for (i = 0; i < neighbors.size; i++) {
        SubdivCCGCoord *coord2 = neighbors.coords + i;

        int vertex_index2 = int(coord2->y) * key->grid_size + int(coord2->x);

        float *co2 = CCG_elem_co(
            key, CCG_elem_offset(key, pbvh->grids[coord2->grid_index], vertex_index2));
        float w = len_v3v3(co1, co2);

        r_areas[i] = w;
        totw += w;
      }

      if (neighbors.size != valence) {
        printf(
            "%s: error! neighbors.size was %d expected %d\n", __func__, neighbors.size, valence);
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
    int verts_num = pbvh->totvert;

    if (BKE_pbvh_type(pbvh) == PBVH_GRIDS) {
      verts_num = BKE_pbvh_get_grid_num_verts(pbvh);
    }

    for (int i = 0; i < verts_num; i++) {
      pbvh->boundary_flags[i] |= SCULPT_BOUNDARY_NEEDS_UPDATE;
    }
  }
}

void BKE_pbvh_set_symmetry(PBVH *pbvh, int symmetry)
{
  if (symmetry == pbvh->symmetry) {
    return;
  }

  pbvh->symmetry = symmetry;
}

namespace blender::bke::pbvh {

Span<float3> get_poly_normals(const PBVH *pbvh)
{
  BLI_assert(pbvh->header.type == PBVH_FACES);
  return pbvh->face_normals;
}

void on_stroke_start(PBVH *pbvh)
{
  /* Load current node bounds into original bounds at stroke start.*/
  for (int i : IndexRange(pbvh->nodes.size())) {
    PBVHNode *node = &pbvh->nodes[i];

    node->orig_vb = node->vb;
  }
}

void set_vert_boundary_map(PBVH *pbvh, blender::BitVector<> *vert_boundary_map)
{
  pbvh->vert_boundary_map = vert_boundary_map;
}

void update_edge_boundary_grids(int /*edge*/,
                                Span<blender::int2> /*edges*/,
                                OffsetIndices<int> /*polys*/,
                                int * /*edge_boundary_flags*/,
                                const int * /*vert_boundary_flags*/,
                                const int * /*face_sets*/,
                                const bool * /*sharp_edge*/,
                                const bool * /*seam_edge*/,
                                const GroupedSpan<int> & /*pmap*/,
                                const GroupedSpan<int> & /*epmap*/,
                                const CustomData * /*ldata*/,
                                SubdivCCG * /*subdiv_ccg*/,
                                const CCGKey * /*key*/,
                                float /*sharp_angle_limit*/,
                                blender::Span<int> /*corner_verts*/,
                                blender::Span<int> /*corner_edges*/)
{
  //
}

static void get_edge_polys(int edge,
                           const GroupedSpan<int> &pmap,
                           const GroupedSpan<int> &epmap,
                           Span<blender::int2> edges,
                           OffsetIndices<int> polys,
                           Span<int> corner_edges,
                           int *r_poly1,
                           int *r_poly2)
{
  *r_poly1 = -1;
  *r_poly2 = -1;

  if (!epmap.is_empty()) {
    Span<int> polys = epmap[edge];

    if (polys.size() > 0) {
      *r_poly1 = polys[0];
    }
    if (polys.size() > 1) {
      *r_poly1 = polys[1];
    }
  }
  else {
    int v1 = edges[edge][0];

    for (int poly : pmap[v1]) {
      for (int loop : polys[poly]) {
        if (corner_edges[loop] == edge) {
          if (*r_poly1 == -1) {
            *r_poly1 = poly;
          }
          else {
            *r_poly2 = poly;
          }
        }
      }
    }
  }
}

void update_edge_boundary_faces(int edge,
                                Span<float3> vertex_positions,
                                Span<float3> /*vertex_normals*/,
                                Span<blender::int2> edges,
                                OffsetIndices<int> polys,
                                Span<float3> poly_normals,
                                int *edge_boundary_flags,
                                const int *vert_boundary_flags,
                                const int *face_sets,
                                const bool *sharp_edge,
                                const bool *seam_edge,
                                const GroupedSpan<int> &pmap,
                                const GroupedSpan<int> &epmap,
                                const CustomData * /*ldata*/,
                                float sharp_angle_limit,
                                blender::Span<int> corner_verts,
                                blender::Span<int> corner_edges)
{
  int oldflag = edge_boundary_flags[edge];
  bool update_uv = oldflag & SCULPT_BOUNDARY_UPDATE_UV;
  bool update_sharp = oldflag & SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
  int newflag = 0;

  if (update_sharp) {
    int poly1 = -1, poly2 = -1;

    edge_boundary_flags[edge] &= ~SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;

    get_edge_polys(edge, pmap, epmap, edges, polys, corner_edges, &poly1, &poly2);
    if (poly1 != -1 && poly2 != -1 &&
        test_sharp_faces_mesh(
            poly1, poly2, sharp_angle_limit, vertex_positions, polys, poly_normals, corner_verts))
    {
      edge_boundary_flags[edge] |= SCULPT_BOUNDARY_SHARP_ANGLE;
    }
    else {
      edge_boundary_flags[edge] &= ~SCULPT_BOUNDARY_SHARP_ANGLE;
    }
  }

  if (!update_uv) {
    newflag |= oldflag & SCULPT_BOUNDARY_UV;
  }

  if (!(oldflag & SCULPT_BOUNDARY_NEEDS_UPDATE)) {
    return;
  }

  /* Some boundary types require an edge->poly map to be fully accurate. */
  if (!epmap.is_empty()) {
    if (face_sets) {
      int fset = -1;

      for (int poly : epmap[edge]) {
        if (fset == -1) {
          fset = face_sets[poly];
        }
        else if (face_sets[poly] != fset) {
          newflag |= SCULPT_BOUNDARY_FACE_SET;
          break;
        }
      }
    }
    newflag |= epmap[edge].size() == 1 ? SCULPT_BOUNDARY_MESH : 0;
  }
  else { /* No edge->poly map; approximate from vertices (will give artifacts on
            corners). */
    int v1 = edges[edge][0];
    int v2 = edges[edge][1];

    int a = vert_boundary_flags[v1] &
            ~(SCULPT_BOUNDARY_UPDATE_UV | SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE);
    int b = vert_boundary_flags[v2] &
            ~(SCULPT_BOUNDARY_UPDATE_UV | SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE);

    newflag |= a & b;
  }

  newflag |= sharp_edge && sharp_edge[edge] ? SCULPT_BOUNDARY_SHARP_MARK : 0;
  newflag |= (seam_edge && seam_edge[edge]) ? SCULPT_BOUNDARY_SEAM : 0;

  edge_boundary_flags[edge] = newflag;
}

void set_flags_valence(PBVH *pbvh, uint8_t *flags, int *valence)
{
  pbvh->sculpt_flags = flags;
  pbvh->valence = valence;
}

void set_original(PBVH *pbvh, Span<float3> origco, Span<float3> origno)
{
  pbvh->origco = origco;
  pbvh->origno = origno;
}

void update_vert_boundary_faces(int *boundary_flags,
                                const int *face_sets,
                                const bool *hide_poly,
                                const int2 * /*medge*/,
                                const int *corner_verts,
                                const int *corner_edges,
                                OffsetIndices<int> polys,
                                const blender::GroupedSpan<int> &pmap,
                                PBVHVertRef vertex,
                                const bool *sharp_edges,
                                const bool *seam_edges,
                                uint8_t *flags,
                                int * /*valence*/)
{
  Span<int> vert_map = pmap[vertex.i];
  uint8_t *flag = flags + vertex.i;

  *flag &= ~SCULPTFLAG_VERT_FSET_HIDDEN;

  int last_fset = -1;
  int last_fset2 = -1;

  int *boundary_flag = boundary_flags + vertex.i;
  *boundary_flag = 0;

  int totsharp = 0, totseam = 0, totsharp_angle = 0;
  int visible = false;

  for (int i : vert_map.index_range()) {
    int f_i = vert_map[i];

    IndexRange poly = polys[f_i];
    const int *mc = corner_verts + poly.start();
    const int loop_count = poly.size();
    const int loopstart = poly.start();

    int j = 0;

    for (j = 0; j < loop_count; j++, mc++) {
      if (*mc == (int)vertex.i) {
        break;
      }
    }

    if (j < loop_count) {
      int e_index = corner_edges[loopstart + j];

      if (sharp_edges && sharp_edges[e_index]) {
        *boundary_flag |= SCULPT_BOUNDARY_SHARP_MARK;
        totsharp++;
      }

      if (seam_edges && seam_edges[e_index]) {
        *boundary_flag |= SCULPT_BOUNDARY_SEAM;
        totseam++;
      }
    }

    int fset = face_sets ? abs(face_sets[f_i]) : 1;

    if (!hide_poly || !hide_poly[f_i]) {
      visible = true;
    }

    if (i > 0 && fset != last_fset) {
      *boundary_flag |= SCULPT_BOUNDARY_FACE_SET;

      if (i > 1 && last_fset2 != last_fset && last_fset != -1 && last_fset2 != -1 && fset != -1 &&
          last_fset2 != fset)
      {
        *boundary_flag |= SCULPT_CORNER_FACE_SET;
      }
    }

    if (i > 0 && last_fset != fset) {
      last_fset2 = last_fset;
    }

    last_fset = fset;
  }

  if (!visible) {
    *flag |= SCULPTFLAG_VERT_FSET_HIDDEN;
  }

  if (totsharp_angle > 2) {
    *boundary_flag |= SCULPT_CORNER_SHARP_ANGLE;
  }

  if (!ELEM(totsharp, 0, 2)) {
    *boundary_flag |= SCULPT_CORNER_SHARP_MARK;
  }

  if (totseam > 2) {
    *boundary_flag |= SCULPT_CORNER_SEAM;
  }
}

static bool check_unique_face_set_in_base_mesh(const PBVH *pbvh,
                                               int vertex,
                                               bool *r_corner,
                                               const int *face_sets)
{
  if (!face_sets) {
    return true;
  }
  int fset1 = -1, fset2 = -1, fset3 = -1;

  for (int poly : pbvh->vert_to_face_map[vertex]) {
    int fset = face_sets[poly];

    if (fset1 == -1) {
      fset1 = fset;
    }
    else if (fset2 == -1 && fset != fset1) {
      fset2 = fset;
    }
    else if (fset3 == -1 && fset != fset1 && fset != fset2) {
      fset3 = fset;
    }
  }

  *r_corner = fset3 != -1;
  return fset2 == -1;
}

static bool check_boundary_vertex_in_base_mesh(const PBVH *pbvh, int vert)
{
  return pbvh->vert_boundary_map ? (*pbvh->vert_boundary_map)[vert] : false;
}

/**
 * Checks if the face sets of the adjacent faces to the edge between \a v1 and \a v2
 * in the base mesh are equal.
 */
static bool check_unique_face_set_for_edge_in_base_mesh(const PBVH *pbvh,
                                                        int v1,
                                                        int v2,
                                                        const int *face_sets)
{
  if (!face_sets) {
    return true;
  }

  int p1 = -1, p2 = -1;
  for (int poly : pbvh->vert_to_face_map[v1]) {
    const IndexRange p = pbvh->faces[poly];

    for (int l = 0; l < p.size(); l++) {
      const int *corner_verts = &pbvh->corner_verts[p.start() + l];
      if (*corner_verts == v2) {
        if (p1 == -1) {
          p1 = poly;
          break;
        }

        if (p2 == -1) {
          p2 = poly;
          break;
        }
      }
    }
  }

  if (p1 != -1 && p2 != -1) {
    return abs(face_sets[p1]) == (face_sets[p2]);
  }
  return true;
}

void update_vert_boundary_grids(PBVH *pbvh, int index, const int *face_sets)
{

  int *flag = pbvh->boundary_flags + index;

  *flag = 0;

  const CCGKey *key = BKE_pbvh_get_grid_key(pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;
  SubdivCCGCoord coord{};

  coord.grid_index = grid_index;
  coord.x = vertex_index % key->grid_size;
  coord.y = vertex_index / key->grid_size;

  int v1, v2;
  const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
      *pbvh->subdiv_ccg, coord, pbvh->corner_verts, pbvh->faces, v1, v2);

  bool fset_corner = false;
  switch (adjacency) {
    case SUBDIV_CCG_ADJACENT_VERTEX:
      if (!check_unique_face_set_in_base_mesh(pbvh, v1, &fset_corner, face_sets)) {
        *flag |= SCULPT_BOUNDARY_FACE_SET;
      }
      if (check_boundary_vertex_in_base_mesh(pbvh, v1)) {
        *flag |= SCULPT_BOUNDARY_MESH;
      }
      break;
    case SUBDIV_CCG_ADJACENT_EDGE: {
      if (!check_unique_face_set_for_edge_in_base_mesh(pbvh, v1, v2, face_sets)) {
        *flag |= SCULPT_BOUNDARY_FACE_SET;
      }

      if (check_boundary_vertex_in_base_mesh(pbvh, v1) &&
          check_boundary_vertex_in_base_mesh(pbvh, v2))
      {
        *flag |= SCULPT_BOUNDARY_MESH;
      }
      break;
    }
    case SUBDIV_CCG_ADJACENT_NONE:
      break;
  }

  if (fset_corner) {
    *flag |= SCULPT_CORNER_FACE_SET | SCULPT_BOUNDARY_FACE_SET;
  }
}

}  // namespace blender::bke::pbvh

void BKE_pbvh_distort_correction_set(PBVH *pbvh, eAttrCorrectMode value)
{
  /* Condition to update UV boundaries.*/
  bool update = !pbvh->distort_correction_mode != !value;
  pbvh->distort_correction_mode = value;

  if (update) {
    pbvh_boundaries_flag_update(pbvh);
  }
}

void BKE_pbvh_set_bmesh(PBVH *pbvh, BMesh *bm)
{
  pbvh->header.bm = bm;
}

BMLog *BKE_pbvh_get_bm_log(PBVH *pbvh)
{
  return pbvh->bm_log;
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

void BKE_pbvh_node_num_loops(PBVH *pbvh, PBVHNode *node, int *r_corners_num)
{
  UNUSED_VARS(pbvh);
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  if (r_corners_num) {
    *r_corners_num = node->loop_indices.size();
  }
}

void BKE_pbvh_update_active_vcol(PBVH *pbvh, Mesh *mesh)
{
  CustomDataLayer *last_layer = pbvh->color_layer;

  BKE_pbvh_get_color_layer(pbvh, mesh, &pbvh->color_layer, &pbvh->color_domain);

  if (pbvh->color_layer) {
    pbvh->cd_vcol_offset = pbvh->color_layer->offset;
  }
  else {
    pbvh->cd_vcol_offset = -1;
  }

  if (pbvh->color_layer != last_layer) {
    for (int i = 0; i < pbvh->nodes.size(); i++) {
      PBVHNode *node = &pbvh->nodes[i];

      if (node->flag & PBVH_Leaf) {
        BKE_pbvh_node_mark_update_color(node);
      }
    }
  }
}

void BKE_pbvh_ensure_node_loops(PBVH *pbvh)
{
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_FACES);

  int corners_num = 0;

  /* Check if nodes already have loop indices. */
  for (PBVHNode &node : pbvh->nodes) {
    if (!(node.flag & PBVH_Leaf)) {
      continue;
    }

    if (!node.loop_indices.is_empty()) {
      return;
    }

    corners_num += node.prim_indices.size() * 3;
  }

  BLI_bitmap *visit = BLI_BITMAP_NEW(corners_num, __func__);

  /* Create loop indices from node loop triangles. */
  Vector<int> loop_indices;
  for (PBVHNode &node : pbvh->nodes) {
    if (!(node.flag & PBVH_Leaf)) {
      continue;
    }

    loop_indices.clear();

    for (const int i : node.prim_indices) {
      const int3 &mlt = pbvh->corner_tris[i];

      for (int k = 0; k < 3; k++) {
        if (!BLI_BITMAP_TEST(visit, mlt[k])) {
          loop_indices.append(mlt[k]);
          BLI_BITMAP_ENABLE(visit, mlt[k]);
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

    fd->verts = (PBVHVertRef *)MEM_malloc_arrayN(fd->verts_size_, sizeof(void *), __func__);
  }

  fd->verts_num = verts_num;
}

BLI_INLINE int face_iter_prim_to_face(PBVHFaceIter *fd, int prim_index)
{
  if (fd->subdiv_ccg_) {
    return BKE_subdiv_ccg_grid_to_face_index(*fd->subdiv_ccg_, prim_index);
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
        ++fd->bm_iter_;
      }

      if (fd->bm_iter_ == fd->bm_iter_end_) {
        return;
      }

      BMFace *f = *fd->bm_iter_;
      fd->face.i = (intptr_t)f;
      fd->index = f->head.index;

      if (fd->cd_face_set_ != -1) {
        fd->face_set = (int *)BM_ELEM_CD_GET_VOID_P(f, fd->cd_face_set_);
      }

      /* TODO: BMesh doesn't use .hide_poly yet.*/
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
      fd->face_sets_ = static_cast<int *>(CustomData_get_layer_named_for_write(
          pbvh->face_data, CD_PROP_INT32, ".sculpt_face_set", pbvh->faces_num));
      fd->last_face_index_ = -1;

      break;
    case PBVH_BMESH:
      fd->bm = pbvh->header.bm;
      fd->cd_face_set_ = CustomData_get_offset_named(
          &pbvh->header.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

      fd->bm_iter_ = node->bm_faces->begin();
      fd->bm_iter_end_ = node->bm_faces->end();
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
      return fd->bm_iter_ == fd->bm_iter_end_;
    default:
      BLI_assert_unreachable();
      return true;
  }
}

void BKE_pbvh_sync_visibility_from_verts(PBVH *pbvh, Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  switch (pbvh->header.type) {
    case PBVH_FACES: {
      mesh_hide_vert_flush(*mesh);
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
      const OffsetIndices faces = mesh->faces();
      CCGKey key = pbvh->gridkey;

      IndexMaskMemory memory;
      const IndexMask hidden_faces = IndexMask::from_predicate(
          faces.index_range(), GrainSize(1024), memory, [&](const int i) {
            const IndexRange face = faces[i];
            return std::any_of(face.begin(), face.end(), [&](const int corner) {
              if ((*pbvh->grid_hidden)[corner].is_empty()) {
                return false;
              }
              return (*pbvh->grid_hidden)[corner][key.grid_area - 1].test();
            });
          });

      MutableAttributeAccessor attributes = mesh->attributes_for_write();
      if (hidden_faces.is_empty()) {
        attributes.remove(".hide_poly");
      }
      else {
        SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_span<bool>(
            ".hide_poly", AttrDomain::Face, AttributeInitConstruct());
        hide_poly.span.fill(false);
        index_mask::masked_fill(hide_poly.span, true, hidden_faces);
        hide_poly.finish();
      }

      mesh_hide_face_flush(*mesh);
      BKE_pbvh_update_hide_attributes_from_mesh(pbvh);
      break;
    }
  }
}

void BKE_pbvh_flush_tri_areas(PBVH *pbvh)
{
  for (int i : IndexRange(pbvh->nodes.size())) {
    PBVHNode *node = &pbvh->nodes[i];

    if (!(node->flag & PBVH_Leaf) || !(node->flag & PBVH_UpdateTriAreas)) {
      continue;
    }

    BKE_pbvh_check_tri_areas(pbvh, node);
    node->flag |= PBVH_UpdateTriAreas;
  }

  BKE_pbvh_face_areas_begin(pbvh);

  for (int i : IndexRange(pbvh->nodes.size())) {
    PBVHNode *node = &pbvh->nodes[i];

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    BKE_pbvh_check_tri_areas(pbvh, node);
  }
}

namespace blender::bke::pbvh {
Vector<PBVHNode *> search_gather(PBVH *pbvh,
                                 const FunctionRef<bool(PBVHNode &)> scb,
                                 PBVHNodeFlags leaf_flag)
{
  if (pbvh->nodes.is_empty()) {
    return {};
  }

  PBVHIter iter;
  Vector<PBVHNode *> nodes;

  pbvh_iter_begin(&iter, pbvh, scb);

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
    if (!node.proxies.is_empty()) {
      array.append(&node);
    }
  }

  return array;
}

Vector<PBVHNode *> get_flagged_nodes(PBVH *pbvh, int flag)
{
  return blender::bke::pbvh::search_gather(
      pbvh, [&](PBVHNode &node) { return update_search(&node, flag); });
}

struct GroupedSpan<int> get_pmap(PBVH *pbvh) {
  return pbvh->vert_to_face_map;
}

  void
  set_pmap(PBVH *pbvh, GroupedSpan<int> pmap)
{
  pbvh->vert_to_face_map = pmap;
}

void set_vemap(PBVH *pbvh, GroupedSpan<int> vemap)
{
  pbvh->vert_to_edge_map = vemap;
}

static bool test_colinear_tri(int f,
                              Span<float3> positions,
                              blender::OffsetIndices<int> polys,
                              Span<int> corner_verts)
{
  Span<int> verts = corner_verts.slice(polys[f]);

  float area_limit = 0.00001f;
  area_limit = len_squared_v3v3(positions[verts[0]], positions[verts[1]]) * 0.001f;

  return area_tri_v3(positions[verts[0]], positions[verts[1]], positions[verts[2]]) <= area_limit;
}

float test_sharp_faces_mesh(int f1,
                            int f2,
                            float limit,
                            Span<float3> positions,
                            blender::OffsetIndices<int> &polys,
                            Span<float3> poly_normals,
                            Span<int> corner_verts)
{
  float angle = math::safe_acos(dot_v3v3(poly_normals[f1], poly_normals[f2]));

  /* Detect coincident triangles. */
  if (polys[f1].size() == 3 && test_colinear_tri(f1, positions, polys, corner_verts)) {
    return false;
  }
  if (polys[f2].size() == 3 && test_colinear_tri(f2, positions, polys, corner_verts)) {
    return false;
  }

  /* Try to ignore folded over edges. */
  if (angle > M_PI * 0.6) {
    return false;
  }

  return angle > limit;
}

bool check_vert_boundary(PBVH *pbvh, PBVHVertRef vertex, const int *face_sets)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH: {
      if (pbvh->cd_boundary_flag == -1) {
        return false;
      }

      return pbvh_check_vert_boundary_bmesh(pbvh, reinterpret_cast<BMVert *>(vertex.i));
    }
    case PBVH_FACES: {
      if (!pbvh->boundary_flags) {
        return false;
      }
      if (pbvh->boundary_flags[vertex.i] &
          (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV))
      {
        update_vert_boundary_faces(pbvh->boundary_flags,
                                   face_sets,
                                   pbvh->hide_poly,
                                   nullptr,
                                   pbvh->corner_verts.data(),
                                   pbvh->corner_edges.data(),
                                   pbvh->faces,
                                   pbvh->vert_to_face_map,
                                   vertex,
                                   pbvh->sharp_edges,
                                   pbvh->seam_edges,
                                   pbvh->sculpt_flags,
                                   pbvh->valence);
        return true;
      }
    }
    case PBVH_GRIDS: {
      if (!pbvh->boundary_flags) {
        return false;
      }

      if (pbvh->boundary_flags[vertex.i] &
          (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV))
      {
        update_vert_boundary_grids(pbvh, vertex.i, face_sets);
        return true;
      }
    }
  }

  return false;
}

bool check_edge_boundary(PBVH *pbvh, PBVHEdgeRef edge, const int *face_sets)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH: {
      BMEdge *e = reinterpret_cast<BMEdge *>(edge.i);

      if (pbvh->cd_edge_boundary == -1) {
        return false;
      }

      if (BM_ELEM_CD_GET_INT(e, pbvh->cd_edge_boundary) &
          (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV))
      {
        update_edge_boundary_bmesh(e,
                                   pbvh->cd_faceset_offset,
                                   pbvh->cd_edge_boundary,
                                   pbvh->cd_flag,
                                   pbvh->cd_valence,
                                   &pbvh->header.bm->ldata,
                                   pbvh->sharp_angle_limit);
      }
    }
    case PBVH_FACES: {
      if (!pbvh->edge_boundary_flags) {
        return false;
      }
      if (pbvh->edge_boundary_flags[edge.i] &
          (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV))
      {
        Span<float3> cos = pbvh->vert_positions;
        Span<float3> nos = pbvh->vert_normals;

        update_edge_boundary_faces(edge.i,
                                   cos,
                                   nos,
                                   pbvh->edges,
                                   pbvh->faces,
                                   pbvh->face_normals,
                                   pbvh->edge_boundary_flags,
                                   pbvh->boundary_flags,
                                   face_sets,
                                   pbvh->sharp_edges,
                                   pbvh->seam_edges,
                                   pbvh->vert_to_face_map,
                                   {},
                                   pbvh->corner_data,
                                   pbvh->sharp_angle_limit,
                                   pbvh->corner_verts,
                                   pbvh->corner_edges);
        return true;
      }

      break;
    }
    case PBVH_GRIDS: {
      if (!pbvh->edge_boundary_flags) {
        return false;
      }

      if (pbvh->edge_boundary_flags[edge.i] &
          (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV))
      {
        update_edge_boundary_grids(edge.i,
                                   pbvh->edges,
                                   pbvh->faces,
                                   pbvh->edge_boundary_flags,
                                   pbvh->boundary_flags,
                                   face_sets,
                                   pbvh->sharp_edges,
                                   pbvh->seam_edges,
                                   pbvh->vert_to_face_map,
                                   {},
                                   pbvh->corner_data,
                                   pbvh->subdiv_ccg,
                                   BKE_pbvh_get_grid_key(pbvh),
                                   pbvh->sharp_angle_limit,
                                   pbvh->corner_verts,
                                   pbvh->corner_edges);
        return true;
      }
      break;
    }
  }

  return false;
}

}  // namespace blender::bke::pbvh
