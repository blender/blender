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
#include "BLI_stack.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_time.h"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph_query.hh"

#include "bmesh.hh"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

namespace blender::bke::pbvh {

// #define DEBUG_BUILD_TIME

#define STACK_FIXED_DEPTH 100

/** Create invalid bounds for use with #math::min_max. */
static Bounds<float3> negative_bounds()
{
  return {float3(std::numeric_limits<float>::max()), float3(std::numeric_limits<float>::lowest())};
}

static Bounds<float3> merge_bounds(const Bounds<float3> &a, const Bounds<float3> &b)
{
  return bounds::merge(a, b);
}

static int partition_along_axis(const Span<float3> face_centers,
                                MutableSpan<int> faces,
                                const int axis,
                                const float middle)
{
  const int *split = std::partition(faces.begin(), faces.end(), [&](const int face) {
    return face_centers[face][axis] >= middle;
  });
  return split - faces.begin();
}

static int partition_material_indices(const Span<int> material_indices, MutableSpan<int> faces)
{
  const int first = material_indices[faces.first()];
  const int *split = std::partition(
      faces.begin(), faces.end(), [&](const int face) { return material_indices[face] == first; });
  return split - faces.begin();
}

BLI_NOINLINE static void build_mesh_leaf_nodes(const int verts_num,
                                               const OffsetIndices<int> faces,
                                               const Span<int> corner_verts,
                                               MutableSpan<MeshNode> nodes)
{
#ifdef DEBUG_BUILD_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif
  Array<Array<int>> verts_per_node(nodes.size(), NoInitialization());
  threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
    Set<int> verts;
    for (const int i : range) {
      MeshNode &node = nodes[i];

      verts.clear();
      int corners_count = 0;
      for (const int face_index : node.face_indices_) {
        const IndexRange face = faces[face_index];
        verts.add_multiple(corner_verts.slice(face));
        corners_count += face.size();
      }
      nodes[i].corners_num_ = corners_count;

      new (&verts_per_node[i]) Array<int>(verts.size());
      std::copy(verts.begin(), verts.end(), verts_per_node[i].begin());
      std::sort(verts_per_node[i].begin(), verts_per_node[i].end());
    }
  });

  Vector<int> owned_verts;
  Vector<int> shared_verts;
  BitVector<> vert_used(verts_num);
  for (const int i : nodes.index_range()) {
    MeshNode &node = nodes[i];

    owned_verts.clear();
    shared_verts.clear();
    for (const int vert : verts_per_node[i]) {
      if (vert_used[vert]) {
        shared_verts.append(vert);
      }
      else {
        vert_used[vert].set();
        owned_verts.append(vert);
      }
    }
    node.unique_verts_num_ = owned_verts.size();
    node.vert_indices_.reserve(owned_verts.size() + shared_verts.size());
    node.vert_indices_.add_multiple(owned_verts);
    node.vert_indices_.add_multiple(shared_verts);
  }
}

static bool leaf_needs_material_split(const Span<int> faces, const Span<int> material_indices)
{
  if (material_indices.is_empty()) {
    return false;
  }
  const int first = material_indices[faces.first()];
  return std::any_of(
      faces.begin(), faces.end(), [&](const int face) { return material_indices[face] != first; });
}

static void build_nodes_recursive_mesh(const Span<int> material_indices,
                                       const int leaf_limit,
                                       const int node_index,
                                       const std::optional<Bounds<float3>> &bounds_precalc,
                                       const Span<float3> face_centers,
                                       const int depth,
                                       MutableSpan<int> faces,
                                       Vector<MeshNode> &nodes)
{
  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = faces.size() <= leaf_limit || depth >= STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(faces, material_indices)) {
      MeshNode &node = nodes[node_index];
      node.flag_ |= Node::Leaf;
      node.face_indices_ = faces;
      return;
    }
  }

  /* Add two child nodes */
  nodes[node_index].children_offset_ = nodes.size();
  nodes.resize(nodes.size() + 2);

  int split;
  if (!below_leaf_limit) {
    Bounds<float3> bounds;
    if (bounds_precalc) {
      bounds = *bounds_precalc;
    }
    else {
      bounds = threading::parallel_reduce(
          faces.index_range(),
          1024,
          negative_bounds(),
          [&](const IndexRange range, Bounds<float3> value) {
            for (const int face : faces.slice(range)) {
              math::min_max(face_centers[face], value.min, value.max);
            }
            return value;
          },
          merge_bounds);
    }
    const int axis = math::dominant_axis(bounds.max - bounds.min);

    /* Partition primitives along that axis */
    split = partition_along_axis(
        face_centers, faces, axis, math::midpoint(bounds.min[axis], bounds.max[axis]));
  }
  else {
    /* Partition primitives by material */
    split = partition_material_indices(material_indices, faces);
  }

  /* Build children */
  build_nodes_recursive_mesh(material_indices,
                             leaf_limit,
                             nodes[node_index].children_offset_,
                             std::nullopt,
                             face_centers,
                             depth + 1,
                             faces.take_front(split),
                             nodes);
  build_nodes_recursive_mesh(material_indices,
                             leaf_limit,
                             nodes[node_index].children_offset_ + 1,
                             std::nullopt,
                             face_centers,
                             depth + 1,
                             faces.drop_front(split),
                             nodes);
}

inline Bounds<float3> calc_face_bounds(const Span<float3> vert_positions,
                                       const Span<int> face_verts)
{
  Bounds<float3> bounds{vert_positions[face_verts.first()]};
  for (const int vert : face_verts.slice(1, face_verts.size() - 1)) {
    math::min_max(vert_positions[vert], bounds.min, bounds.max);
  }
  return bounds;
}

Tree Tree::from_mesh(const Mesh &mesh)
{
#ifdef DEBUG_BUILD_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif
  Tree pbvh(Type::Mesh);
  const Span<float3> vert_positions = mesh.vert_positions();
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  if (faces.is_empty()) {
    return pbvh;
  }

  constexpr int leaf_limit = 10000;
  static_assert(leaf_limit < std::numeric_limits<MeshNode::LocalVertMapIndexT>::max());

  Array<float3> face_centers(faces.size());
  const Bounds<float3> bounds = threading::parallel_reduce(
      faces.index_range(),
      1024,
      negative_bounds(),
      [&](const IndexRange range, const Bounds<float3> &init) {
        Bounds<float3> current = init;
        for (const int face : range) {
          const Bounds<float3> bounds = calc_face_bounds(vert_positions,
                                                         corner_verts.slice(faces[face]));
          face_centers[face] = bounds.center();
          current = bounds::merge(current, bounds);
        }
        return current;
      },
      merge_bounds);

  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
  const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);

  pbvh.prim_indices_.reinitialize(faces.size());
  array_utils::fill_index_range<int>(pbvh.prim_indices_);

  Vector<MeshNode> &nodes = std::get<Vector<MeshNode>>(pbvh.nodes_);
  nodes.resize(1);
  {
#ifdef DEBUG_BUILD_TIME
    SCOPED_TIMER_AVERAGED("build_nodes_recursive_mesh");
#endif
    build_nodes_recursive_mesh(
        material_index, leaf_limit, 0, bounds, face_centers, 0, pbvh.prim_indices_, nodes);
  }

  build_mesh_leaf_nodes(mesh.verts_num, faces, corner_verts, nodes);

  pbvh.tag_positions_changed(nodes.index_range());

  update_bounds_mesh(vert_positions, pbvh);
  store_bounds_orig(pbvh);

  if (!hide_vert.is_empty()) {
    threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
      for (const int i : range) {
        node_update_visibility_mesh(hide_vert, nodes[i]);
      }
    });
  }

  update_mask_mesh(mesh, nodes.index_range(), pbvh);

  return pbvh;
}

