/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <climits>

#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bitmap.h"
#include "BLI_bounds.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_time.h"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_ccg.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DRW_pbvh.hh"

#include "bmesh.hh"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

namespace blender::bke::pbvh {

#define LEAF_LIMIT 10000

#define STACK_FIXED_DEPTH 100

struct PBVHStack {
  blender::bke::pbvh::Node *node;
  bool revisiting;
};

struct PBVHIter {
  blender::bke::pbvh::Tree *pbvh;
  blender::FunctionRef<bool(blender::bke::pbvh::Node &)> scb;

  PBVHStack *stack;
  int stacksize;

  PBVHStack stackfixed[STACK_FIXED_DEPTH];
  int stackspace;
};

/** Create invalid bounds for use with #math::min_max. */
static Bounds<float3> negative_bounds()
{
  return {float3(std::numeric_limits<float>::max()), float3(std::numeric_limits<float>::lowest())};
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
                                  MutableSpan<int> prim_scratch,
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
                                 MutableSpan<bool> vert_bitmap,
                                 Node &node)
{
  node.unique_verts_num_ = 0;
  int shared_verts = 0;
  const Span<int> prim_indices = node.prim_indices_;

  /* reserve size is rough guess */
  Map<int, int> map;
  map.reserve(prim_indices.size());

  node.face_vert_indices_.reinitialize(prim_indices.size());

  for (const int i : prim_indices.index_range()) {
    const int3 &tri = corner_tris[prim_indices[i]];
    for (int j = 0; j < 3; j++) {
      node.face_vert_indices_[i][j] = map_insert_vert(
          map, vert_bitmap, &shared_verts, &node.unique_verts_num_, corner_verts[tri[j]]);
    }
  }

  node.vert_indices_.reinitialize(node.unique_verts_num_ + shared_verts);

  /* Build the vertex list, unique verts first */
  for (const MapItem<int, int> item : map.items()) {
    int value = item.value;
    if (value < 0) {
      value = -value + node.unique_verts_num_ - 1;
    }

    node.vert_indices_[value] = item.key;
  }

  for (const int i : prim_indices.index_range()) {
    for (int j = 0; j < 3; j++) {
      if (node.face_vert_indices_[i][j] < 0) {
        node.face_vert_indices_[i][j] = -node.face_vert_indices_[i][j] + node.unique_verts_num_ -
                                        1;
      }
    }
  }

  BKE_pbvh_node_mark_positions_update(&node);
  BKE_pbvh_node_mark_rebuild_draw(&node);
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static bool leaf_needs_material_split(Tree &pbvh,
                                      const Span<int> prim_to_face_map,
                                      const Span<int> material_indices,
                                      const Span<bool> sharp_faces,
                                      int offset,
                                      int count)
{
  if (count <= 1) {
    return false;
  }

  const int first = prim_to_face_map[pbvh.prim_indices_[offset]];
  for (int i = offset + count - 1; i > offset; i--) {
    int prim = pbvh.prim_indices_[i];
    if (!face_materials_match(material_indices, sharp_faces, first, prim_to_face_map[prim])) {
      return true;
    }
  }

  return false;
}

static void build_nodes_recursive_mesh(Tree &pbvh,
                                       const Span<int> corner_verts,
                                       const Span<int3> corner_tris,
                                       const Span<int> tri_faces,
                                       const Span<int> material_indices,
                                       const Span<bool> sharp_faces,
                                       const int leaf_limit,
                                       MutableSpan<bool> vert_bitmap,
                                       const int node_index,
                                       const Bounds<float3> *cb,
                                       const Span<Bounds<float3>> prim_bounds,
                                       const int prim_offset,
                                       const int prims_num,
                                       MutableSpan<int> prim_scratch,
                                       const int depth)
{
  int end;

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = prims_num <= leaf_limit || depth >= STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(
            pbvh, tri_faces, material_indices, sharp_faces, prim_offset, prims_num))
    {
      Node &node = pbvh.nodes_[node_index];
      node.flag_ |= PBVH_Leaf;
      node.prim_indices_ = pbvh.prim_indices_.as_span().slice(prim_offset, prims_num);
      build_mesh_leaf_node(corner_verts, corner_tris, vert_bitmap, node);

      return;
    }
  }

  /* Add two child nodes */
  pbvh.nodes_[node_index].children_offset_ = pbvh.nodes_.size();
  pbvh.nodes_.resize(pbvh.nodes_.size() + 2);

  Bounds<float3> cb_backing;
  if (!below_leaf_limit) {
    /* Find axis with widest range of primitive centroids */
    if (!cb) {
      cb_backing = negative_bounds();
      for (int i = prim_offset + prims_num - 1; i >= prim_offset; i--) {
        const int prim = pbvh.prim_indices_[i];
        const float3 center = math::midpoint(prim_bounds[prim].min, prim_bounds[prim].max);
        math::min_max(center, cb_backing.min, cb_backing.max);
      }
      cb = &cb_backing;
    }
    const int axis = math::dominant_axis(cb->max - cb->min);

    /* Partition primitives along that axis */
    end = partition_prim_indices(pbvh.prim_indices_,
                                 prim_scratch,
                                 prim_offset,
                                 prim_offset + prims_num,
                                 axis,
                                 math::midpoint(cb->min[axis], cb->max[axis]),
                                 prim_bounds,
                                 tri_faces);
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material_faces(pbvh.prim_indices_,
                                           tri_faces,
                                           material_indices,
                                           sharp_faces,
                                           prim_offset,
                                           prim_offset + prims_num - 1);
  }

  /* Build children */
  build_nodes_recursive_mesh(pbvh,
                             corner_verts,
                             corner_tris,
                             tri_faces,
                             material_indices,
                             sharp_faces,
                             leaf_limit,
                             vert_bitmap,
                             pbvh.nodes_[node_index].children_offset_,
                             nullptr,
                             prim_bounds,
                             prim_offset,
                             end - prim_offset,
                             prim_scratch,
                             depth + 1);
  build_nodes_recursive_mesh(pbvh,
                             corner_verts,
                             corner_tris,
                             tri_faces,
                             material_indices,
                             sharp_faces,
                             leaf_limit,
                             vert_bitmap,
                             pbvh.nodes_[node_index].children_offset_ + 1,
                             nullptr,
                             prim_bounds,
                             end,
                             prim_offset + prims_num - end,
                             prim_scratch,
                             depth + 1);
}

void update_mesh_pointers(Tree &pbvh, Mesh *mesh)
{
  BLI_assert(pbvh.type() == Type::Mesh);
  if (!pbvh.deformed_) {
    /* Deformed data not matching the original mesh are owned directly by the
     * Tree, and are set separately by #BKE_pbvh_vert_coords_apply. */
    pbvh.vert_positions_ = mesh->vert_positions_for_write();
    pbvh.vert_normals_ = mesh->vert_normals();
    pbvh.face_normals_ = mesh->face_normals();
  }
}

std::unique_ptr<Tree> build_mesh(Mesh *mesh)
{
  std::unique_ptr<Tree> pbvh = std::make_unique<Tree>(Type::Mesh);

  MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int3> corner_tris = mesh->corner_tris();

  pbvh->mesh_ = mesh;

  update_mesh_pointers(*pbvh, mesh);
  const Span<int> tri_faces = mesh->corner_tri_faces();

  Array<bool> vert_bitmap(mesh->verts_num, false);

  const int leaf_limit = LEAF_LIMIT;

  /* For each face, store the AABB and the AABB centroid */
  Array<Bounds<float3>> prim_bounds(corner_tris.size());
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

  if (!corner_tris.is_empty()) {
    const AttributeAccessor attributes = mesh->attributes();
    const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
    const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);
    const VArraySpan sharp_face = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);

    pbvh->prim_indices_.reinitialize(corner_tris.size());
    array_utils::fill_index_range<int>(pbvh->prim_indices_);

    pbvh->nodes_.resize(1);
    build_nodes_recursive_mesh(*pbvh,
                               corner_verts,
                               corner_tris,
                               tri_faces,
                               material_index,
                               sharp_face,
                               leaf_limit,
                               vert_bitmap,
                               0,
                               &cb,
                               prim_bounds,
                               0,
                               corner_tris.size(),
                               Array<int>(pbvh->prim_indices_.size()),
                               0);

    update_bounds(*pbvh);
    store_bounds_orig(*pbvh);

    if (!hide_vert.is_empty()) {
      MutableSpan<Node> nodes = pbvh->nodes_;
      threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
        for (const int i : range) {
          const Span<int> verts = node_verts(nodes[i]);
          if (std::all_of(verts.begin(), verts.end(), [&](const int i) { return hide_vert[i]; })) {
            nodes[i].flag_ |= PBVH_FullyHidden;
          }
        }
      });
    }
  }

  return pbvh;
}