static void build_nodes_recursive_grids(const Span<int> material_indices,
                                        const int leaf_limit,
                                        const int node_index,
                                        const std::optional<Bounds<float3>> &bounds_precalc,
                                        const Span<float3> face_centers,
                                        const int depth,
                                        MutableSpan<int> faces,
                                        Vector<GridsNode> &nodes)
{
  /* Decide whether this is a leaf or not */
  const bool below_leaf_limit = faces.size() <= leaf_limit || depth >= STACK_FIXED_DEPTH - 1;
  if (below_leaf_limit) {
    if (!leaf_needs_material_split(faces, material_indices)) {
      GridsNode &node = nodes[node_index];
      node.flag_ |= Node::Leaf;
      node.prim_indices_ = faces;
      return;
    }
  }

  /* Add two child nodes */
  nodes[node_index].children_offset_ = nodes.size();
  nodes.resize(nodes.size() + 2);

  int split;
  if (!below_leaf_limit) {
    Bounds<float3> bounds;
    if (bounds_precalc) {
      bounds = *bounds_precalc;
    }
    else {
      bounds = threading::parallel_reduce(
          faces.index_range(),
          1024,
          negative_bounds(),
          [&](const IndexRange range, Bounds<float3> value) {
            for (const int face : faces.slice(range)) {
              math::min_max(face_centers[face], value.min, value.max);
            }
            return value;
          },
          merge_bounds);
    }
    const int axis = math::dominant_axis(bounds.max - bounds.min);

    /* Partition primitives along that axis */
    split = partition_along_axis(
        face_centers, faces, axis, math::midpoint(bounds.min[axis], bounds.max[axis]));
  }
  else {
    /* Partition primitives by material */
    split = partition_material_indices(material_indices, faces);
  }

  /* Build children */
  build_nodes_recursive_grids(material_indices,
                              leaf_limit,
                              nodes[node_index].children_offset_,
                              std::nullopt,
                              face_centers,
                              depth + 1,
                              faces.take_front(split),
                              nodes);
  build_nodes_recursive_grids(material_indices,
                              leaf_limit,
                              nodes[node_index].children_offset_ + 1,
                              std::nullopt,
                              face_centers,
                              depth + 1,
                              faces.drop_front(split),
                              nodes);
}

static Bounds<float3> calc_face_grid_bounds(const OffsetIndices<int> faces,
                                            const Span<float3> positions,
                                            const CCGKey &key,
                                            const int face)
{
  Bounds<float3> bounds = negative_bounds();
  for (const float3 &position : positions.slice(ccg::face_range(faces, key, face))) {
    math::min_max(position, bounds.min, bounds.max);
  }
  return bounds;
}

Tree Tree::from_grids(const Mesh &base_mesh, const SubdivCCG &subdiv_ccg)
{
#ifdef DEBUG_BUILD_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif
  Tree pbvh(Type::Grids);
  const OffsetIndices faces = base_mesh.faces();
  if (faces.is_empty()) {
    return pbvh;
  }

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;
  if (positions.is_empty()) {
    return pbvh;
  }

  const int leaf_limit = std::max(2500 / key.grid_area, 1);

  Array<float3> face_centers(faces.size());
  const Bounds<float3> bounds = threading::parallel_reduce(
      faces.index_range(),
      leaf_limit,
      negative_bounds(),
      [&](const IndexRange range, const Bounds<float3> &init) {
        Bounds<float3> current = init;
        for (const int face : range) {
          const Bounds<float3> bounds = calc_face_grid_bounds(faces, positions, key, face);
          face_centers[face] = bounds.center();
          current = bounds::merge(current, bounds);
        }
        return current;
      },
      merge_bounds);

  const AttributeAccessor attributes = base_mesh.attributes();
  const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);

  Array<int> face_indices(faces.size());
  array_utils::fill_index_range<int>(face_indices);

  Vector<GridsNode> &nodes = std::get<Vector<GridsNode>>(pbvh.nodes_);
  nodes.resize(1);
  {
#ifdef DEBUG_BUILD_TIME
    SCOPED_TIMER_AVERAGED("build_nodes_recursive_grids");
#endif
    build_nodes_recursive_grids(
        material_index, leaf_limit, 0, bounds, face_centers, 0, face_indices, nodes);
  }

  /* Convert face indices into grid indices. */
  pbvh.prim_indices_.reinitialize(faces.total_size());
  {
    int offset = 0;
    for (const int i : nodes.index_range()) {
      for (const int face : nodes[i].prim_indices_) {
        for (const int corner : faces[face]) {
          pbvh.prim_indices_[offset] = corner;
          offset++;
        }
      }
    }
  }

  /* Change the nodes to reference the BVH prim_indices array instead of the local face indices. */
  Array<int> node_grids_num(nodes.size() + 1);
  threading::parallel_for(nodes.index_range(), 16, [&](const IndexRange range) {
    for (const int i : range) {
      node_grids_num[i] = offset_indices::sum_group_sizes(faces, nodes[i].prim_indices_);
    }
  });
  const OffsetIndices<int> node_grid_offsets = offset_indices::accumulate_counts_to_offsets(
      node_grids_num);

  threading::parallel_for(nodes.index_range(), 512, [&](const IndexRange range) {
    for (const int i : range) {
      nodes[i].prim_indices_ = pbvh.prim_indices_.as_span().slice(node_grid_offsets[i]);
    }
  });

  pbvh.tag_positions_changed(nodes.index_range());

  update_bounds_grids(key, positions, pbvh);
  store_bounds_orig(pbvh);

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (!grid_hidden.is_empty()) {
    threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
      for (const int i : range) {
        node_update_visibility_grids(grid_hidden, nodes[i]);
      }
    });
  }

  update_mask_grids(subdiv_ccg, nodes.index_range(), pbvh);

  return pbvh;
}

Tree::Tree(const Type type) : type_(type)
{
  switch (type) {
    case bke::pbvh::Type::Mesh:
      nodes_ = Vector<MeshNode>();
      break;
    case bke::pbvh::Type::Grids:
      nodes_ = Vector<GridsNode>();
      break;
    case bke::pbvh::Type::BMesh:
      nodes_ = Vector<BMeshNode>();
      break;
  }
}

int Tree::nodes_num() const
{
  return std::visit([](const auto &nodes) { return nodes.size(); }, this->nodes_);
}

template<> Span<MeshNode> Tree::nodes() const
{
  return std::get<Vector<MeshNode>>(this->nodes_);
}
template<> Span<GridsNode> Tree::nodes() const
{
  return std::get<Vector<GridsNode>>(this->nodes_);
}
template<> Span<BMeshNode> Tree::nodes() const
{
  return std::get<Vector<BMeshNode>>(this->nodes_);
}
template<> MutableSpan<MeshNode> Tree::nodes()
{
  return std::get<Vector<MeshNode>>(this->nodes_);
}
template<> MutableSpan<GridsNode> Tree::nodes()
{
  return std::get<Vector<GridsNode>>(this->nodes_);
}
template<> MutableSpan<BMeshNode> Tree::nodes()
{
  return std::get<Vector<BMeshNode>>(this->nodes_);
}

Tree::~Tree()
{
  std::visit(
      [](auto &nodes) {
        for (Node &node : nodes) {
          if (node.flag_ & (Node::Leaf | Node::TexLeaf)) {
            node_pixels_free(&node);
          }
        }
      },
      this->nodes_);

  pixels_free(this);
}

void Tree::tag_positions_changed(const IndexMask &node_mask)
{
  this->bounds_dirty_.resize(std::max(this->bounds_dirty_.size(), node_mask.min_array_size()),
                             false);
  this->normals_dirty_.resize(std::max(this->normals_dirty_.size(), node_mask.min_array_size()),
                              false);
  node_mask.set_bits(this->bounds_dirty_);
  node_mask.set_bits(this->normals_dirty_);
  if (this->draw_data) {
    this->draw_data->tag_positions_changed(node_mask);
  }
}

void Tree::tag_visibility_changed(const IndexMask &node_mask)
{
  this->visibility_dirty_.resize(std::max(this->bounds_dirty_.size(), node_mask.min_array_size()),
                                 false);
  node_mask.set_bits(this->visibility_dirty_);
  if (this->draw_data) {
    this->draw_data->tag_visibility_changed(node_mask);
  }
}

void Tree::tag_topology_changed(const IndexMask &node_mask)
{
  if (this->draw_data) {
    this->draw_data->tag_topology_changed(node_mask);
  }
}

void Tree::tag_face_sets_changed(const IndexMask &node_mask)
{
  if (this->draw_data) {
    this->draw_data->tag_face_sets_changed(node_mask);
  }
}

void Tree::tag_masks_changed(const IndexMask &node_mask)
{
  if (this->draw_data) {
    this->draw_data->tag_masks_changed(node_mask);
  }
}

void Tree::tag_attribute_changed(const IndexMask &node_mask, const StringRef attribute_name)
{
  if (this->draw_data) {
    this->draw_data->tag_attribute_changed(node_mask, attribute_name);
  }
}

static bool tree_is_empty(const Tree &pbvh)
{
  return std::visit([](const auto &nodes) { return nodes.is_empty(); }, pbvh.nodes_);
}

static Node &first_node(Tree &pbvh)
{
  BLI_assert(!tree_is_empty(pbvh));
  return std::visit([](auto &nodes) -> Node & { return nodes.first(); }, pbvh.nodes_);
}

struct StackItem {
  Node *node;
  bool revisiting;
};

struct PBVHIter {
  Tree *pbvh;
  blender::FunctionRef<bool(Node &)> scb;

  Stack<StackItem, 100> stack;
};

static void pbvh_iter_begin(PBVHIter *iter, Tree &pbvh, FunctionRef<bool(Node &)> scb)
{
  iter->pbvh = &pbvh;
  iter->scb = scb;
  iter->stack.push({&first_node(pbvh), false});
}

static Node *pbvh_iter_next(PBVHIter *iter, Node::Flags leaf_flag)
{
  /* purpose here is to traverse tree, visiting child nodes before their
   * parents, this order is necessary for e.g. computing bounding boxes */

  while (!iter->stack.is_empty()) {
    StackItem item = iter->stack.pop();
    Node *node = item.node;
    bool revisiting = item.revisiting;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == nullptr) {
      return nullptr;
    }

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
    iter->stack.push({node, true});

    /* push two child nodes on the stack */
    std::visit(
        [&](auto &nodes) {
          iter->stack.push({&nodes[node->children_offset_ + 1], false});
          iter->stack.push({&nodes[node->children_offset_], false});
        },
        iter->pbvh->nodes_);
  }

  return nullptr;
}

static Node *pbvh_iter_next_occluded(PBVHIter *iter)
{
  while (!iter->stack.is_empty()) {
    StackItem item = iter->stack.pop();
    Node *node = item.node;

    /* on a mesh with no faces this can happen
     * can remove this check if we know meshes have at least 1 face */
    if (node == nullptr) {
      return nullptr;
    }

    if (iter->scb && !iter->scb(*node)) {
      continue; /* don't traverse, outside of search zone */
    }

    if (node->flag_ & Node::Leaf) {
      /* immediately hit leaf node */
      return node;
    }

    std::visit(
        [&](auto &nodes) {
          iter->stack.push({&nodes[node->children_offset_ + 1], false});
          iter->stack.push({&nodes[node->children_offset_], false});
        },
        iter->pbvh->nodes_);
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

static void search_callback_occluded(Tree &pbvh,
                                     const FunctionRef<bool(Node &)> scb,
                                     const FunctionRef<void(Node &node, float *tmin)> hit_fn)
{
  if (tree_is_empty(pbvh)) {
    return;
  }
  PBVHIter iter;
  Node *node;
  node_tree *tree = nullptr;

  pbvh_iter_begin(&iter, pbvh, scb);

  while ((node = pbvh_iter_next_occluded(&iter))) {
    if (node->flag_ & Node::Leaf) {
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

  if (tree) {
    float tmin = FLT_MAX;
    traverse_tree(tree, hit_fn, &tmin);
    free_tree(tree);
  }
}

/**
 * Logic used to test whether to use the evaluated mesh for positions.
 * \todo A deeper test of equality of topology array pointers would be better. This is kept for now
 * to avoid changing logic during a refactor.
 */
static bool mesh_topology_count_matches(const Mesh &a, const Mesh &b)
{
  return a.faces_num == b.faces_num && a.corners_num == b.corners_num &&
         a.verts_num == b.verts_num;
}

static const SharedCache<Vector<float3>> &vert_normals_cache_eval(const Object &object_orig,
                                                                  const Object &object_eval)
{
  const SculptSession &ss = *object_orig.sculpt;
  const Mesh &mesh_orig = *static_cast<const Mesh *>(object_orig.data);
  BLI_assert(bke::object::pbvh_get(object_orig)->type() == Type::Mesh);
  if (object_orig.mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
    if (const Mesh *mesh_eval = BKE_object_get_evaluated_mesh_no_subsurf(&object_eval)) {
      if (mesh_topology_count_matches(*mesh_eval, mesh_orig)) {
        return mesh_eval->runtime->vert_normals_cache;
      }
    }
    if (const Mesh *mesh_eval = BKE_object_get_mesh_deform_eval(&object_eval)) {
      return mesh_eval->runtime->vert_normals_cache;
    }
  }

  if (!ss.deform_cos.is_empty()) {
    BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
    return ss.vert_normals_deform;
  }

  return mesh_orig.runtime->vert_normals_cache;
}
static SharedCache<Vector<float3>> &vert_normals_cache_eval_for_write(Object &object_orig,
                                                                      Object &object_eval)
{
  return const_cast<SharedCache<Vector<float3>> &>(
      vert_normals_cache_eval(object_orig, object_eval));
}

static const SharedCache<Vector<float3>> &face_normals_cache_eval(const Object &object_orig,
                                                                  const Object &object_eval)
{
  const SculptSession &ss = *object_orig.sculpt;
  const Mesh &mesh_orig = *static_cast<const Mesh *>(object_orig.data);
  BLI_assert(bke::object::pbvh_get(object_orig)->type() == Type::Mesh);
  if (object_orig.mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
    if (const Mesh *mesh_eval = BKE_object_get_evaluated_mesh_no_subsurf(&object_eval)) {
      if (mesh_topology_count_matches(*mesh_eval, mesh_orig)) {
        return mesh_eval->runtime->face_normals_cache;
      }
    }
    if (const Mesh *mesh_eval = BKE_object_get_mesh_deform_eval(&object_eval)) {
      return mesh_eval->runtime->face_normals_cache;
    }
  }

  if (!ss.deform_cos.is_empty()) {
    BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
    return ss.face_normals_deform;
  }

  return mesh_orig.runtime->face_normals_cache;
}
static SharedCache<Vector<float3>> &face_normals_cache_eval_for_write(Object &object_orig,
                                                                      Object &object_eval)
{
  return const_cast<SharedCache<Vector<float3>> &>(
      face_normals_cache_eval(object_orig, object_eval));
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
                                   const Span<MeshNode> nodes,
                                   const IndexMask &nodes_to_update,
                                   MutableSpan<float3> face_normals)
{
  nodes_to_update.foreach_index(GrainSize(1), [&](const int i) {
    normals_calc_faces(positions, faces, corner_verts, nodes[i].faces(), face_normals);
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
                                   const Span<MeshNode> nodes,
                                   const IndexMask &nodes_to_update,
                                   MutableSpan<float3> vert_normals)
{
  nodes_to_update.foreach_index(GrainSize(1), [&](const int i) {
    normals_calc_verts_simple(vert_to_face_map, face_normals, nodes[i].verts(), vert_normals);
  });
}

static void update_normals_mesh(Object &object_orig,
                                Object &object_eval,
                                const Span<MeshNode> nodes,
                                const IndexMask &nodes_to_update)
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
  Mesh &mesh = *static_cast<Mesh *>(object_orig.data);
  const Span<float3> positions = bke::pbvh::vert_positions_eval_from_eval(object_eval);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();

  SharedCache<Vector<float3>> &vert_normals_cache = vert_normals_cache_eval_for_write(object_orig,
                                                                                      object_eval);
  SharedCache<Vector<float3>> &face_normals_cache = face_normals_cache_eval_for_write(object_orig,
                                                                                      object_eval);

  VectorSet<int> boundary_faces;
  nodes_to_update.foreach_index([&](const int i) {
    const MeshNode &node = nodes[i];
    for (const int vert : node.vert_indices_.as_span().drop_front(node.unique_verts_num_)) {
      boundary_faces.add_multiple(vert_to_face_map[vert]);
    }
  });

  VectorSet<int> boundary_verts;

  threading::parallel_invoke(
      [&]() {
        if (face_normals_cache.is_dirty()) {
          face_normals_cache.ensure([&](Vector<float3> &r_data) {
            r_data.resize(faces.size());
            bke::mesh::normals_calc_faces(positions, faces, corner_verts, r_data);
          });
        }
        else {
          face_normals_cache.update([&](Vector<float3> &r_data) {
            calc_node_face_normals(positions, faces, corner_verts, nodes, nodes_to_update, r_data);
            calc_boundary_face_normals(positions, faces, corner_verts, boundary_faces, r_data);
          });
        }
      },
      [&]() {
        /* Update all normals connected to affected faces, even if not explicitly tagged. */
        boundary_verts.reserve(boundary_faces.size());
        for (const int face : boundary_faces) {
          boundary_verts.add_multiple(corner_verts.slice(faces[face]));
        }
      });
  const Span<float3> face_normals = face_normals_cache.data();

  if (vert_normals_cache.is_dirty()) {
    vert_normals_cache.ensure([&](Vector<float3> &r_data) {
      r_data.resize(positions.size());
      mesh::normals_calc_verts(
          positions, faces, corner_verts, vert_to_face_map, face_normals, r_data);
    });
  }
  else {
    vert_normals_cache.update([&](Vector<float3> &r_data) {
      calc_node_vert_normals(vert_to_face_map, face_normals, nodes, nodes_to_update, r_data);
      calc_boundary_vert_normals(vert_to_face_map, face_normals, boundary_verts, r_data);
    });
  }
}

static void update_normals(Object &object_orig, Object &object_eval, Tree &pbvh)
{
  IndexMaskMemory memory;
  const IndexMask nodes_to_update = IndexMask::from_bits(pbvh.normals_dirty_, memory);

  switch (pbvh.type()) {
    case Type::Mesh: {
      update_normals_mesh(object_orig, object_eval, pbvh.nodes<MeshNode>(), nodes_to_update);
      break;
    }
    case Type::Grids: {
      SculptSession &ss = *object_orig.sculpt;
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      MutableSpan<GridsNode> nodes = pbvh.nodes<GridsNode>();
      IndexMaskMemory memory;
      const IndexMask faces_to_update = nodes_to_face_selection_grids(
          subdiv_ccg, nodes, nodes_to_update, memory);
      BKE_subdiv_ccg_update_normals(subdiv_ccg, faces_to_update);
      break;
    }
    case Type::BMesh: {
      bmesh_normals_update(pbvh, nodes_to_update);
      break;
    }
  }
  pbvh.normals_dirty_.clear_and_shrink();
}

void update_normals(const Depsgraph &depsgraph, Object &object_orig, Tree &pbvh)
{
  BLI_assert(DEG_is_original_object(&object_orig));
  Object &object_eval = *DEG_get_evaluated_object(&depsgraph, &object_orig);
  update_normals(object_orig, object_eval, pbvh);
}

void update_normals_from_eval(Object &object_eval, Tree &pbvh)
{
  /* Updating the original object's mesh normals caches is necessary because we skip dependency
   * graph updates for sculpt deformations in some cases (so the evaluated object doesn't contain
   * their result), and also because (currently) sculpt deformations skip tagging the mesh normals
   * caches dirty. */
  Object &object_orig = *DEG_get_original_object(&object_eval);
  update_normals(object_orig, object_eval, pbvh);
}

void update_node_bounds_mesh(const Span<float3> positions, MeshNode &node)
{
  Bounds<float3> bounds = negative_bounds();
  for (const int vert : node.all_verts()) {
    math::min_max(positions[vert], bounds.min, bounds.max);
  }
  node.bounds_ = bounds;
}

void update_node_bounds_grids(const int grid_area, const Span<float3> positions, GridsNode &node)
{
  Bounds<float3> bounds = negative_bounds();
  for (const int grid : node.grids()) {
    for (const float3 &position : positions.slice(bke::ccg::grid_range(grid_area, grid))) {
      math::min_max(position, bounds.min, bounds.max);
    }
  }
  node.bounds_ = bounds;
}

void update_node_bounds_bmesh(BMeshNode &node)
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

struct BoundsMergeInfo {
  Bounds<float3> bounds;
  bool update;
};

template<typename NodeT>
static BoundsMergeInfo merge_child_bounds(MutableSpan<NodeT> nodes,
                                          const BitSpan dirty,
                                          const int node_index)
{
  NodeT &node = nodes[node_index];
  if (node.flag_ & Node::Leaf) {
    const bool update = node_index < dirty.size() && dirty[node_index];
    return {node.bounds_, update};
  }

  const BoundsMergeInfo info_0 = merge_child_bounds(nodes, dirty, node.children_offset_ + 0);
  const BoundsMergeInfo info_1 = merge_child_bounds(nodes, dirty, node.children_offset_ + 1);
  const bool update = info_0.update || info_1.update;
  if (update) {
    node.bounds_ = bounds::merge(info_0.bounds, info_1.bounds);
  }
  return {node.bounds_, update};
}

void flush_bounds_to_parents(Tree &pbvh)
{
  std::visit(
      [&](auto &nodes) {
        nodes.first().bounds_ =
            merge_child_bounds(nodes.as_mutable_span(), pbvh.bounds_dirty_, 0).bounds;
      },
      pbvh.nodes_);
  pbvh.bounds_dirty_.clear_and_shrink();
}

void update_bounds_mesh(const Span<float3> vert_positions, Tree &pbvh)
{
  IndexMaskMemory memory;
  const IndexMask nodes_to_update = IndexMask::from_bits(pbvh.bounds_dirty_, memory);
  if (nodes_to_update.is_empty()) {
    return;
  }
  MutableSpan<MeshNode> nodes = pbvh.nodes<MeshNode>();
  nodes_to_update.foreach_index(
      GrainSize(1), [&](const int i) { update_node_bounds_mesh(vert_positions, nodes[i]); });
  flush_bounds_to_parents(pbvh);
}

void update_bounds_grids(const CCGKey &key, const Span<float3> positions, Tree &pbvh)
{
  IndexMaskMemory memory;
  const IndexMask nodes_to_update = IndexMask::from_bits(pbvh.bounds_dirty_, memory);
  if (nodes_to_update.is_empty()) {
    return;
  }
  MutableSpan<GridsNode> nodes = pbvh.nodes<GridsNode>();
  nodes_to_update.foreach_index(GrainSize(1), [&](const int i) {
    update_node_bounds_grids(key.grid_area, positions, nodes[i]);
  });
  flush_bounds_to_parents(pbvh);
}

void update_bounds_bmesh(const BMesh & /*bm*/, Tree &pbvh)
{
  IndexMaskMemory memory;
  const IndexMask nodes_to_update = IndexMask::from_bits(pbvh.bounds_dirty_, memory);
  if (nodes_to_update.is_empty()) {
    return;
  }
  MutableSpan<BMeshNode> nodes = pbvh.nodes<BMeshNode>();
  nodes_to_update.foreach_index(GrainSize(1),
                                [&](const int i) { update_node_bounds_bmesh(nodes[i]); });
  flush_bounds_to_parents(pbvh);
}

void update_bounds(const Depsgraph &depsgraph, const Object &object, Tree &pbvh)
{
  switch (pbvh.type()) {
    case Type::Mesh: {
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      update_bounds_mesh(positions, pbvh);
      break;
    }
    case Type::Grids: {
      const SculptSession &ss = *object.sculpt;
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      update_bounds_grids(key, subdiv_ccg.positions, pbvh);
      break;
    }
    case Type::BMesh: {
      const SculptSession &ss = *object.sculpt;
      update_bounds_bmesh(*ss.bm, pbvh);
      break;
    }
  }
}

void store_bounds_orig(Tree &pbvh)
{
  std::visit(
      [](auto &nodes) {
        threading::parallel_for(nodes.index_range(), 256, [&](const IndexRange range) {
          for (const int i : range) {
            nodes[i].bounds_orig_ = nodes[i].bounds_;
          }
        });
      },
      pbvh.nodes_);
}

void node_update_mask_mesh(const Span<float> mask, MeshNode &node)
{
  const Span<int> verts = node.all_verts();
  const bool fully_masked = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return mask[vert] == 1.0f; });
  const bool fully_unmasked = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return mask[vert] <= 0.0f; });
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, Node::FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, Node::FullyUnmasked);
}