static void build_nodes_recursive_grids(Tree &pbvh,
                                        const Span<int> material_indices,
                                        const Span<bool> sharp_faces,
                                        const int leaf_limit,
                                        const int node_index,
                                        const Bounds<float3> *cb,
                                        const Span<Bounds<float3>> prim_bounds,
                                        const int prim_offset,
                                        const int prims_num,
                                        MutableSpan<int> prim_scratch,
                                        const int depth)
{
  const Span<int> prim_to_face_map = pbvh.subdiv_ccg_->grid_to_face_map;
  int end;

  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = prims_num <= leaf_limit || depth >= STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(
            pbvh, prim_to_face_map, material_indices, sharp_faces, prim_offset, prims_num))
    {
      Node &node = pbvh.nodes_[node_index];
      node.flag_ |= PBVH_Leaf;

      node.prim_indices_ = pbvh.prim_indices_.as_span().slice(prim_offset, prims_num);
      BKE_pbvh_node_mark_positions_update(&node);
      BKE_pbvh_node_mark_rebuild_draw(&node);
      return;
    }
  }

  /* Add two child nodes */
  pbvh.nodes_[node_index].children_offset_ = pbvh.nodes_.size();
  pbvh.nodes_.resize(pbvh.nodes_.size() + 2);

  Bounds<float3> cb_backing;
  if (!below_leaf_limit) {
    /* Find axis with widest range of primitive centroids */
    if (!cb) {
      cb_backing = negative_bounds();
      for (int i = prim_offset + prims_num - 1; i >= prim_offset; i--) {
        const int prim = pbvh.prim_indices_[i];
        const float3 center = math::midpoint(prim_bounds[prim].min, prim_bounds[prim].max);
        math::min_max(center, cb_backing.min, cb_backing.max);
      }
      cb = &cb_backing;
    }
    const int axis = math::dominant_axis(cb->max - cb->min);

    /* Partition primitives along that axis */
    end = partition_prim_indices(pbvh.prim_indices_,
                                 prim_scratch,
                                 prim_offset,
                                 prim_offset + prims_num,
                                 axis,
                                 math::midpoint(cb->min[axis], cb->max[axis]),
                                 prim_bounds,
                                 prim_to_face_map);
  }
  else {
    /* Partition primitives by material */
    end = partition_indices_material_faces(pbvh.prim_indices_,
                                           prim_to_face_map,
                                           material_indices,
                                           sharp_faces,
                                           prim_offset,
                                           prim_offset + prims_num - 1);
  }

  /* Build children */
  build_nodes_recursive_grids(pbvh,
                              material_indices,
                              sharp_faces,
                              leaf_limit,
                              pbvh.nodes_[node_index].children_offset_,
                              nullptr,
                              prim_bounds,
                              prim_offset,
                              end - prim_offset,
                              prim_scratch,
                              depth + 1);
  build_nodes_recursive_grids(pbvh,
                              material_indices,
                              sharp_faces,
                              leaf_limit,
                              pbvh.nodes_[node_index].children_offset_ + 1,
                              nullptr,
                              prim_bounds,
                              end,
                              prim_offset + prims_num - end,
                              prim_scratch,
                              depth + 1);
}

std::unique_ptr<Tree> build_grids(Mesh *mesh, SubdivCCG *subdiv_ccg)
{
  std::unique_ptr<Tree> pbvh = std::make_unique<Tree>(Type::Grids);

  pbvh->subdiv_ccg_ = subdiv_ccg;

  /* Find maximum number of grids per face. */
  int max_grids = 1;
  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    max_grids = max_ii(max_grids, faces[i].size());
  }

  const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);
  const Span<CCGElem *> grids = subdiv_ccg->grids;

  /* Ensure leaf limit is at least 4 so there's room
   * to split at original face boundaries.
   * Fixes #102209.
   */
  const int leaf_limit = max_ii(LEAF_LIMIT / (key.grid_area), max_grids);

  /* We also need the base mesh for Tree draw. */
  pbvh->mesh_ = mesh;

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
          for (const int j : IndexRange(key.grid_area)) {
            const float3 &position = CCG_elem_offset_co(key, grid, j);
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

    pbvh->prim_indices_.reinitialize(grids.size());
    array_utils::fill_index_range<int>(pbvh->prim_indices_);

    pbvh->nodes_.resize(1);
    build_nodes_recursive_grids(*pbvh,
                                material_index,
                                sharp_face,
                                leaf_limit,
                                0,
                                &cb,
                                prim_bounds,
                                0,
                                grids.size(),
                                Array<int>(pbvh->prim_indices_.size()),
                                0);

    update_bounds(*pbvh);
    store_bounds_orig(*pbvh);

    const BitGroupVector<> &grid_hidden = subdiv_ccg->grid_hidden;
    if (!grid_hidden.is_empty()) {
      MutableSpan<Node> nodes = pbvh->nodes_;
      threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
        for (const int i : range) {
          const Span<int> grids = node_grid_indices(nodes[i]);
          if (std::all_of(grids.begin(), grids.end(), [&](const int i) {
                return !bits::any_bit_unset(grid_hidden[i]);
              }))
          {
            nodes[i].flag_ |= PBVH_FullyHidden;
          }
        }
      });
    }
  }

  return pbvh;
}

Tree::~Tree()
{
  for (Node &node : this->nodes_) {
    if (node.flag_ & PBVH_Leaf) {
      if (node.draw_batches_) {
        blender::draw::pbvh::node_free(node.draw_batches_);
      }
    }

    if (node.flag_ & (PBVH_Leaf | PBVH_TexLeaf)) {
      node_pixels_free(&node);
    }
  }

  pixels_free(this);
}

void free(std::unique_ptr<Tree> &pbvh)
{
  pbvh.reset();
}

static void pbvh_iter_begin(PBVHIter *iter, Tree &pbvh, FunctionRef<bool(Node &)> scb)
{
  iter->pbvh = &pbvh;
  iter->scb = scb;

  iter->stack = iter->stackfixed;
  iter->stackspace = STACK_FIXED_DEPTH;

  iter->stack[0].node = &pbvh.nodes_.first();
  iter->stack[0].revisiting = false;
  iter->stacksize = 1;
}

static void pbvh_iter_end(PBVHIter *iter)
{
  if (iter->stackspace > STACK_FIXED_DEPTH) {
    MEM_freeN(iter->stack);
  }
}

static void pbvh_stack_push(PBVHIter *iter, Node *node, bool revisiting)
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

static Node *pbvh_iter_next(PBVHIter *iter, PBVHNodeFlags leaf_flag)
{
  /* purpose here is to traverse tree, visiting child nodes before their
   * parents, this order is necessary for e.g. computing bounding boxes */

  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    Node *node = iter->stack[iter->stacksize].node;

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

    if (node->flag_ & leaf_flag) {
      /* immediately hit leaf node */
      return node;
    }

    /* come back later when children are done */
    pbvh_stack_push(iter, node, true);

    /* push two child nodes on the stack */
    pbvh_stack_push(iter, &iter->pbvh->nodes_[node->children_offset_ + 1], false);
    pbvh_stack_push(iter, &iter->pbvh->nodes_[node->children_offset_], false);
  }

  return nullptr;
}

static Node *pbvh_iter_next_occluded(PBVHIter *iter)
{
  while (iter->stacksize) {
    /* pop node */
    iter->stacksize--;
    Node *node = iter->stack[iter->stacksize].node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == nullptr) {
      return nullptr;
    }

    if (iter->scb && !iter->scb(*node)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag_ & PBVH_Leaf) {
      /* immediately hit leaf node */
      return node;
    }

    pbvh_stack_push(iter, &iter->pbvh->nodes_[node->children_offset_ + 1], false);
    pbvh_stack_push(iter, &iter->pbvh->nodes_[node->children_offset_], false);
  }

  return nullptr;
}

struct node_tree {
  Node *data;

  node_tree *left;
  node_tree *right;
};

static void node_tree_insert(node_tree *tree, node_tree *new_node)
{
  if (new_node->data->tmin_ < tree->data->tmin_) {
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
                          const FunctionRef<void(Node &node, float *tmin)> hit_fn,
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

  ::free(tree);
}

}  // namespace blender::bke::pbvh

float BKE_pbvh_node_get_tmin(const blender::bke::pbvh::Node *node)
{
  return node->tmin_;
}

namespace blender::bke::pbvh {

void search_callback(Tree &pbvh,
                     FunctionRef<bool(Node &)> filter_fn,
                     FunctionRef<void(Node &)> hit_fn)
{
  if (pbvh.nodes_.is_empty()) {
    return;
  }
  PBVHIter iter;
  Node *node;

  pbvh_iter_begin(&iter, pbvh, filter_fn);

  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
    if (node->flag_ & PBVH_Leaf) {
      hit_fn(*node);
    }
  }

  pbvh_iter_end(&iter);
}