void update_mask_mesh(const Mesh &mesh, const IndexMask &node_mask, Tree &pbvh)
{
  const MutableSpan<MeshNode> nodes = pbvh.nodes<MeshNode>();
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask", AttrDomain::Point);
  if (mask.is_empty()) {
    node_mask.foreach_index([&](const int i) {
      nodes[i].flag_ &= ~Node::FullyMasked;
      nodes[i].flag_ |= Node::FullyUnmasked;
    });
    return;
  }

  node_mask.foreach_index(GrainSize(1),
                          [&](const int i) { node_update_mask_mesh(mask, nodes[i]); });
}

void node_update_mask_grids(const CCGKey &key, const Span<float> masks, GridsNode &node)
{
  bool fully_masked = true;
  bool fully_unmasked = true;
  for (const int grid : node.grids()) {
    for (const float mask : masks.slice(bke::ccg::grid_range(key, grid))) {
      fully_masked &= mask == 1.0f;
      fully_unmasked &= mask <= 0.0f;
    }
  }
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, Node::FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, Node::FullyUnmasked);
}

void update_mask_grids(const SubdivCCG &subdiv_ccg, const IndexMask &node_mask, Tree &pbvh)
{
  const MutableSpan<GridsNode> nodes = pbvh.nodes<GridsNode>();
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  if (subdiv_ccg.masks.is_empty()) {
    node_mask.foreach_index([&](const int i) {
      nodes[i].flag_ &= ~Node::FullyMasked;
      nodes[i].flag_ |= Node::FullyUnmasked;
    });
    return;
  }

  node_mask.foreach_index(
      GrainSize(1), [&](const int i) { node_update_mask_grids(key, subdiv_ccg.masks, nodes[i]); });
}

void node_update_mask_bmesh(const int mask_offset, BMeshNode &node)
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
  SET_FLAG_FROM_TEST(node.flag_, fully_masked, Node::FullyMasked);
  SET_FLAG_FROM_TEST(node.flag_, fully_unmasked, Node::FullyUnmasked);
}

void update_mask_bmesh(const BMesh &bm, const IndexMask &node_mask, Tree &pbvh)
{
  const MutableSpan<BMeshNode> nodes = pbvh.nodes<BMeshNode>();
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (offset == -1) {
    node_mask.foreach_index([&](const int i) {
      nodes[i].flag_ &= ~Node::FullyMasked;
      nodes[i].flag_ |= Node::FullyUnmasked;
    });
    return;
  }

  node_mask.foreach_index(GrainSize(1),
                          [&](const int i) { node_update_mask_bmesh(offset, nodes[i]); });
}

void node_update_visibility_mesh(const Span<bool> hide_vert, MeshNode &node)
{
  BLI_assert(!hide_vert.is_empty());
  const Span<int> verts = node.all_verts();
  const bool fully_hidden = std::all_of(
      verts.begin(), verts.end(), [&](const int vert) { return hide_vert[vert]; });
  SET_FLAG_FROM_TEST(node.flag_, fully_hidden, Node::FullyHidden);
}

static void update_visibility_faces(const Mesh &mesh,
                                    const MutableSpan<MeshNode> nodes,
                                    const IndexMask &node_mask)
{
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
  if (hide_vert.is_empty()) {
    node_mask.foreach_index([&](const int i) { nodes[i].flag_ &= ~Node::FullyHidden; });
    return;
  }

  node_mask.foreach_index(GrainSize(1),
                          [&](const int i) { node_update_visibility_mesh(hide_vert, nodes[i]); });
}

void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, GridsNode &node)
{
  BLI_assert(!grid_hidden.is_empty());
  const bool fully_hidden = std::none_of(
      node.prim_indices_.begin(), node.prim_indices_.end(), [&](const int grid) {
        return bits::any_bit_unset(grid_hidden[grid]);
      });
  SET_FLAG_FROM_TEST(node.flag_, fully_hidden, Node::FullyHidden);
}

static void update_visibility_grids(const SubdivCCG &subdiv_ccg,
                                    const MutableSpan<GridsNode> nodes,
                                    const IndexMask &node_mask)
{
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (grid_hidden.is_empty()) {
    node_mask.foreach_index([&](const int i) { nodes[i].flag_ &= ~Node::FullyHidden; });
    return;
  }

  node_mask.foreach_index(
      GrainSize(1), [&](const int i) { node_update_visibility_grids(grid_hidden, nodes[i]); });
}