static void search_callback_occluded(Tree &pbvh,
                                     const FunctionRef<bool(Node &)> scb,
                                     const FunctionRef<void(Node &node, float *tmin)> hit_fn)
{
  if (pbvh.nodes_.is_empty()) {
    return;
  }
  PBVHIter iter;
  Node *node;
  node_tree *tree = nullptr;

  pbvh_iter_begin(&iter, pbvh, scb);

  while ((node = pbvh_iter_next_occluded(&iter))) {
    if (node->flag_ & PBVH_Leaf) {
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

static bool update_search(Node *node, const int flag)
{
  if (node->flag_ & PBVH_Leaf) {
    return (node->flag_ & flag) != 0;
  }

  return true;
}

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
                                   const Span<int> corner_tri_faces,
                                   const Span<const Node *> nodes,
                                   MutableSpan<float3> face_normals)
{
  threading::EnumerableThreadSpecific<Vector<int>> all_index_data;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<int> &node_faces = all_index_data.local();
    for (const Node *node : nodes.slice(range)) {
      normals_calc_faces(positions,
                         faces,
                         corner_verts,
                         node_face_indices_calc_mesh(corner_tri_faces, *node, node_faces),
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
                                   const Span<Node *> nodes,
                                   MutableSpan<float3> vert_normals)
{
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const Node *node : nodes.slice(range)) {
      normals_calc_verts_simple(
          vert_to_face_map, face_normals, node_unique_verts(*node), vert_normals);
    }
  });
}

static void update_normals_faces(Tree &pbvh, Span<Node *> nodes, Mesh &mesh)
{
  /* Position changes are tracked on a per-node level, so all the vertex and face normals for every
   * affected node are recalculated. However, the additional complexity comes from the fact that
   * changing vertex normals also changes surrounding face normals. Those changed face normals then
   * change the normals of all connected vertices, which can be in other nodes. So the set of
   * vertices that need recalculated normals can propagate into unchanged/untagged Tree nodes.
   *
   * Currently we have no good way of finding neighboring Tree nodes, so we use the vertex to
   * face topology map to find the neighboring vertices that need normal recalculation.
   *
   * Those boundary face and vertex indices are deduplicated with #VectorSet in order to avoid
   * duplicate work recalculation for the same vertex, and to make parallel storage for vertices
   * during recalculation thread-safe. */
  const Span<float3> positions = pbvh.vert_positions_;
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> tri_faces = mesh.corner_tri_faces();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();

  VectorSet<int> boundary_faces;
  for (const Node *node : nodes) {
    for (const int vert : node->vert_indices_.as_span().drop_front(node->unique_verts_num_)) {
      boundary_faces.add_multiple(vert_to_face_map[vert]);
    }
  }

  /* In certain cases when undoing strokes on a duplicate object, the cached data may be marked
   * dirty before this code is run, leaving the relevant vectors empty. We force reinitialize the
   * vectors to prevent crashes here.
   * See #125375 for more detail. */
  if (!pbvh.deformed_) {
    if (mesh.runtime->face_normals_cache.is_dirty()) {
      mesh.face_normals();
    }
    if (mesh.runtime->vert_normals_cache.is_dirty()) {
      mesh.vert_normals();
    }
  }

  VectorSet<int> boundary_verts;
  threading::parallel_invoke(
      [&]() {
        if (pbvh.deformed_) {
          calc_node_face_normals(
              positions, faces, corner_verts, tri_faces, nodes, pbvh.face_normals_deformed_);
          calc_boundary_face_normals(
              positions, faces, corner_verts, boundary_faces, pbvh.face_normals_deformed_);
        }
        else {
          mesh.runtime->face_normals_cache.update([&](Vector<float3> &r_data) {
            calc_node_face_normals(positions, faces, corner_verts, tri_faces, nodes, r_data);
            calc_boundary_face_normals(positions, faces, corner_verts, boundary_faces, r_data);
          });
          /* #SharedCache::update() reallocates cached vectors if they were shared initially. */
          pbvh.face_normals_ = mesh.runtime->face_normals_cache.data();
        }
      },
      [&]() {
        /* Update all normals connected to affected faces, even if not explicitly tagged. */
        boundary_verts.reserve(boundary_faces.size());
        for (const int face : boundary_faces) {
          boundary_verts.add_multiple(corner_verts.slice(faces[face]));
        }
      });

  if (pbvh.deformed_) {
    calc_node_vert_normals(
        vert_to_face_map, pbvh.face_normals_, nodes, pbvh.vert_normals_deformed_);
    calc_boundary_vert_normals(
        vert_to_face_map, pbvh.face_normals_, boundary_verts, pbvh.vert_normals_deformed_);
  }
  else {
    mesh.runtime->vert_normals_cache.update([&](Vector<float3> &r_data) {
      calc_node_vert_normals(vert_to_face_map, pbvh.face_normals_, nodes, r_data);
      calc_boundary_vert_normals(vert_to_face_map, pbvh.face_normals_, boundary_verts, r_data);
    });
    pbvh.vert_normals_ = mesh.runtime->vert_normals_cache.data();
  }

  for (Node *node : nodes) {
    node->flag_ &= ~PBVH_UpdateNormals;
  }
}

void update_normals(Tree &pbvh, SubdivCCG *subdiv_ccg)
{
  Vector<Node *> nodes = search_gather(
      pbvh, [&](Node &node) { return update_search(&node, PBVH_UpdateNormals); });
  if (nodes.is_empty()) {
    return;
  }

  if (pbvh.type() == Type::BMesh) {
    bmesh_normals_update(nodes);
  }
  else if (pbvh.type() == Type::Mesh) {
    update_normals_faces(pbvh, nodes, *pbvh.mesh_);
  }
  else if (pbvh.type() == Type::Grids) {
    IndexMaskMemory memory;
    const IndexMask faces_to_update = nodes_to_face_selection_grids(*subdiv_ccg, nodes, memory);
    BKE_subdiv_ccg_update_normals(*subdiv_ccg, faces_to_update);
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_UpdateNormals;
    }
  }
}

void update_node_bounds_mesh(const Span<float3> positions, Node &node)
{
  Bounds<float3> bounds = negative_bounds();
  for (const int vert : node_verts(node)) {
    math::min_max(positions[vert], bounds.min, bounds.max);
  }
  node.bounds_ = bounds;
}

void update_node_bounds_grids(const CCGKey &key, const Span<CCGElem *> grids, Node &node)
{
  Bounds<float3> bounds = negative_bounds();
  for (const int grid : node.prim_indices_) {
    for (const int i : IndexRange(key.grid_area)) {
      math::min_max(CCG_elem_offset_co(key, grids[grid], i), bounds.min, bounds.max);
    }
  }
  node.bounds_ = bounds;
}

void update_node_bounds_bmesh(Node &node)
{
  Bounds<float3> bounds = negative_bounds();
  for (const BMVert *vert : node.bm_unique_verts_) {
    math::min_max(float3(vert->co), bounds.min, bounds.max);
  }
  for (const BMVert *vert : node.bm_other_verts_) {
    math::min_max(float3(vert->co), bounds.min, bounds.max);
  }
  node.bounds_ = bounds;
}

static bool update_leaf_node_bounds(Tree &pbvh)
{
  Vector<Node *> nodes = search_gather(
      pbvh, [&](Node &node) { return update_search(&node, PBVH_UpdateBB); });
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.as_span().slice(range)) {
      switch (pbvh.type()) {
        case Type::Mesh:
          update_node_bounds_mesh(pbvh.vert_positions_, *node);
          break;
        case Type::Grids:
          update_node_bounds_grids(
              BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_), pbvh.subdiv_ccg_->grids, *node);
          break;
        case Type::BMesh:
          update_node_bounds_bmesh(*node);
          break;
      }
    }
  });
  return !nodes.is_empty();
}

struct BoundsMergeInfo {
  Bounds<float3> bounds;
  bool update;
};

static BoundsMergeInfo merge_child_bounds(MutableSpan<Node> nodes, const int node_index)
{
  Node &node = nodes[node_index];
  if (node.flag_ & PBVH_Leaf) {
    const bool update = node.flag_ & PBVH_UpdateBB;
    node.flag_ &= ~PBVH_UpdateBB;
    return {node.bounds_, update};
  }

  const BoundsMergeInfo info_0 = merge_child_bounds(nodes, node.children_offset_ + 0);
  const BoundsMergeInfo info_1 = merge_child_bounds(nodes, node.children_offset_ + 1);
  const bool update = info_0.update || info_1.update;
  if (update) {
    node.bounds_ = bounds::merge(info_0.bounds, info_1.bounds);
  }
  node.flag_ &= ~PBVH_UpdateBB;
  return {node.bounds_, update};
}

static void flush_bounds_to_parents(Tree &pbvh)
{
  pbvh.nodes_.first().bounds_ = merge_child_bounds(pbvh.nodes_, 0).bounds;
}

void update_bounds(Tree &pbvh)
{
  if (update_leaf_node_bounds(pbvh)) {
    flush_bounds_to_parents(pbvh);
  }
}

void store_bounds_orig(Tree &pbvh)
{
  MutableSpan<Node> nodes = pbvh.nodes_;
  threading::parallel_for(nodes.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      nodes[i].bounds_orig_ = nodes[i].bounds_;
    }
  });
}