void node_update_visibility_bmesh(BMeshNode &node)
{
  const bool unique_hidden = std::all_of(
      node.bm_unique_verts_.begin(), node.bm_unique_verts_.end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  const bool other_hidden = std::all_of(
      node.bm_other_verts_.begin(), node.bm_other_verts_.end(), [&](const BMVert *vert) {
        return BM_elem_flag_test(vert, BM_ELEM_HIDDEN);
      });
  SET_FLAG_FROM_TEST(node.flag_, unique_hidden && other_hidden, Node::FullyHidden);
}

static void update_visibility_bmesh(const MutableSpan<BMeshNode> nodes, const IndexMask &node_mask)
{
  node_mask.foreach_index(GrainSize(1),
                          [&](const int i) { node_update_visibility_bmesh(nodes[i]); });
}

void update_visibility(const Object &object, Tree &pbvh)
{
  IndexMaskMemory memory;
  const IndexMask node_mask = IndexMask::from_bits(pbvh.visibility_dirty_, memory);
  if (node_mask.is_empty()) {
    return;
  }
  pbvh.visibility_dirty_.clear_and_shrink();
  switch (pbvh.type()) {
    case Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      update_visibility_faces(mesh, pbvh.nodes<MeshNode>(), node_mask);
      break;
    }
    case Type::Grids: {
      const SculptSession &ss = *object.sculpt;
      update_visibility_grids(*ss.subdiv_ccg, pbvh.nodes<GridsNode>(), node_mask);
      break;
    }
    case Type::BMesh: {
      update_visibility_bmesh(pbvh.nodes<BMeshNode>(), node_mask);
      break;
    }
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

blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(const blender::bke::pbvh::Tree &pbvh)
{
  using namespace blender;
  using namespace blender::bke::pbvh;
  if (tree_is_empty(pbvh)) {
    return {};
  }
  Bounds<float3> bounds = negative_bounds();

  PBVHIter iter;
  pbvh_iter_begin(&iter, const_cast<blender::bke::pbvh::Tree &>(pbvh), {});
  Node *node;
  while ((node = pbvh_iter_next(&iter, Node::Leaf))) {
    if (node->flag_ & Node::UpdateRedraw) {
      bounds = bounds::merge(bounds, node->bounds_);
    }
  }

  return bounds;
}

namespace blender::bke::pbvh {

IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        const Span<GridsNode> nodes,
                                        const IndexMask &nodes_mask,
                                        IndexMaskMemory &memory)
{
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  /* Using a #VectorSet for index deduplication would also work, but the performance gets much
   * worse with large selections since the loop would be single-threaded. A boolean array has an
   * overhead regardless of selection size, but that is small. */
  Array<bool> faces_to_update(subdiv_ccg.faces.size(), false);
  nodes_mask.foreach_index(GrainSize(1), [&](const int i) {
    for (const int grid : nodes[i].grids()) {
      faces_to_update[grid_to_face_map[grid]] = true;
    }
  });
  return IndexMask::from_bools(faces_to_update, memory);
}

Bounds<float3> bounds_get(const Tree &pbvh)
{
  return std::visit(
      [](auto &nodes) -> Bounds<float3> {
        if (nodes.is_empty()) {
          return float3(0);
        }
        return nodes.first().bounds_;
      },
      pbvh.nodes_);
}

}  // namespace blender::bke::pbvh

int BKE_pbvh_get_grid_num_verts(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  BLI_assert(blender::bke::object::pbvh_get(object)->type() == blender::bke::pbvh::Type::Grids);
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
  return ss.subdiv_ccg->grids_num * key.grid_area;
}

int BKE_pbvh_get_grid_num_faces(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  BLI_assert(blender::bke::object::pbvh_get(object)->type() == blender::bke::pbvh::Type::Grids);
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
  return ss.subdiv_ccg->grids_num * square_i(key.grid_size - 1);
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_update(blender::bke::pbvh::Node &node)
{
  node.flag_ |= blender::bke::pbvh::Node::RebuildPixels;
}

void BKE_pbvh_mark_rebuild_pixels(blender::bke::pbvh::Tree &pbvh)
{
  std::visit(
      [](auto &nodes) {
        for (blender::bke::pbvh::Node &node : nodes) {
          if (node.flag_ & blender::bke::pbvh::Node::Leaf) {
            node.flag_ |= blender::bke::pbvh::Node::RebuildPixels;
          }
        }
      },
      pbvh.nodes_);
}

void BKE_pbvh_node_fully_hidden_set(blender::bke::pbvh::Node &node, int fully_hidden)
{
  BLI_assert(node.flag_ & blender::bke::pbvh::Node::Leaf);

  if (fully_hidden) {
    node.flag_ |= blender::bke::pbvh::Node::FullyHidden;
  }
  else {
    node.flag_ &= ~blender::bke::pbvh::Node::FullyHidden;
  }
}

bool BKE_pbvh_node_fully_hidden_get(const blender::bke::pbvh::Node &node)
{
  return (node.flag_ & blender::bke::pbvh::Node::Leaf) &&
         (node.flag_ & blender::bke::pbvh::Node::FullyHidden);
}

void BKE_pbvh_node_fully_masked_set(blender::bke::pbvh::Node &node, int fully_masked)
{
  BLI_assert(node.flag_ & blender::bke::pbvh::Node::Leaf);

  if (fully_masked) {
    node.flag_ |= blender::bke::pbvh::Node::FullyMasked;
  }
  else {
    node.flag_ &= ~blender::bke::pbvh::Node::FullyMasked;
  }
}

bool BKE_pbvh_node_fully_masked_get(const blender::bke::pbvh::Node &node)
{
  return (node.flag_ & blender::bke::pbvh::Node::Leaf) &&
         (node.flag_ & blender::bke::pbvh::Node::FullyMasked);
}

void BKE_pbvh_node_fully_unmasked_set(blender::bke::pbvh::Node &node, int fully_masked)
{
  BLI_assert(node.flag_ & blender::bke::pbvh::Node::Leaf);

  if (fully_masked) {
    node.flag_ |= blender::bke::pbvh::Node::FullyUnmasked;
  }
  else {
    node.flag_ &= ~blender::bke::pbvh::Node::FullyUnmasked;
  }
}

bool BKE_pbvh_node_fully_unmasked_get(const blender::bke::pbvh::Node &node)
{
  return (node.flag_ & blender::bke::pbvh::Node::Leaf) &&
         (node.flag_ & blender::bke::pbvh::Node::FullyUnmasked);
}

namespace blender::bke::pbvh {

Span<int> node_face_indices_calc_grids(const SubdivCCG &subdiv_ccg,
                                       const GridsNode &node,
                                       Vector<int> &faces)
{
  faces.clear();
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  int prev_face = -1;
  for (const int grid : node.grids()) {
    const int face = grid_to_face_map[grid];
    if (face != prev_face) {
      faces.append(face);
      prev_face = face;
    }
  }
  return faces.as_span();
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

void BKE_pbvh_node_get_bm_orco_data(const blender::bke::pbvh::BMeshNode &node,
                                    blender::Span<blender::float3> &r_orig_positions,
                                    blender::Span<blender::int3> &r_orig_tris)
{
  r_orig_positions = node.orig_positions_;
  r_orig_tris = node.orig_tris_;
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
             const float3 &ray_start,
             const float3 &ray_normal,
             bool original)
{
  RaycastData rcd;

  isect_ray_aabb_v3_precalc(&rcd.ray, ray_start, ray_normal);
  rcd.original = original;

  search_callback_occluded(
      pbvh, [&](Node &node) { return ray_aabb_intersect(node, rcd); }, hit_fn);
}

bool ray_face_intersection_quad(const float3 &ray_start,
                                const IsectRayPrecalc *isect_precalc,
                                const float3 &t0,
                                const float3 &t1,
                                const float3 &t2,
                                const float3 &t3,
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

bool ray_face_intersection_tri(const float3 &ray_start,
                               const IsectRayPrecalc *isect_precalc,
                               const float3 &t0,
                               const float3 &t1,
                               const float3 &t2,
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
static float dist_squared_ray_to_tri_v3_fast(const float3 &ray_origin,
                                             const float3 &ray_direction,
                                             const float3 &v0,
                                             const float3 &v1,
                                             const float3 &v2,
                                             float3 &r_point,
                                             float *r_depth)
{
  const float *tri[3] = {v0, v1, v2};
  float dist_sq_best = FLT_MAX;
  for (int i = 0, j = 2; i < 3; j = i++) {
    float3 point_test;
    float depth_test = FLT_MAX;
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

bool ray_face_nearest_quad(const float3 &ray_start,
                           const float3 &ray_normal,
                           const float3 &t0,
                           const float3 &t1,
                           const float3 &t2,
                           const float3 &t3,
                           float *r_depth,
                           float *dist_sq)
{
  float dist_sq_test;
  float3 co;
  float depth_test;

  if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
           ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)
  {
    *dist_sq = dist_sq_test;
    *r_depth = depth_test;
    if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
             ray_start, ray_normal, t0, t2, t3, co, &depth_test)) < *dist_sq)
    {
      *dist_sq = dist_sq_test;
      *r_depth = depth_test;
    }
    return true;
  }

  return false;
}

bool ray_face_nearest_tri(const float3 &ray_start,
                          const float3 &ray_normal,
                          const float3 &t0,
                          const float3 &t1,
                          const float3 &t2,
                          float *r_depth,
                          float *dist_sq)
{
  float dist_sq_test;
  float3 co;
  float depth_test;

  if ((dist_sq_test = dist_squared_ray_to_tri_v3_fast(
           ray_start, ray_normal, t0, t1, t2, co, &depth_test)) < *dist_sq)
  {
    *dist_sq = dist_sq_test;
    *r_depth = depth_test;
    return true;
  }

  return false;
}

static void calc_mesh_intersect_data(const Span<int> corner_verts,
                                     const Span<int3> corner_tris,
                                     const float3 &ray_start,
                                     const float3 &ray_normal,
                                     const int face_index,
                                     const int tri_index,
                                     const std::array<const float *, 3> co,
                                     const float *depth,
                                     int &r_active_vertex,
                                     int &r_active_face_index,
                                     float3 &r_face_normal)

{
  float3 nearest_vertex_co(0.0f);
  normal_tri_v3(r_face_normal, co[0], co[1], co[2]);

  const float3 location = ray_start + ray_normal * *depth;
  for (int i = 0; i < co.size(); i++) {
    /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
     * uninitialized values. This stores the closest vertex in the current intersecting
     * triangle. */
    if (i == 0 ||
        len_squared_v3v3(location, co[i]) < len_squared_v3v3(location, nearest_vertex_co))
    {
      nearest_vertex_co = co[i];
      r_active_vertex = corner_verts[corner_tris[tri_index][i]];
      r_active_face_index = face_index;
    }
  }
}

bool node_raycast_mesh(const MeshNode &node,
                       const Span<float3> node_positions,
                       const Span<float3> vert_positions,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const Span<int3> corner_tris,
                       const Span<bool> hide_poly,
                       const float3 &ray_start,
                       const float3 &ray_normal,
                       IsectRayPrecalc *isect_precalc,
                       float *depth,
                       int &r_active_vertex,
                       int &r_active_face_index,
                       float3 &r_face_normal)
{
  const Span<int> face_indices = node.faces();

  bool hit = false;
  if (node_positions.is_empty()) {
    for (const int i : face_indices.index_range()) {
      const int face_i = face_indices[i];
      if (!hide_poly.is_empty() && hide_poly[face_i]) {
        continue;
      }

      for (const int tri_i : bke::mesh::face_triangles_range(faces, face_i)) {
        const int3 &tri = corner_tris[tri_i];
        const std::array<const float *, 3> co{{vert_positions[corner_verts[tri[0]]],
                                               vert_positions[corner_verts[tri[1]]],
                                               vert_positions[corner_verts[tri[2]]]}};
        if (ray_face_intersection_tri(ray_start, isect_precalc, co[0], co[1], co[2], depth)) {
          hit = true;
          calc_mesh_intersect_data(corner_verts,
                                   corner_tris,
                                   ray_start,
                                   ray_normal,
                                   face_i,
                                   tri_i,
                                   co,
                                   depth,
                                   r_active_vertex,
                                   r_active_face_index,
                                   r_face_normal);
        }
      }
    }
  }
  else {
    const MeshNode::LocalVertMap &vert_map = node.vert_indices_;
    for (const int i : face_indices.index_range()) {
      const int face_i = face_indices[i];
      if (!hide_poly.is_empty() && hide_poly[face_i]) {
        continue;
      }

      for (const int tri_i : bke::mesh::face_triangles_range(faces, face_i)) {
        const int3 &tri = corner_tris[tri_i];
        const std::array<const float *, 3> co{
            {node_positions[vert_map.index_of(corner_verts[tri[0]])],
             node_positions[vert_map.index_of(corner_verts[tri[1]])],
             node_positions[vert_map.index_of(corner_verts[tri[2]])]}};
        if (ray_face_intersection_tri(ray_start, isect_precalc, co[0], co[1], co[2], depth)) {
          hit = true;
          calc_mesh_intersect_data(corner_verts,
                                   corner_tris,
                                   ray_start,
                                   ray_normal,
                                   face_i,
                                   tri_i,
                                   co,
                                   depth,
                                   r_active_vertex,
                                   r_active_face_index,
                                   r_face_normal);
        }
      }
    }
  }

  return hit;
}

static void calc_grids_intersect_data(const float3 &ray_start,
                                      const float3 &ray_normal,
                                      const int grid,
                                      const short x,
                                      const short y,
                                      const std::array<const float *, 4> co,
                                      float *depth,
                                      SubdivCCGCoord &r_active_vertex,
                                      int &r_active_grid_index,
                                      float3 &r_face_normal)

{
  float3 nearest_vertex_co;
  normal_quad_v3(r_face_normal, co[0], co[1], co[2], co[3]);

  const float3 location = ray_start + ray_normal * *depth;

  constexpr short x_it[4] = {0, 1, 1, 0};
  constexpr short y_it[4] = {1, 1, 0, 0};

  for (int i = 0; i < co.size(); i++) {
    /* Always assign nearest_vertex_co in the first iteration to avoid comparison against
     * uninitialized values. This stores the closest vertex in the current intersecting
     * quad. */
    if (i == 0 ||
        len_squared_v3v3(location, co[i]) < len_squared_v3v3(location, nearest_vertex_co))
    {
      copy_v3_v3(nearest_vertex_co, co[i]);
      r_active_vertex = SubdivCCGCoord{grid, short(x + x_it[i]), short(y + y_it[i])};
    }
  }
  r_active_grid_index = grid;
}

bool node_raycast_grids(const SubdivCCG &subdiv_ccg,
                        GridsNode &node,
                        const Span<float3> node_positions,
                        const float3 &ray_start,
                        const float3 &ray_normal,
                        const IsectRayPrecalc *isect_precalc,
                        float *depth,
                        SubdivCCGCoord &r_active_vertex,
                        int &r_active_grid_index,
                        float3 &r_face_normal)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<int> grids = node.grids();
  const int grid_size = key.grid_size;
  bool hit = false;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  const Span<float3> positions = subdiv_ccg.positions;

  if (node_positions.is_empty()) {
    for (const int grid : grids) {
      const Span<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grid));
      for (const short y : IndexRange(grid_size - 1)) {
        for (const short x : IndexRange(grid_size - 1)) {
          if (!grid_hidden.is_empty()) {
            if (paint_is_grid_face_hidden(grid_hidden[grid], grid_size, x, y)) {
              continue;
            }
          }
          const std::array<const float *, 4> co{
              {grid_positions[CCG_grid_xy_to_index(grid_size, x, y + 1)],
               grid_positions[CCG_grid_xy_to_index(grid_size, x + 1, y + 1)],
               grid_positions[CCG_grid_xy_to_index(grid_size, x + 1, y)],
               grid_positions[CCG_grid_xy_to_index(grid_size, x, y)]}};
          if (ray_face_intersection_quad(
                  ray_start, isect_precalc, co[0], co[1], co[2], co[3], depth))
          {
            hit = true;
            calc_grids_intersect_data(ray_start,
                                      ray_normal,
                                      grid,
                                      x,
                                      y,
                                      co,
                                      depth,
                                      r_active_vertex,
                                      r_active_grid_index,
                                      r_face_normal);
          }
        }
      }
    }
  }
  else {
    for (const int i : grids.index_range()) {
      const int grid = grids[i];
      const Span<float3> grid_positions = node_positions.slice(bke::ccg::grid_range(key, i));
      for (const short y : IndexRange(grid_size - 1)) {
        for (const short x : IndexRange(grid_size - 1)) {
          if (!grid_hidden.is_empty()) {
            if (paint_is_grid_face_hidden(grid_hidden[grid], grid_size, x, y)) {
              continue;
            }
          }
          const std::array<const float *, 4> co{grid_positions[(y + 1) * grid_size + x],
                                                grid_positions[(y + 1) * grid_size + x + 1],
                                                grid_positions[y * grid_size + x + 1],
                                                grid_positions[y * grid_size + x]};
          if (ray_face_intersection_quad(
                  ray_start, isect_precalc, co[0], co[1], co[2], co[3], depth))
          {
            hit = true;
            calc_grids_intersect_data(ray_start,
                                      ray_normal,
                                      grid,
                                      x,
                                      y,
                                      co,
                                      depth,
                                      r_active_vertex,
                                      r_active_grid_index,
                                      r_face_normal);
          }
        }
      }
    }
  }

  return hit;
}

void clip_ray_ortho(
    Tree &pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
  if (tree_is_empty(pbvh)) {
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
    bb_root = BKE_pbvh_node_get_original_BB(&first_node(pbvh));
  }
  else {
    bb_root = node_bounds(first_node(pbvh));
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
                         const float3 &ray_start,
                         const float3 &ray_normal,
                         const bool original)
{
  const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(ray_start,
                                                                                   ray_normal);

  search_callback_occluded(
      pbvh,
      [&](Node &node) { return nearest_to_ray_aabb_dist_sq(&node, ray_dist_precalc, original); },
      fn);
}

static bool pbvh_faces_node_nearest_to_ray(const MeshNode &node,
                                           const Span<float3> node_positions,
                                           const Span<float3> vert_positions,
                                           const OffsetIndices<int> faces,
                                           const Span<int> corner_verts,
                                           const Span<int3> corner_tris,
                                           const Span<bool> hide_poly,
                                           const float3 &ray_start,
                                           const float3 &ray_normal,
                                           float *r_depth,
                                           float *dist_sq)
{
  const Span<int> face_indices = node.faces();

  bool hit = false;
  if (node_positions.is_empty()) {
    for (const int i : face_indices.index_range()) {
      const int face_i = face_indices[i];
      if (!hide_poly.is_empty() && hide_poly[face_i]) {
        continue;
      }

      for (const int tri_i : bke::mesh::face_triangles_range(faces, face_i)) {
        const int3 &corner_tri = corner_tris[tri_i];
        hit |= ray_face_nearest_tri(ray_start,
                                    ray_normal,
                                    vert_positions[corner_verts[corner_tri[0]]],
                                    vert_positions[corner_verts[corner_tri[1]]],
                                    vert_positions[corner_verts[corner_tri[2]]],
                                    r_depth,
                                    dist_sq);
      }
    }
  }
  else {
    const MeshNode::LocalVertMap &vert_map = node.vert_indices_;
    for (const int i : face_indices.index_range()) {
      const int face_i = face_indices[i];
      if (!hide_poly.is_empty() && hide_poly[face_i]) {
        continue;
      }

      for (const int tri_i : bke::mesh::face_triangles_range(faces, face_i)) {
        const int3 &corner_tri = corner_tris[tri_i];
        hit |= ray_face_nearest_tri(ray_start,
                                    ray_normal,
                                    node_positions[vert_map.index_of(corner_verts[corner_tri[0]])],
                                    node_positions[vert_map.index_of(corner_verts[corner_tri[1]])],
                                    node_positions[vert_map.index_of(corner_verts[corner_tri[2]])],
                                    r_depth,
                                    dist_sq);
      }
    }
  }

  return hit;
}

static bool pbvh_grids_node_nearest_to_ray(const SubdivCCG &subdiv_ccg,
                                           GridsNode &node,
                                           const Span<float3> node_positions,
                                           const float ray_start[3],
                                           const float ray_normal[3],
                                           float *r_depth,
                                           float *dist_sq)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<int> grids = node.grids();
  const int grid_size = key.grid_size;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  const Span<float3> positions = subdiv_ccg.positions;

  bool hit = false;
  if (node_positions.is_empty()) {
    for (const int grid : grids) {
      const Span<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grid));
      for (const short y : IndexRange(grid_size - 1)) {
        for (const short x : IndexRange(grid_size - 1)) {
          if (!grid_hidden.is_empty()) {
            if (paint_is_grid_face_hidden(grid_hidden[grid], grid_size, x, y)) {
              continue;
            }
          }
          hit |= ray_face_nearest_quad(
              ray_start,
              ray_normal,
              grid_positions[CCG_grid_xy_to_index(grid_size, x, y)],
              grid_positions[CCG_grid_xy_to_index(grid_size, x + 1, y)],
              grid_positions[CCG_grid_xy_to_index(grid_size, x + 1, y + 1)],
              grid_positions[CCG_grid_xy_to_index(grid_size, x, y + 1)],
              r_depth,
              dist_sq);
        }
      }
    }
  }
  else {
    for (const int i : grids.index_range()) {
      const int grid = grids[i];
      const Span<float3> grid_positions = node_positions.slice(bke::ccg::grid_range(key, i));
      for (const short y : IndexRange(grid_size - 1)) {
        for (const short x : IndexRange(grid_size - 1)) {
          if (!grid_hidden.is_empty()) {
            if (paint_is_grid_face_hidden(grid_hidden[grid], grid_size, x, y)) {
              continue;
            }
          }
          hit |= ray_face_nearest_quad(ray_start,
                                       ray_normal,
                                       grid_positions[y * grid_size + x],
                                       grid_positions[y * grid_size + x + 1],
                                       grid_positions[(y + 1) * grid_size + x + 1],
                                       grid_positions[(y + 1) * grid_size + x],
                                       r_depth,
                                       dist_sq);
        }
      }
    }
  }

  return hit;
}

bool find_nearest_to_ray_node(Tree &pbvh,
                              Node &node,
                              const Span<float3> node_positions,
                              bool use_origco,
                              const Span<float3> vert_positions,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const Span<int3> corner_tris,
                              const Span<bool> hide_poly,
                              const SubdivCCG *subdiv_ccg,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq)
{
  if (node.flag_ & Node::FullyHidden) {
    return false;
  }
  switch (pbvh.type()) {
    case Type::Mesh:
      return pbvh_faces_node_nearest_to_ray(static_cast<MeshNode &>(node),
                                            node_positions,
                                            vert_positions,
                                            faces,
                                            corner_verts,
                                            corner_tris,
                                            hide_poly,
                                            ray_start,
                                            ray_normal,
                                            depth,
                                            dist_sq);
    case Type::Grids:
      return pbvh_grids_node_nearest_to_ray(*subdiv_ccg,
                                            static_cast<GridsNode &>(node),
                                            node_positions,
                                            ray_start,
                                            ray_normal,
                                            depth,
                                            dist_sq);
    case Type::BMesh:
      return bmesh_node_nearest_to_ray(
          static_cast<BMeshNode &>(node), ray_start, ray_normal, depth, dist_sq, use_origco);
  }
  BLI_assert_unreachable();
  return false;
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
                                        const Span<float4> frustum_planes)
{
  PlaneAABBIsect ret = ISECT_INSIDE;

  for (const int i : frustum_planes.index_range()) {
    float vmin[3], vmax[3];

    for (int axis = 0; axis < 3; axis++) {
      if (frustum_planes[i][axis] < 0) {
        vmin[axis] = bounds.min[axis];
        vmax[axis] = bounds.max[axis];
      }
      else {
        vmin[axis] = bounds.max[axis];
        vmax[axis] = bounds.min[axis];
      }
    }

    if (dot_v3v3(frustum_planes[i], vmin) + frustum_planes[i][3] < 0) {
      return ISECT_OUTSIDE;
    }
    if (dot_v3v3(frustum_planes[i], vmax) + frustum_planes[i][3] <= 0) {
      ret = ISECT_INTERSECT;
    }
  }

  return ret;
}