void node_update_mask_mesh(const Span<float> mask, Node &node)
{
  const Span<int> verts = node_verts(node);
  const bool fully_masked = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return mask[vert] == 1.0f; });
  const bool fully_unmasked = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return mask[vert] <= 0.0f; });
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, PBVH_FullyUnmasked);
  node.flag_ &= ~PBVH_UpdateMask;
}

static void update_mask_mesh(const Mesh &mesh, const Span<Node *> nodes)
{
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask", AttrDomain::Point);
  if (mask.is_empty()) {
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_FullyMasked;
      node->flag_ |= PBVH_FullyUnmasked;
      node->flag_ &= ~PBVH_UpdateMask;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_mask_mesh(mask, *node);
    }
  });
}

void node_update_mask_grids(const CCGKey &key, const Span<CCGElem *> grids, Node &node)
{
  BLI_assert(key.has_mask);
  bool fully_masked = true;
  bool fully_unmasked = true;
  for (const int grid : node.prim_indices_) {
    CCGElem *elem = grids[grid];
    for (const int i : IndexRange(key.grid_area)) {
      const float mask = CCG_elem_offset_mask(key, elem, i);
      fully_masked &= mask == 1.0f;
      fully_unmasked &= mask <= 0.0f;
    }
  }
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, PBVH_FullyUnmasked);
  node.flag_ &= ~PBVH_UpdateMask;
}

static void update_mask_grids(const SubdivCCG &subdiv_ccg, const Span<Node *> nodes)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  if (!key.has_mask) {
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_FullyMasked;
      node->flag_ |= PBVH_FullyUnmasked;
      node->flag_ &= ~PBVH_UpdateMask;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_mask_grids(key, subdiv_ccg.grids, *node);
    }
  });
}

void node_update_mask_bmesh(const int mask_offset, Node &node)
{
  BLI_assert(mask_offset != -1);
  bool fully_masked = true;
  bool fully_unmasked = true;
  for (const BMVert *vert : node.bm_unique_verts_) {
    fully_masked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) == 1.0f;
    fully_unmasked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) <= 0.0f;
  }
  for (const BMVert *vert : node.bm_other_verts_) {
    fully_masked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) == 1.0f;
    fully_unmasked &= BM_ELEM_CD_GET_FLOAT(vert, mask_offset) <= 0.0f;
  }
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, PBVH_FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, PBVH_FullyUnmasked);
  node.flag_ &= ~PBVH_UpdateMask;
}

static void update_mask_bmesh(const BMesh &bm, const Span<Node *> nodes)
{
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (offset == -1) {
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_FullyMasked;
      node->flag_ |= PBVH_FullyUnmasked;
      node->flag_ &= ~PBVH_UpdateMask;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_mask_bmesh(offset, *node);
    }
  });
}

void update_mask(Tree &pbvh)
{
  Vector<Node *> nodes = search_gather(
      pbvh, [&](Node &node) { return update_search(&node, PBVH_UpdateMask); });

  switch (pbvh.type()) {
    case Type::Mesh:
      update_mask_mesh(*pbvh.mesh_, nodes);
      break;
    case Type::Grids:
      update_mask_grids(*pbvh.subdiv_ccg_, nodes);
      break;
    case Type::BMesh:
      update_mask_bmesh(*pbvh.bm_, nodes);
      break;
  }
}

void node_update_visibility_mesh(const Span<bool> hide_vert, Node &node)
{
  BLI_assert(!hide_vert.is_empty());
  const Span<int> verts = node_verts(node);
  const bool fully_hidden = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return hide_vert[vert]; });
  SET_FLAG_FROM_TEST(node.flag_, fully_hidden, PBVH_FullyHidden);
  node.flag_ &= ~PBVH_UpdateVisibility;
}

static void update_visibility_faces(const Mesh &mesh, const Span<Node *> nodes)
{
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
  if (hide_vert.is_empty()) {
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_FullyHidden;
      node->flag_ &= ~PBVH_UpdateVisibility;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_visibility_mesh(hide_vert, *node);
    }
  });
}

void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, Node &node)
{
  BLI_assert(!grid_hidden.is_empty());
  const bool fully_hidden = std::none_of(
      node.prim_indices_.begin(), node.prim_indices_.end(), [&](const int grid) {
        return bits::any_bit_unset(grid_hidden[grid]);
      });
  SET_FLAG_FROM_TEST(node.flag_, fully_hidden, PBVH_FullyHidden);
  node.flag_ &= ~PBVH_UpdateVisibility;
}

static void update_visibility_grids(Tree &pbvh, const Span<Node *> nodes)
{
  const BitGroupVector<> &grid_hidden = pbvh.subdiv_ccg_->grid_hidden;
  if (grid_hidden.is_empty()) {
    for (Node *node : nodes) {
      node->flag_ &= ~PBVH_FullyHidden;
      node->flag_ &= ~PBVH_UpdateVisibility;
    }
    return;
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_visibility_grids(grid_hidden, *node);
    }
  });
}

void node_update_visibility_bmesh(Node &node)
{
  const bool unique_hidden = std::all_of(
      node.bm_unique_verts_.begin(), node.bm_unique_verts_.end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  const bool other_hidden = std::all_of(
      node.bm_other_verts_.begin(), node.bm_other_verts_.end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  SET_FLAG_FROM_TEST(node.flag_, unique_hidden && other_hidden, PBVH_FullyHidden);
  node.flag_ &= ~PBVH_UpdateVisibility;
}

static void update_visibility_bmesh(const Span<Node *> nodes)
{
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_visibility_bmesh(*node);
    }
  });
}

void update_visibility(Tree &pbvh)
{
  Vector<Node *> nodes = search_gather(
      pbvh, [&](Node &node) { return update_search(&node, PBVH_UpdateVisibility); });

  switch (pbvh.type()) {
    case Type::Mesh:
      update_visibility_faces(*pbvh.mesh_, nodes);
      break;
    case Type::Grids:
      update_visibility_grids(pbvh, nodes);
      break;
    case Type::BMesh:
      update_visibility_bmesh(nodes);
      break;
  }
}

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

blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(blender::bke::pbvh::Tree &pbvh)
{
  using namespace blender;
  using namespace blender::bke::pbvh;
  if (pbvh.nodes_.is_empty()) {
    return {};
  }
  Bounds<float3> bounds = negative_bounds();

  PBVHIter iter;
  pbvh_iter_begin(&iter, pbvh, {});
  Node *node;
  while ((node = pbvh_iter_next(&iter, PBVH_Leaf))) {
    if (node->flag_ & PBVH_UpdateRedraw) {
      bounds = bounds::merge(bounds, node->bounds_);
    }
  }
  pbvh_iter_end(&iter);

  return bounds;
}

namespace blender::bke::pbvh {

IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        const Span<const Node *> nodes,
                                        IndexMaskMemory &memory)
{
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  /* Using a #VectorSet for index deduplication would also work, but the performance gets much
   * worse with large selections since the loop would be single-threaded. A boolean array has an
   * overhead regardless of selection size, but that is small. */
  Array<bool> faces_to_update(subdiv_ccg.faces.size(), false);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const Node *node : nodes.slice(range)) {
      for (const int grid : node->prim_indices_) {
        faces_to_update[grid_to_face_map[grid]] = true;
      }
    }
  });
  return IndexMask::from_bools(faces_to_update, memory);
}

Bounds<float3> bounds_get(const Tree &pbvh)
{
  if (pbvh.nodes_.is_empty()) {
    return float3(0);
  }
  return pbvh.nodes_.first().bounds_;
}

}  // namespace blender::bke::pbvh

int BKE_pbvh_get_grid_num_verts(const blender::bke::pbvh::Tree &pbvh)
{
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Grids);
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
  return pbvh.subdiv_ccg_->grids.size() * key.grid_area;
}

int BKE_pbvh_get_grid_num_faces(const blender::bke::pbvh::Tree &pbvh)
{
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Grids);
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
  return pbvh.subdiv_ccg_->grids.size() * square_i(key.grid_size - 1);
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_update(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw |
                 PBVH_RebuildPixels;
}