bool node_frustum_contain_aabb(const Node &node, const Span<float4> frustum_planes)
{
  return test_frustum_aabb(node.bounds_, frustum_planes) != ISECT_OUTSIDE;
}

bool node_frustum_exclude_aabb(const Node &node, const Span<float4> frustum_planes)
{
  return test_frustum_aabb(node.bounds_, frustum_planes) != ISECT_INSIDE;
}

}  // namespace blender::bke::pbvh

void BKE_pbvh_draw_debug_cb(blender::bke::pbvh::Tree &pbvh,
                            void (*draw_fn)(blender::bke::pbvh::Node *node, void *user_data),
                            void *user_data)
{
  blender::bke::pbvh::Node::Flags flag = blender::bke::pbvh::Node::Leaf;

  std::visit(
      [&](auto &nodes) {
        for (blender::bke::pbvh::Node &node : nodes) {
          if (node.flag_ & blender::bke::pbvh::Node::TexLeaf) {
            flag = blender::bke::pbvh::Node::TexLeaf;
            break;
          }
        }

        for (blender::bke::pbvh::Node &node : nodes) {
          if (!(node.flag_ & flag)) {
            continue;
          }

          draw_fn(&node, user_data);
        }
      },
      pbvh.nodes_);
}

void BKE_pbvh_vert_coords_apply(blender::bke::pbvh::Tree &pbvh,
                                const blender::Span<blender::float3> vert_positions)
{
  using namespace blender::bke::pbvh;
  pbvh.tag_positions_changed(blender::IndexRange(pbvh.nodes_num()));
  update_bounds_mesh(vert_positions, pbvh);
  store_bounds_orig(pbvh);
}

namespace blender::bke::pbvh {

static Span<float3> vert_positions_eval(const Object &object_orig, const Object &object_eval)
{
  const SculptSession &ss = *object_orig.sculpt;
  const Mesh &mesh_orig = *static_cast<const Mesh *>(object_orig.data);
  BLI_assert(bke::object::pbvh_get(object_orig)->type() == Type::Mesh);
  if (object_orig.mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
    if (const Mesh *mesh_eval = BKE_object_get_evaluated_mesh_no_subsurf(&object_eval)) {
      if (mesh_topology_count_matches(*mesh_eval, mesh_orig)) {
        return mesh_eval->vert_positions();
      }
    }
    if (!ss.deform_cos.is_empty()) {
      BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
      return ss.deform_cos;
    }
    if (const Mesh *mesh_eval = BKE_object_get_mesh_deform_eval(&object_eval)) {
      return mesh_eval->vert_positions();
    }
  }

  if (!ss.deform_cos.is_empty()) {
    BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
    return ss.deform_cos;
  }

  return mesh_orig.vert_positions();
}
static MutableSpan<float3> vert_positions_eval_for_write(Object &object_orig, Object &object_eval)
{
  SculptSession &ss = *object_orig.sculpt;
  Mesh &mesh_orig = *static_cast<Mesh *>(object_orig.data);
  BLI_assert(bke::object::pbvh_get(object_orig)->type() == Type::Mesh);
  if (object_orig.mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
    if (const Mesh *mesh_eval = BKE_object_get_evaluated_mesh_no_subsurf(&object_eval)) {
      if (mesh_topology_count_matches(*mesh_eval, mesh_orig)) {
        Mesh *mesh_eval_mut = const_cast<Mesh *>(mesh_eval);
        return mesh_eval_mut->vert_positions_for_write();
      }
    }
    if (!ss.deform_cos.is_empty()) {
      BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
      return ss.deform_cos;
    }
    if (const Mesh *mesh_eval = BKE_object_get_mesh_deform_eval(&object_eval)) {
      Mesh *mesh_eval_mut = const_cast<Mesh *>(mesh_eval);
      return mesh_eval_mut->vert_positions_for_write();
    }
  }

  if (!ss.deform_cos.is_empty()) {
    BLI_assert(ss.deform_cos.size() == mesh_orig.verts_num);
    return ss.deform_cos;
  }

  return mesh_orig.vert_positions_for_write();
}

Span<float3> vert_positions_eval(const Depsgraph &depsgraph, const Object &object_orig)
{
  const Object &object_eval = *DEG_get_evaluated_object(&depsgraph,
                                                        &const_cast<Object &>(object_orig));
  return vert_positions_eval(object_orig, object_eval);
}

Span<float3> vert_positions_eval_from_eval(const Object &object_eval)
{
  BLI_assert(!DEG_is_original_object(&object_eval));
  const Object &object_orig = *DEG_get_original_object(&const_cast<Object &>(object_eval));
  return vert_positions_eval(object_orig, object_eval);
}

MutableSpan<float3> vert_positions_eval_for_write(const Depsgraph &depsgraph, Object &object_orig)
{
  Object &object_eval = *DEG_get_evaluated_object(&depsgraph, &object_orig);
  return vert_positions_eval_for_write(object_orig, object_eval);
}

Span<float3> vert_normals_eval(const Depsgraph &depsgraph, const Object &object_orig)
{
  const Object &object_eval = *DEG_get_evaluated_object(&depsgraph,
                                                        &const_cast<Object &>(object_orig));
  return vert_normals_cache_eval(object_orig, object_eval).data();
}

Span<float3> vert_normals_eval_from_eval(const Object &object_eval)
{
  BLI_assert(!DEG_is_original_object(&object_eval));
  Object &object_orig = *DEG_get_original_object(&const_cast<Object &>(object_eval));
  return vert_normals_cache_eval(object_orig, object_eval).data();
}

Span<float3> face_normals_eval_from_eval(const Object &object_eval)
{
  BLI_assert(!DEG_is_original_object(&object_eval));
  Object &object_orig = *DEG_get_original_object(&const_cast<Object &>(object_eval));
  return face_normals_cache_eval(object_orig, object_eval).data();
}

}  // namespace blender::bke::pbvh

int BKE_pbvh_debug_draw_gen_get(blender::bke::pbvh::Node &node)
{
  return node.debug_draw_gen_;
}

void BKE_pbvh_sync_visibility_from_verts(Object &object)
{
  using namespace blender;
  using namespace blender::bke;
  const SculptSession &ss = *object.sculpt;
  switch (object::pbvh_get(object)->type()) {
    case blender::bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      mesh_hide_vert_flush(mesh);
      break;
    }
    case blender::bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      BMIter iter;
      BMVert *v;
      BMEdge *e;
      BMFace *f;

      BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (e, &iter, &bm, BM_EDGES_OF_MESH) {
        BM_elem_flag_disable(e, BM_ELEM_HIDDEN);
      }

      BM_ITER_MESH (v, &iter, &bm, BM_VERTS_OF_MESH) {
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
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const OffsetIndices faces = mesh.faces();

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

      MutableAttributeAccessor attributes = mesh.attributes_for_write();
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

      mesh_hide_face_flush(mesh);
      break;
    }
  }
}

namespace blender::bke::pbvh {

IndexMask all_leaf_nodes(const Tree &pbvh, IndexMaskMemory &memory)
{
  return std::visit(
      [&](const auto &nodes) {
        return IndexMask::from_predicate(
            nodes.index_range(), GrainSize(1024), memory, [&](const int i) {
              return (nodes[i].flag_ & Node::Leaf) != 0;
            });
      },
      pbvh.nodes_);
}

static Vector<Node *> search_gather(Tree &pbvh,
                                    const FunctionRef<bool(Node &)> scb,
                                    Node::Flags leaf_flag)
{
  if (tree_is_empty(pbvh)) {
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

  return nodes;
}

IndexMask search_nodes(const Tree &pbvh,
                       IndexMaskMemory &memory,
                       FunctionRef<bool(const Node &)> filter_fn)
{
  Vector<Node *> nodes = search_gather(
      const_cast<Tree &>(pbvh), [&](Node &node) { return filter_fn(node); }, Node::Leaf);
  Array<int> indices(nodes.size());
  std::visit(
      [&](const auto &pbvh_nodes) {
        using VectorT = std::decay_t<decltype(pbvh_nodes)>;
        for (const int i : nodes.index_range()) {
          indices[i] = static_cast<typename VectorT::value_type *>(nodes[i]) - pbvh_nodes.data();
        }
      },
      pbvh.nodes_);
  std::sort(indices.begin(), indices.end());
  return IndexMask::from_indices(indices.as_span(), memory);
}

}  // namespace blender::bke::pbvh