void BKE_pbvh_node_mark_update_mask(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateMask | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_color(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateColor | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_update_face_sets(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_mark_rebuild_pixels(blender::bke::pbvh::Tree &pbvh)
{
  for (blender::bke::pbvh::Node &node : pbvh.nodes_) {
    if (node.flag_ & PBVH_Leaf) {
      node.flag_ |= PBVH_RebuildPixels;
    }
  }
}

void BKE_pbvh_node_mark_update_visibility(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateVisibility | PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers |
                 PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_rebuild_draw(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_redraw(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_positions_update(blender::bke::pbvh::Node *node)
{
  node->flag_ |= PBVH_UpdateNormals | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw | PBVH_UpdateBB;
}

void BKE_pbvh_node_fully_hidden_set(blender::bke::pbvh::Node *node, int fully_hidden)
{
  BLI_assert(node->flag_ & PBVH_Leaf);

  if (fully_hidden) {
    node->flag_ |= PBVH_FullyHidden;
  }
  else {
    node->flag_ &= ~PBVH_FullyHidden;
  }
}

bool BKE_pbvh_node_fully_hidden_get(const blender::bke::pbvh::Node *node)
{
  return (node->flag_ & PBVH_Leaf) && (node->flag_ & PBVH_FullyHidden);
}

void BKE_pbvh_node_fully_masked_set(blender::bke::pbvh::Node *node, int fully_masked)
{
  BLI_assert(node->flag_ & PBVH_Leaf);

  if (fully_masked) {
    node->flag_ |= PBVH_FullyMasked;
  }
  else {
    node->flag_ &= ~PBVH_FullyMasked;
  }
}

bool BKE_pbvh_node_fully_masked_get(const blender::bke::pbvh::Node *node)
{
  return (node->flag_ & PBVH_Leaf) && (node->flag_ & PBVH_FullyMasked);
}

void BKE_pbvh_node_fully_unmasked_set(blender::bke::pbvh::Node *node, int fully_masked)
{
  BLI_assert(node->flag_ & PBVH_Leaf);

  if (fully_masked) {
    node->flag_ |= PBVH_FullyUnmasked;
  }
  else {
    node->flag_ &= ~PBVH_FullyUnmasked;
  }
}

bool BKE_pbvh_node_fully_unmasked_get(const blender::bke::pbvh::Node *node)
{
  return (node->flag_ & PBVH_Leaf) && (node->flag_ & PBVH_FullyUnmasked);
}

namespace blender::bke::pbvh {

Span<int> node_corners(const Node &node)
{
  return node.corner_indices_;
}

Span<int> node_verts(const Node &node)
{
  return node.vert_indices_;
}

Span<int> node_unique_verts(const Node &node)
{
  return node.vert_indices_.as_span().take_front(node.unique_verts_num_);
}

Span<int> node_face_indices_calc_mesh(const Span<int> corner_tri_faces,
                                      const Node &node,
                                      Vector<int> &faces)
{
  faces.clear();
  int prev_face = -1;
  for (const int tri : node.prim_indices_) {
    const int face = corner_tri_faces[tri];
    if (face != prev_face) {
      faces.append(face);
      prev_face = face;
    }
  }
  return faces.as_span();
}

Span<int> node_face_indices_calc_grids(const Tree &pbvh, const Node &node, Vector<int> &faces)
{
  faces.clear();
  const Span<int> grid_to_face_map = pbvh.subdiv_ccg_->grid_to_face_map;
  int prev_face = -1;
  for (const int prim : node.prim_indices_) {
    const int face = grid_to_face_map[prim];
    if (face != prev_face) {
      faces.append(face);
      prev_face = face;
    }
  }
  return faces.as_span();
}

Span<int> node_grid_indices(const Node &node)
{
  return node.prim_indices_;
}

}  // namespace blender::bke::pbvh

namespace blender::bke::pbvh {

Bounds<float3> node_bounds(const Node &node)
{
  return node.bounds_;
}

}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_node_get_original_BB(
    const blender::bke::pbvh::Node *node)
{
  return node->bounds_orig_;
}

void BKE_pbvh_node_get_bm_orco_data(blender::bke::pbvh::Node *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3],
                                    BMVert ***r_orco_verts)
{
  *r_orco_tris = node->bm_ortri_;
  *r_orco_tris_num = node->bm_tot_ortri_;
  *r_orco_coords = node->bm_orco_;

  if (r_orco_verts) {
    *r_orco_verts = node->bm_orvert_;
  }
}

/********************************* Ray-cast ***********************************/

namespace blender::bke::pbvh {

struct RaycastData {
  IsectRayAABB_Precalc ray;
  bool original;
};

static bool ray_aabb_intersect(Node &node, const RaycastData &rcd)
{
  if (rcd.original) {
    return isect_ray_aabb_v3(&rcd.ray, node.bounds_orig_.min, node.bounds_orig_.max, &node.tmin_);
  }
  return isect_ray_aabb_v3(&rcd.ray, node.bounds_.min, node.bounds_.max, &node.tmin_);
}

void raycast(Tree &pbvh,
             const FunctionRef<void(Node &node, float *tmin)> hit_fn,
             const float ray_start[3],
             const float ray_normal[3],
             bool original)
{
  RaycastData rcd;

  isect_ray_aabb_v3_precalc(&rcd.ray, ray_start, ray_normal);
  rcd.original = original;

  search_callback_occluded(
      pbvh, [&](Node &node) { return ray_aabb_intersect(node, rcd); }, hit_fn);
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

static bool pbvh_faces_node_raycast(Tree &pbvh,
                                    const Node &node,
                                    const float (*origco)[3],
                                    const Span<int> corner_verts,
                                    const Span<int3> corner_tris,
                                    const Span<int> corner_tri_faces,
                                    const Span<bool> hide_poly,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    PBVHVertRef *r_active_vertex,
                                    int *r_active_face_index,
                                    float *r_face_normal)
{
  using namespace blender;
  const Span<float3> positions = pbvh.vert_positions_;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0f};

  for (const int i : node.prim_indices_.index_range()) {
    const int tri_i = node.prim_indices_[i];
    const int3 &tri = corner_tris[tri_i];
    const int3 face_verts = node.face_vert_indices_[i];

    if (!hide_poly.is_empty() && hide_poly[corner_tri_faces[tri_i]]) {
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
              len_squared_v3v3(location, co[j]) < len_squared_v3v3(location, nearest_vertex_co))
          {
            copy_v3_v3(nearest_vertex_co, co[j]);
            r_active_vertex->i = corner_verts[tri[j]];
            *r_active_face_index = corner_tri_faces[tri_i];
          }
        }
      }
    }
  }

  return hit;
}

static bool pbvh_grids_node_raycast(Tree &pbvh,
                                    Node &node,
                                    const float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    IsectRayPrecalc *isect_precalc,
                                    float *depth,
                                    PBVHVertRef *r_active_vertex,
                                    int *r_active_grid_index,
                                    float *r_face_normal)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
  const int totgrid = node.prim_indices_.size();
  const int gridsize = key.grid_size;
  bool hit = false;
  float nearest_vertex_co[3] = {0.0};
  const BitGroupVector<> &grid_hidden = pbvh.subdiv_ccg_->grid_hidden;
  const Span<CCGElem *> grids = pbvh.subdiv_ccg_->grids;

  for (int i = 0; i < totgrid; i++) {
    const int grid_index = node.prim_indices_[i];
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
          co[0] = CCG_grid_elem_co(key, grid, x, y + 1);
          co[1] = CCG_grid_elem_co(key, grid, x + 1, y + 1);
          co[2] = CCG_grid_elem_co(key, grid, x + 1, y);
          co[3] = CCG_grid_elem_co(key, grid, x, y);
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

                r_active_vertex->i = key.grid_area * grid_index + (y + y_it[j]) * key.grid_size +
                                     (x + x_it[j]);
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

bool raycast_node(Tree &pbvh,
                  Node &node,
                  const float (*origco)[3],
                  bool use_origco,
                  const Span<int> corner_verts,
                  const Span<int3> corner_tris,
                  const Span<int> corner_tri_faces,
                  const Span<bool> hide_poly,
                  const float ray_start[3],
                  const float ray_normal[3],
                  IsectRayPrecalc *isect_precalc,
                  float *depth,
                  PBVHVertRef *active_vertex,
                  int *active_face_grid_index,
                  float *face_normal)
{
  bool hit = false;

  if (node.flag_ & PBVH_FullyHidden) {
    return false;
  }

  switch (pbvh.type()) {
    case Type::Mesh:
      hit |= pbvh_faces_node_raycast(pbvh,
                                     node,
                                     origco,
                                     corner_verts,
                                     corner_tris,
                                     corner_tri_faces,
                                     hide_poly,
                                     ray_start,
                                     ray_normal,
                                     isect_precalc,
                                     depth,
                                     active_vertex,
                                     active_face_grid_index,
                                     face_normal);
      break;
    case Type::Grids:
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
    case Type::BMesh:
      BM_mesh_elem_index_ensure(pbvh.bm_, BM_VERT);
      hit = bmesh_node_raycast(node,
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

void clip_ray_ortho(
    Tree &pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (pbvh.nodes_.is_empty()) {
    return;
  }
  float rootmin_start, rootmin_end;
  Bounds<float3> bb_root;
  float bb_center[3], bb_diff[3];
  IsectRayAABB_Precalc ray;
  float ray_normal_inv[3];
  float offset = 1.0f + 1e-3f;
  const float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

  if (original) {
    bb_root = BKE_pbvh_node_get_original_BB(&pbvh.nodes_.first());
  }
  else {
    bb_root = node_bounds(pbvh.nodes_.first());
  }

  /* Calc rough clipping to avoid overflow later. See #109555. */
  float mat[3][3];
  axis_dominant_v3_to_m3(mat, ray_normal);
  float a[3], b[3], min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {FLT_MIN, FLT_MIN, FLT_MIN};

  /* Compute AABB bounds rotated along ray_normal. */
  copy_v3_v3(a, bb_root.min);
  copy_v3_v3(b, bb_root.max);
  mul_m3_v3(mat, a);
  mul_m3_v3(mat, b);
  minmax_v3v3_v3(min, max, a);
  minmax_v3v3_v3(min, max, b);

  float cent[3];

  /* Find midpoint of aabb on ray. */
  mid_v3_v3v3(cent, bb_root.min, bb_root.max);
  float t = line_point_factor_v3(cent, ray_start, ray_end);
  interp_v3_v3v3(cent, ray_start, ray_end, t);

  /* Compute rough interval. */
  float dist = max[2] - min[2];
  madd_v3_v3v3fl(ray_start, cent, ray_normal, -dist);
  madd_v3_v3v3fl(ray_end, cent, ray_normal, dist);

  /* Slightly offset min and max in case we have a zero width node
   * (due to a plane mesh for instance), or faces very close to the bounding box boundary. */
  mid_v3_v3v3(bb_center, bb_root.max, bb_root.min);
  /* Diff should be same for both min/max since it's calculated from center. */
  sub_v3_v3v3(bb_diff, bb_root.max, bb_center);
  /* Handles case of zero width bb. */
  add_v3_v3(bb_diff, offset_vec);
  madd_v3_v3v3fl(bb_root.max, bb_center, bb_diff, offset);
  madd_v3_v3v3fl(bb_root.min, bb_center, bb_diff, -offset);

  /* Final projection of start ray. */
  isect_ray_aabb_v3_precalc(&ray, ray_start, ray_normal);
  if (!isect_ray_aabb_v3(&ray, bb_root.min, bb_root.max, &rootmin_start)) {
    return;
  }

  /* Final projection of end ray. */
  mul_v3_v3fl(ray_normal_inv, ray_normal, -1.0);
  isect_ray_aabb_v3_precalc(&ray, ray_end, ray_normal_inv);
  /* Unlikely to fail exiting if entering succeeded, still keep this here. */
  if (!isect_ray_aabb_v3(&ray, bb_root.min, bb_root.max, &rootmin_end)) {
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

static bool nearest_to_ray_aabb_dist_sq(Node *node,
                                        const DistRayAABB_Precalc &dist_ray_to_aabb_precalc,
                                        const bool original)
{
  const float *bb_min, *bb_max;

  if (original) {
    /* BKE_pbvh_node_get_original_BB */
    bb_min = node->bounds_orig_.min;
    bb_max = node->bounds_orig_.max;
  }
  else {
    bb_min = node->bounds_.min;
    bb_max = node->bounds_.max;
  }

  float co_dummy[3], depth;
  node->tmin_ = dist_squared_ray_to_aabb_v3(
      &dist_ray_to_aabb_precalc, bb_min, bb_max, co_dummy, &depth);
  /* Ideally we would skip distances outside the range. */
  return depth > 0.0f;
}

void find_nearest_to_ray(Tree &pbvh,
                         const FunctionRef<void(Node &node, float *tmin)> fn,
                         const float ray_start[3],
                         const float ray_normal[3],
                         const bool original)
{
  const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(ray_start,
                                                                                   ray_normal);

  search_callback_occluded(
      pbvh,
      [&](Node &node) { return nearest_to_ray_aabb_dist_sq(&node, ray_dist_precalc, original); },
      fn);
}

static bool pbvh_faces_node_nearest_to_ray(Tree &pbvh,
                                           const Node &node,
                                           const float (*origco)[3],
                                           const Span<int> corner_verts,
                                           const Span<int3> corner_tris,
                                           const Span<int> corner_tri_faces,
                                           const Span<bool> hide_poly,
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  using namespace blender;
  const Span<float3> positions = pbvh.vert_positions_;
  bool hit = false;

  for (const int i : node.prim_indices_.index_range()) {
    const int tri_i = node.prim_indices_[i];
    const int3 &corner_tri = corner_tris[tri_i];
    const int3 face_verts = node.face_vert_indices_[i];

    if (!hide_poly.is_empty() && hide_poly[corner_tri_faces[tri_i]]) {
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
                                  positions[corner_verts[corner_tri[0]]],
                                  positions[corner_verts[corner_tri[1]]],
                                  positions[corner_verts[corner_tri[2]]],
                                  depth,
                                  dist_sq);
    }
  }

  return hit;
}

static bool pbvh_grids_node_nearest_to_ray(Tree &pbvh,
                                           Node &node,
                                           const float (*origco)[3],
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *depth,
                                           float *dist_sq)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
  const int totgrid = node.prim_indices_.size();
  const int gridsize = key.grid_size;
  bool hit = false;
  const BitGroupVector<> &grid_hidden = pbvh.subdiv_ccg_->grid_hidden;
  const Span<CCGElem *> grids = pbvh.subdiv_ccg_->grids;

  for (int i = 0; i < totgrid; i++) {
    CCGElem *grid = grids[node.prim_indices_[i]];
    if (!grid) {
      continue;
    }

    for (int y = 0; y < gridsize - 1; y++) {
      for (int x = 0; x < gridsize - 1; x++) {
        /* check if grid face is hidden */
        if (!grid_hidden.is_empty()) {
          if (paint_is_grid_face_hidden(grid_hidden[node.prim_indices_[i]], gridsize, x, y)) {
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
                                       CCG_grid_elem_co(key, grid, x, y),
                                       CCG_grid_elem_co(key, grid, x + 1, y),
                                       CCG_grid_elem_co(key, grid, x + 1, y + 1),
                                       CCG_grid_elem_co(key, grid, x, y + 1),
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

bool find_nearest_to_ray_node(Tree &pbvh,
                              Node &node,
                              const float (*origco)[3],
                              bool use_origco,
                              const Span<int> corner_verts,
                              const Span<int3> corner_tris,
                              const Span<int> corner_tri_faces,
                              const Span<bool> hide_poly,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq)
{
  bool hit = false;

  if (node.flag_ & PBVH_FullyHidden) {
    return false;
  }

  switch (pbvh.type()) {
    case Type::Mesh:
      hit |= pbvh_faces_node_nearest_to_ray(pbvh,
                                            node,
                                            origco,
                                            corner_verts,
                                            corner_tris,
                                            corner_tri_faces,
                                            hide_poly,
                                            ray_start,
                                            ray_normal,
                                            depth,
                                            dist_sq);
      break;
    case Type::Grids:
      hit |= pbvh_grids_node_nearest_to_ray(
          pbvh, node, origco, ray_start, ray_normal, depth, dist_sq);
      break;
    case Type::BMesh:
      hit = bmesh_node_nearest_to_ray(node, ray_start, ray_normal, depth, dist_sq, use_origco);
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
static PlaneAABBIsect test_frustum_aabb(const Bounds<float3> &bounds,
                                        const PBVHFrustumPlanes *frustum)
{
  PlaneAABBIsect ret = ISECT_INSIDE;
  const float(*planes)[4] = frustum->planes;

  for (int i = 0; i < frustum->num_planes; i++) {
    float vmin[3], vmax[3];

    for (int axis = 0; axis < 3; axis++) {
      if (planes[i][axis] < 0) {
        vmin[axis] = bounds.min[axis];
        vmax[axis] = bounds.max[axis];
      }
      else {
        vmin[axis] = bounds.max[axis];
        vmax[axis] = bounds.min[axis];
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

}  // namespace blender::bke::pbvh

bool BKE_pbvh_node_frustum_contain_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *data)
{
  return blender::bke::pbvh::test_frustum_aabb(node->bounds_, data) !=
         blender::bke::pbvh::ISECT_OUTSIDE;
}

bool BKE_pbvh_node_frustum_exclude_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *data)
{
  return blender::bke::pbvh::test_frustum_aabb(node->bounds_, data) !=
         blender::bke::pbvh::ISECT_INSIDE;
}

static blender::draw::pbvh::PBVH_GPU_Args pbvh_draw_args_init(const Mesh &mesh,
                                                              blender::bke::pbvh::Tree &pbvh,
                                                              const blender::bke::pbvh::Node &node)
{
  /* TODO: Use an explicit argument for the original mesh to avoid relying on
   * #Tree::mesh. */
  blender::draw::pbvh::PBVH_GPU_Args args{};

  args.pbvh_type = pbvh.type();

  /* Occasionally, the evaluated and original meshes are out of sync. Prefer using the pbvh mesh in
   * these cases. See #115856 and #121008 */
  args.face_sets_color_default = pbvh.mesh_ ? pbvh.mesh_->face_sets_color_default :
                                              mesh.face_sets_color_default;
  args.face_sets_color_seed = pbvh.mesh_ ? pbvh.mesh_->face_sets_color_seed :
                                           mesh.face_sets_color_seed;

  args.active_color = pbvh.mesh_ ? pbvh.mesh_->active_color_attribute :
                                   mesh.active_color_attribute;
  args.render_color = pbvh.mesh_ ? pbvh.mesh_->default_color_attribute :
                                   mesh.default_color_attribute;

  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Mesh:
      args.vert_data = &mesh.vert_data;
      args.corner_data = &mesh.corner_data;
      args.face_data = &mesh.face_data;
      args.mesh = pbvh.mesh_;
      args.vert_positions = pbvh.vert_positions_;
      args.corner_verts = mesh.corner_verts();
      args.corner_edges = mesh.corner_edges();
      args.corner_tris = mesh.corner_tris();
      args.vert_normals = pbvh.vert_normals_;
      args.face_normals = pbvh.face_normals_;
      /* Retrieve data from the original mesh. Ideally that would be passed to this function to
       * make it clearer when each is used. */
      args.hide_poly = *pbvh.mesh_->attributes().lookup<bool>(".hide_poly",
                                                              blender::bke::AttrDomain::Face);

      args.prim_indices = node.prim_indices_;
      args.tri_faces = mesh.corner_tri_faces();
      break;
    case blender::bke::pbvh::Type::Grids:
      args.vert_data = &pbvh.mesh_->vert_data;
      args.corner_data = &pbvh.mesh_->corner_data;
      args.face_data = &pbvh.mesh_->face_data;
      args.ccg_key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
      args.mesh = pbvh.mesh_;
      args.grid_indices = node.prim_indices_;
      args.subdiv_ccg = pbvh.subdiv_ccg_;
      args.grids = pbvh.subdiv_ccg_->grids;
      args.vert_normals = pbvh.vert_normals_;
      break;
    case blender::bke::pbvh::Type::BMesh:
      args.bm = pbvh.bm_;
      args.vert_data = &args.bm->vdata;
      args.corner_data = &args.bm->ldata;
      args.face_data = &args.bm->pdata;
      args.bm_faces = &node.bm_faces_;
      args.cd_mask_layer = CustomData_get_offset_named(
          &pbvh.bm_->vdata, CD_PROP_FLOAT, ".sculpt_mask");

      break;
  }

  return args;
}

namespace blender::bke::pbvh {

static void node_update_draw_buffers(const Mesh &mesh, Tree &pbvh, Node &node)
{
  /* Create and update draw buffers. The functions called here must not
   * do any OpenGL calls. Flags are not cleared immediately, that happens
   * after GPU_pbvh_buffer_flush() which does the final OpenGL calls. */
  if (node.flag_ & PBVH_RebuildDrawBuffers) {
    const blender::draw::pbvh::PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, node);
    node.draw_batches_ = blender::draw::pbvh::node_create(args);
  }

  if (node.flag_ & PBVH_UpdateDrawBuffers) {
    node.debug_draw_gen_++;

    if (node.draw_batches_) {
      const blender::draw::pbvh::PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, node);
      blender::draw::pbvh::node_update(node.draw_batches_, args);
    }
  }
}

void free_draw_buffers(Tree & /*pbvh*/, Node *node)
{
  if (node->draw_batches_) {
    draw::pbvh::node_free(node->draw_batches_);
    node->draw_batches_ = nullptr;
  }
}

static void pbvh_update_draw_buffers(const Mesh &mesh,
                                     Tree &pbvh,
                                     Span<Node *> nodes,
                                     int update_flag)
{
  if (pbvh.type() == Type::BMesh && !pbvh.bm_) {
    /* BMesh hasn't been created yet */
    return;
  }

  if ((update_flag & PBVH_RebuildDrawBuffers) || ELEM(pbvh.type(), Type::Grids, Type::BMesh)) {
    /* Free buffers uses OpenGL, so not in parallel. */
    for (Node *node : nodes) {
      if (node->flag_ & PBVH_RebuildDrawBuffers) {
        free_draw_buffers(pbvh, node);
      }
      else if ((node->flag_ & PBVH_UpdateDrawBuffers) && node->draw_batches_) {
        const draw::pbvh::PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, *node);
        draw::pbvh::update_pre(node->draw_batches_, args);
      }
    }
  }

  /* Parallel creation and update of draw buffers. */
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (Node *node : nodes.slice(range)) {
      node_update_draw_buffers(mesh, pbvh, *node);
    }
  });

  /* Flush buffers uses OpenGL, so not in parallel. */
  for (Node *node : nodes) {
    if (node->flag_ & PBVH_UpdateDrawBuffers) {

      if (node->draw_batches_) {
        draw::pbvh::node_gpu_flush(node->draw_batches_);
      }
    }

    node->flag_ &= ~(PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers);
  }
}

void draw_cb(const Mesh &mesh,
             Tree &pbvh,
             bool update_only_visible,
             const PBVHFrustumPlanes &update_frustum,
             const PBVHFrustumPlanes &draw_frustum,
             const FunctionRef<void(draw::pbvh::PBVHBatches *batches,
                                    const draw::pbvh::PBVH_GPU_Args &args)> draw_fn)
{
  if (update_only_visible) {
    int update_flag = 0;
    Vector<Node *> nodes = search_gather(pbvh, [&](Node &node) {
      if (!BKE_pbvh_node_frustum_contain_AABB(&node, &update_frustum)) {
        return false;
      }
      update_flag |= node.flag_;
      return true;
    });
    if (update_flag & (PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers)) {
      pbvh_update_draw_buffers(mesh, pbvh, nodes, update_flag);
    }
  }
  else {
    /* Get all nodes with draw updates, also those outside the view. */
    Vector<Node *> nodes = search_gather(pbvh, [&](Node &node) {
      return update_search(&node, PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers);
    });
    pbvh_update_draw_buffers(mesh, pbvh, nodes, PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers);
  }

  /* Draw visible nodes. */
  Vector<Node *> nodes = search_gather(
      pbvh, [&](Node &node) { return BKE_pbvh_node_frustum_contain_AABB(&node, &draw_frustum); });

  for (Node *node : nodes) {
    if (node->flag_ & PBVH_FullyHidden) {
      continue;
    }
    if (!node->draw_batches_) {
      continue;
    }
    const draw::pbvh::PBVH_GPU_Args args = pbvh_draw_args_init(mesh, pbvh, *node);
    draw_fn(node->draw_batches_, args);
  }
}

}  // namespace blender::bke::pbvh

void BKE_pbvh_draw_debug_cb(blender::bke::pbvh::Tree &pbvh,
                            void (*draw_fn)(blender::bke::pbvh::Node *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data)
{
  PBVHNodeFlags flag = PBVH_Leaf;

  for (blender::bke::pbvh::Node &node : pbvh.nodes_) {
    if (node.flag_ & PBVH_TexLeaf) {
      flag = PBVH_TexLeaf;
      break;
    }
  }

  for (blender::bke::pbvh::Node &node : pbvh.nodes_) {
    if (!(node.flag_ & flag)) {
      continue;
    }

    draw_fn(&node, user_data, node.bounds_.min, node.bounds_.max, node.flag_);
  }
}

void BKE_pbvh_vert_coords_apply(blender::bke::pbvh::Tree &pbvh,
                                const blender::Span<blender::float3> vert_positions)
{
  using namespace blender::bke::pbvh;

  if (!pbvh.deformed_) {
    if (!pbvh.vert_positions_.is_empty()) {
      /* When the Tree is deformed, it creates a separate vertex position array
       * that it owns directly. Conceptually these copies often aren't and often adds extra
       * indirection, but:
       *  - Sculpting shape keys, the deformations are flushed back to the keys as a separate step.
       *  - Sculpting on a deformed mesh, deformations are also flushed to original positions
       *    separately.
       *  - The Tree currently always assumes we want to change positions, and
       * has no way to avoid calculating normals if it's only used for painting, for example. */
      pbvh.vert_positions_deformed_ = pbvh.vert_positions_.as_span();
      pbvh.vert_positions_ = pbvh.vert_positions_deformed_;

      pbvh.vert_normals_deformed_ = pbvh.vert_normals_;
      pbvh.vert_normals_ = pbvh.vert_normals_deformed_;

      pbvh.face_normals_deformed_ = pbvh.face_normals_;
      pbvh.face_normals_ = pbvh.face_normals_deformed_;

      pbvh.deformed_ = true;
    }
  }

  if (!pbvh.vert_positions_.is_empty()) {
    blender::MutableSpan<blender::float3> positions = pbvh.vert_positions_;
    positions.copy_from(vert_positions);

    for (Node &node : pbvh.nodes_) {
      BKE_pbvh_node_mark_positions_update(&node);
    }

    update_bounds(pbvh);
    store_bounds_orig(pbvh);
  }
}

bool BKE_pbvh_is_deformed(const blender::bke::pbvh::Tree &pbvh)
{
  return pbvh.deformed_;
}
/* Proxies */

void pbvh_vertex_iter_init(blender::bke::pbvh::Tree &pbvh,
                           blender::bke::pbvh::Node *node,
                           PBVHVertexIter *vi,
                           int mode)
{
  vi->grid = nullptr;
  vi->no = nullptr;
  vi->fno = nullptr;
  vi->vert_positions = {};
  vi->vertex.i = 0LL;

  int uniq_verts;
  int totvert;
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Grids:
      totvert = node->prim_indices_.size() *
                BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_).grid_area;
      uniq_verts = totvert;
      break;
    case blender::bke::pbvh::Type::Mesh:
      totvert = node->vert_indices_.size();
      uniq_verts = node->unique_verts_num_;
      break;
    case blender::bke::pbvh::Type::BMesh:
      totvert = node->bm_unique_verts_.size() + node->bm_other_verts_.size();
      uniq_verts = node->bm_unique_verts_.size();
      break;
  }

  if (pbvh.type() == blender::bke::pbvh::Type::Grids) {
    vi->key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);
    vi->grids = pbvh.subdiv_ccg_->grids.data();
    vi->grid_indices = node->prim_indices_.data();
    vi->totgrid = node->prim_indices_.size();
    vi->gridsize = vi->key.grid_size;
  }
  else {
    vi->key = {};
    vi->grids = nullptr;
    vi->grid_indices = nullptr;
    vi->totgrid = 1;
    vi->gridsize = 0;
  }

  if (mode == PBVH_ITER_ALL) {
    vi->totvert = totvert;
  }
  else {
    vi->totvert = uniq_verts;
  }
  vi->vert_indices = node->vert_indices_.data();
  vi->vert_positions = pbvh.vert_positions_;
  vi->is_mesh = !pbvh.vert_positions_.is_empty();

  if (pbvh.type() == blender::bke::pbvh::Type::BMesh) {
    vi->bm_unique_verts = node->bm_unique_verts_.begin();
    vi->bm_unique_verts_end = node->bm_unique_verts_.end();
    vi->bm_other_verts = node->bm_other_verts_.begin();
    vi->bm_other_verts_end = node->bm_other_verts_.end();
    vi->bm_vdata = &pbvh.bm_->vdata;
    vi->cd_vert_mask_offset = CustomData_get_offset_named(
        vi->bm_vdata, CD_PROP_FLOAT, ".sculpt_mask");
  }

  vi->gh.reset();
  if (vi->grids && mode == PBVH_ITER_UNIQUE) {
    vi->grid_hidden = pbvh.subdiv_ccg_->grid_hidden.is_empty() ? nullptr :
                                                                 &pbvh.subdiv_ccg_->grid_hidden;
  }

  vi->mask = 0.0f;
  if (pbvh.type() == blender::bke::pbvh::Type::Mesh) {
    vi->vert_normals = pbvh.vert_normals_;
    vi->hide_vert = static_cast<const bool *>(
        CustomData_get_layer_named(&pbvh.mesh_->vert_data, CD_PROP_BOOL, ".hide_vert"));
    vi->vmask = static_cast<const float *>(
        CustomData_get_layer_named(&pbvh.mesh_->vert_data, CD_PROP_FLOAT, ".sculpt_mask"));
  }
}

bool pbvh_has_mask(const blender::bke::pbvh::Tree &pbvh)
{
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Grids:
      return BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_).has_mask;
    case blender::bke::pbvh::Type::Mesh:
      return pbvh.mesh_->attributes().contains(".sculpt_mask");
    case blender::bke::pbvh::Type::BMesh:
      return pbvh.bm_ &&
             CustomData_has_layer_named(&pbvh.bm_->vdata, CD_PROP_FLOAT, ".sculpt_mask");
  }

  return false;
}

bool pbvh_has_face_sets(blender::bke::pbvh::Tree &pbvh)
{
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Grids:
    case blender::bke::pbvh::Type::Mesh:
      return pbvh.mesh_->attributes().contains(".sculpt_face_set");
    case blender::bke::pbvh::Type::BMesh:
      return CustomData_has_layer_named(&pbvh.bm_->pdata, CD_PROP_FLOAT, ".sculpt_mask");
  }
  return false;
}

namespace blender::bke::pbvh {

void set_frustum_planes(Tree &pbvh, PBVHFrustumPlanes *planes)
{
  pbvh.num_planes_ = planes->num_planes;
  for (int i = 0; i < pbvh.num_planes_; i++) {
    copy_v4_v4(pbvh.planes_[i], planes->planes[i]);
  }
}

void get_frustum_planes(const Tree &pbvh, PBVHFrustumPlanes *planes)
{
  planes->num_planes = pbvh.num_planes_;
  for (int i = 0; i < planes->num_planes; i++) {
    copy_v4_v4(planes->planes[i], pbvh.planes_[i]);
  }
}

}  // namespace blender::bke::pbvh

Mesh *BKE_pbvh_get_mesh(blender::bke::pbvh::Tree &pbvh)
{
  return pbvh.mesh_;
}

blender::Span<blender::float3> BKE_pbvh_get_vert_positions(const blender::bke::pbvh::Tree &pbvh)
{
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Mesh);
  return pbvh.vert_positions_;
}

blender::MutableSpan<blender::float3> BKE_pbvh_get_vert_positions(blender::bke::pbvh::Tree &pbvh)
{
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Mesh);
  return pbvh.vert_positions_;
}

blender::Span<blender::float3> BKE_pbvh_get_vert_normals(const blender::bke::pbvh::Tree &pbvh)
{
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Mesh);
  return pbvh.vert_normals_;
}

void BKE_pbvh_subdiv_cgg_set(blender::bke::pbvh::Tree &pbvh, SubdivCCG *subdiv_ccg)
{
  pbvh.subdiv_ccg_ = subdiv_ccg;
}

void BKE_pbvh_ensure_node_face_corners(blender::bke::pbvh::Tree &pbvh,
                                       const blender::Span<blender::int3> corner_tris)
{
  using namespace blender;
  BLI_assert(pbvh.type() == blender::bke::pbvh::Type::Mesh);

  int totloop = 0;

  /* Check if nodes already have loop indices. */
  for (blender::bke::pbvh::Node &node : pbvh.nodes_) {
    if (!(node.flag_ & PBVH_Leaf)) {
      continue;
    }

    if (!node.corner_indices_.is_empty()) {
      return;
    }

    totloop += node.prim_indices_.size() * 3;
  }

  BLI_bitmap *visit = BLI_BITMAP_NEW(totloop, __func__);

  /* Create loop indices from node loop triangles. */
  Vector<int> corner_indices;
  for (blender::bke::pbvh::Node &node : pbvh.nodes_) {
    if (!(node.flag_ & PBVH_Leaf)) {
      continue;
    }

    corner_indices.clear();

    for (const int i : node.prim_indices_) {
      const int3 &tri = corner_tris[i];

      for (int k = 0; k < 3; k++) {
        if (!BLI_BITMAP_TEST(visit, tri[k])) {
          corner_indices.append(tri[k]);
          BLI_BITMAP_ENABLE(visit, tri[k]);
        }
      }
    }

    node.corner_indices_ = corner_indices.as_span();
  }

  MEM_SAFE_FREE(visit);
}

int BKE_pbvh_debug_draw_gen_get(blender::bke::pbvh::Node &node)
{
  return node.debug_draw_gen_;
}

void BKE_pbvh_sync_visibility_from_verts(blender::bke::pbvh::Tree &pbvh, Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Mesh: {
      mesh_hide_vert_flush(*mesh);
      break;
    }
    case blender::bke::pbvh::Type::BMesh: {
      BMIter iter;
      BMVert *v;
      BMEdge *e;
      BMFace *f;

      BM_ITER_MESH (f, &iter, pbvh.bm_, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (e, &iter, pbvh.bm_, BM_EDGES_OF_MESH) {
        BM_elem_flag_disable(e, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (v, &iter, pbvh.bm_, BM_VERTS_OF_MESH) {
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
    case blender::bke::pbvh::Type::Grids: {
      const OffsetIndices faces = mesh->faces();
      const BitGroupVector<> &grid_hidden = pbvh.subdiv_ccg_->grid_hidden;
      CCGKey key = BKE_subdiv_ccg_key_top_level(*pbvh.subdiv_ccg_);

      IndexMaskMemory memory;
      const IndexMask hidden_faces =
          !grid_hidden.is_empty() ?
              IndexMask::from_predicate(faces.index_range(),
                                        GrainSize(1024),
                                        memory,
                                        [&](const int i) {
                                          const IndexRange face = faces[i];
                                          return std::any_of(
                                              face.begin(), face.end(), [&](const int corner) {
                                                return grid_hidden[corner][key.grid_area - 1];
                                              });
                                        }) :
              IndexMask();

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
      break;
    }
  }
}

namespace blender::bke::pbvh {
Vector<Node *> search_gather(Tree &pbvh,
                             const FunctionRef<bool(Node &)> scb,
                             PBVHNodeFlags leaf_flag)
{
  if (pbvh.nodes_.is_empty()) {
    return {};
  }

  PBVHIter iter;
  Vector<Node *> nodes;

  pbvh_iter_begin(&iter, pbvh, scb);

  Node *node;
  while ((node = pbvh_iter_next(&iter, leaf_flag))) {
    if (node->flag_ & leaf_flag) {
      nodes.append(node);
    }
  }

  pbvh_iter_end(&iter);
  return nodes;
}

}  // namespace blender::bke::pbvh
