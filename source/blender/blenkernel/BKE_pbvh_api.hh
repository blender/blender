/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include <optional>
#include <string>
#include <variant>

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_bit_vector.hh"
#include "BLI_bounds_types.hh"
#include "BLI_compiler_compat.h"
#include "BLI_function_ref.hh"
#include "BLI_generic_span.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_customdata_types.h"

/* For embedding CCGKey in iterator. */
#include "BKE_ccg.hh"
#include "BKE_pbvh.hh"

#include "bmesh.hh"

struct BMLog;
struct BMesh;
struct CCGKey;
struct Depsgraph;
struct IsectRayPrecalc;
struct Mesh;
struct SubdivCCG;
struct SubdivCCGCoord;
struct Image;
struct ImageUser;
struct Object;
namespace blender {
namespace bke::pbvh {
class Node;
class Tree;
namespace pixels {
struct PBVHData;
struct NodeData;
}  // namespace pixels
}  // namespace bke::pbvh
}  // namespace blender

namespace blender::bke::pbvh {

class Tree;

/**
 * \todo Most data is public but should either be removed or become private in the future.
 * The "_" suffix means that fields shouldn't be used by consumers of the `bke::pbvh` API.
 */
class Node {
  friend Tree;

 public:
  /** Axis aligned min and max of all vertex positions in the node. */
  Bounds<float3> bounds_ = {};
  /** Bounds from the start of current brush stroke. */
  Bounds<float3> bounds_orig_ = {};

  /* For internal nodes, the offset of the children in the blender::bke::pbvh::Tree
   * 'nodes' array. */
  int children_offset_ = 0;

  /* Indicates whether this node is a leaf or not; also used for
   * marking various updates that need to be applied. */
  PBVHNodeFlags flag_ = PBVH_UpdateRedraw;

  /**
   * Used for ray-casting: how close the bounding-box is to the ray point.
   * \todo Remove and store elsewhere.
   */
  float tmin_ = 0.0f;

  /**
   * Used to flash colors of updated node bounding boxes in
   * debug draw mode (when G.debug_value / bpy.app.debug_value is 889).
   * \todo Remove and store elsewhere.
   */
  int debug_draw_gen_ = 0;

  /** \todo Move storage of image painting data to #Tree or elsewhere. */
  pixels::NodeData *pixels_ = nullptr;
};

struct MeshNode : public Node {
  /**
   * Use a 16 bit integer for the slot index type because there will always be less than
   * #leaf_limit vertices in a node.
   */
  using LocalVertMapIndexT = int16_t;
  /**
   * Global vertices are mapped to local indices with a vector set, with a specialized type in
   * order to use 32 bit integers for slot values. .
   */
  using LocalVertMap = VectorSet<int,
                                 DefaultProbingStrategy,
                                 DefaultHash<int>,
                                 DefaultEquality<int>,
                                 SimpleVectorSetSlot<int, LocalVertMapIndexT>,
                                 GuardedAllocator>;

  /** Indices into the #Mesh::faces() array. Refers to a subset of Tree::prim_indices_. */
  Span<int> face_indices_;

  /**
   * Array of indices into the mesh's vertex array. Contains the
   * indices of all vertices used by faces that are within this
   * node's bounding box.
   *
   * Note that a vertex might be used by a multiple faces, and
   * these faces might be in different leaf nodes. Such a vertex
   * will appear in the vert_indices array of each of those leaf
   * nodes.
   *
   * In order to support cases where you want access to multiple
   * nodes' vertices without duplication, the vert_indices array
   * is ordered such that the first part of the array, up to
   * index 'uniq_verts', contains "unique" vertex indices. These
   * vertices might not be truly unique to this node, but if
   * they appear in another node's vert_indices array, they will
   * be above that node's 'uniq_verts' value.
   *
   * Used for leaf nodes.
   *
   * \todo Find a way to disable the #VectorSet inline buffer.
   */
  LocalVertMap vert_indices_;
  /** The number of vertices in #vert_indices not shared with (owned by) another node. */
  int unique_verts_num_ = 0;
  /**
   * The number of corners in all of the node's referenced faces. This value can easily be
   * recalculated but currently it's faster to avoid that and store it here.
   */
  int corners_num_;

  /** Return the faces contained by the node. */
  Span<int> faces() const;
  /** Return the "unique" vertices owned by the node, excluding vertices owned by other nodes. */
  Span<int> verts() const;
  /**
   * Return all vertices used by faces in this node. The same as #verts(), with the shared
   * vertices added at the end of the array.
   */
  Span<int> all_verts() const;

  /** The number of corners in all of the node's faces. */
  int corners_num() const;
};

struct GridsNode : public Node {
  /** Multires grid indices for this node. Refers to a subset of Tree::prim_indices_. */
  Span<int> prim_indices_;

  /** Return grid indices contained by the node. */
  Span<int> grids() const;
};

struct BMeshNode : public Node {
  /* Set of pointers to the BMFaces used by this node.
   * NOTE: Type::BMesh only. Faces are always triangles
   * (dynamic topology forcibly triangulates the mesh).
   */
  Set<BMFace *, 0> bm_faces_;
  Set<BMVert *, 0> bm_unique_verts_;
  Set<BMVert *, 0> bm_other_verts_;

  /* Stores original coordinates of triangles. */
  Array<float3, 0> orig_positions_;
  Array<int3, 0> orig_tris_;
  Array<BMVert *, 0> orig_verts_;
};

class DrawCache {
 public:
  virtual ~DrawCache() = default;
  virtual void tag_positions_changed(const IndexMask &node_mask) = 0;
  virtual void tag_visibility_changed(const IndexMask &node_mask) = 0;
  virtual void tag_topology_changed(const IndexMask &node_mask) = 0;
  virtual void tag_face_sets_changed(const IndexMask &node_mask) = 0;
  virtual void tag_masks_changed(const IndexMask &node_mask) = 0;
  virtual void tag_attribute_changed(const IndexMask &node_mask, StringRef attribute_name) = 0;
};

/**
 * \todo Most data is public but should either be removed or become private in the future.
 * The "_" suffix means that fields shouldn't be used by consumers of the `bke::pbvh` API.
 */
class Tree {
  friend Node;
  Type type_;

  /** Memory backing for #Node::prim_indices. Without an inline buffer to make #Tree movable. */
  Array<int, 0> prim_indices_;

 public:
  std::variant<Vector<MeshNode>, Vector<GridsNode>, Vector<BMeshNode>> nodes_;

  /**
   * If true, the bounds for the corresponding node index is out of date.
   * \note Values are only meaningful for leaf nodes.
   * \note The vector's size may not match the size of the nodes array.
   */
  BitVector<> bounds_dirty_;

  /**
   * If true, the normals for the corresponding node index are out of date.
   * \note Values are only meaningful for leaf nodes.
   * \note The vector's size may not match the size of the nodes array.
   */
  BitVector<> normals_dirty_;

  /**
   * If true, the visibility status for the corresponding node indices are out of date.
   * \note Values are only meaningful for leaf nodes.
   * \note The vector's size may not match the size of the nodes array.
   */
  BitVector<> visibility_dirty_;

  /** \todo Remove and store elsewhere. */
  float planes_[6][4];
  int num_planes_ = 0;

  pixels::PBVHData *pixels_ = nullptr;

  std::unique_ptr<DrawCache> draw_data;

 public:
  Tree(const Tree &other) = delete;
  Tree(Tree &&other) = default;
  Tree &operator=(const Tree &other) = delete;
  Tree &operator=(Tree &&other) = default;
  ~Tree();

  /** Build a BVH tree from base mesh triangles. */
  static Tree from_mesh(const Mesh &mesh);
  /** Build a BVH tree from grids geometry. */
  static Tree from_grids(const Mesh &base_mesh, const SubdivCCG &subdiv_ccg);
  /** Build a BVH tree from a triangle BMesh. */
  static Tree from_bmesh(BMesh &bm);

  int nodes_num() const;
  template<typename NodeT> Span<NodeT> nodes() const;
  template<typename NodeT> MutableSpan<NodeT> nodes();

  Type type() const
  {
    return this->type_;
  }

  /**
   * Mark data based on positions for specific BVH nodes dirty. In particular: bounds, normals,
   * and GPU data buffers. That data is recomputed later on in functions like #update_bounds.
   * \warning Must not be called from multiple threads in parallel.
   */
  void tag_positions_changed(const IndexMask &node_mask);

  /** Tag nodes where face or vertex visibility has changed.  */
  void tag_visibility_changed(const IndexMask &node_mask);

  /**
   * Tag nodes that have a different number of vertices or faces (currently just for Type::BMesh).
   */
  void tag_topology_changed(const IndexMask &node_mask);

  /** Tag nodes where face sets have changed, causing refresh of derived data. */
  void tag_face_sets_changed(const IndexMask &node_mask);

  /** Tag nodes where mask values have changed, causing refresh of derived data. */
  void tag_masks_changed(const IndexMask &node_mask);

  /**
   * Tag nodes where generic attribute data has changed (not positions, masks, or face sets).
   */
  void tag_attribute_changed(const IndexMask &node_mask, StringRef attribute_name);

 private:
  explicit Tree(Type type);
};

}  // namespace blender::bke::pbvh

struct PBVHFrustumPlanes {
  float (*planes)[4];
  int num_planes;
};

/* Callbacks */

namespace blender::bke::pbvh {

void build_pixels(const Depsgraph &depsgraph, Object &object, Image &image, ImageUser &image_user);

/* Ray-cast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void raycast(Tree &pbvh,
             FunctionRef<void(Node &node, float *tmin)> cb,
             const float3 &ray_start,
             const float3 &ray_normal,
             bool original);

bool node_raycast_mesh(const MeshNode &node,
                       Span<float3> node_positions,
                       Span<float3> vert_positions,
                       OffsetIndices<int> faces,
                       Span<int> corner_verts,
                       Span<int3> corner_tris,
                       Span<bool> hide_poly,
                       const float3 &ray_start,
                       const float3 &ray_normal,
                       IsectRayPrecalc *isect_precalc,
                       float *depth,
                       int &r_active_vertex,
                       int &r_active_face_index,
                       float3 &r_face_normal);

bool node_raycast_grids(const SubdivCCG &subdiv_ccg,
                        GridsNode &node,
                        Span<float3> node_positions,
                        const float3 &ray_start,
                        const float3 &ray_normal,
                        const IsectRayPrecalc *isect_precalc,
                        float *depth,
                        SubdivCCGCoord &r_active_vertex,
                        int &r_active_grid_index,
                        float3 &r_face_normal);

bool node_raycast_bmesh(BMeshNode &node,
                        const float3 &ray_start,
                        const float3 &ray_normal,
                        IsectRayPrecalc *isect_precalc,
                        float *depth,
                        bool use_original,
                        BMVert **r_active_vertex,
                        float3 &r_face_normal);

bool raycast_node_detail_bmesh(BMeshNode &node,
                               const float3 &ray_start,
                               IsectRayPrecalc *isect_precalc,
                               float *depth,
                               float *r_edge_length);

/**
 * For orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision.
 *
 * \note the interval is not guaranteed to lie between ray_start and ray_end; this is
 * not necessary for orthographic views and is impossible anyhow due to the necessity of
 * projecting the far clipping plane into the local object space.  This works out to
 * dividing view3d->clip_end by the object scale, which for small object and large
 * clip_end's can easily lead to floating-point overflows.
 */
void clip_ray_ortho(
    Tree &pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3]);

void find_nearest_to_ray(Tree &pbvh,
                         const FunctionRef<void(Node &node, float *tmin)> fn,
                         const float3 &ray_start,
                         const float3 &ray_normal,
                         bool original);

bool find_nearest_to_ray_node(Tree &pbvh,
                              Node &node,
                              Span<float3> node_positions,
                              bool use_origco,
                              Span<float3> vert_positions,
                              const OffsetIndices<int> faces,
                              Span<int> corner_verts,
                              Span<int3> corner_tris,
                              Span<bool> hide_poly,
                              const SubdivCCG *subdiv_ccg,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq);

/* Drawing */
void set_frustum_planes(Tree &pbvh, PBVHFrustumPlanes *planes);
void get_frustum_planes(const Tree &pbvh, PBVHFrustumPlanes *planes);

/**
 * Get the Tree root's bounding box.
 */
Bounds<float3> bounds_get(const Tree &pbvh);

}  // namespace blender::bke::pbvh

void BKE_pbvh_sync_visibility_from_verts(Object &object);

namespace blender::bke::pbvh {

/**
 * Returns the number of visible quads in the nodes' grids.
 */
int count_grid_quads(const BitGroupVector<> &grid_visibility,
                     Span<int> grid_indices,
                     int gridsize,
                     int display_gridsize);

}  // namespace blender::bke::pbvh

int BKE_pbvh_get_grid_num_verts(const Object &object);
int BKE_pbvh_get_grid_num_faces(const Object &object);

enum PBVHTopologyUpdateMode {
  PBVH_Subdivide = 1,
  PBVH_Collapse = 2,
};
ENUM_OPERATORS(PBVHTopologyUpdateMode, PBVH_Collapse);

namespace blender::bke::pbvh {

/**
 * Collapse short edges, subdivide long edges.
 */
bool bmesh_update_topology(BMesh &bm,
                           Tree &pbvh,
                           BMLog &bm_log,
                           PBVHTopologyUpdateMode mode,
                           float min_edge_len,
                           float max_edge_len,
                           const float center[3],
                           const float view_normal[3],
                           float radius,
                           bool use_frontface,
                           bool use_projected);

}  // namespace blender::bke::pbvh

/* Node Access */

void BKE_pbvh_node_mark_update(blender::bke::pbvh::Node &node);
void BKE_pbvh_node_mark_topology_update(blender::bke::pbvh::Node &node);
void BKE_pbvh_node_fully_hidden_set(blender::bke::pbvh::Node &node, int fully_hidden);
bool BKE_pbvh_node_fully_hidden_get(const blender::bke::pbvh::Node &node);
void BKE_pbvh_node_fully_masked_set(blender::bke::pbvh::Node &node, int fully_masked);
bool BKE_pbvh_node_fully_masked_get(const blender::bke::pbvh::Node &node);
void BKE_pbvh_node_fully_unmasked_set(blender::bke::pbvh::Node &node, int fully_masked);
bool BKE_pbvh_node_fully_unmasked_get(const blender::bke::pbvh::Node &node);

void BKE_pbvh_mark_rebuild_pixels(blender::bke::pbvh::Tree &pbvh);

namespace blender::bke::pbvh {

/**
 * Gather the indices of all base mesh faces in the node.
 * For convenience, pass a reference to the data in the result.
 */
Span<int> node_face_indices_calc_grids(const SubdivCCG &subdiv_ccg,
                                       const GridsNode &node,
                                       Vector<int> &faces);

Bounds<float3> node_bounds(const Node &node);

}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_node_get_original_BB(
    const blender::bke::pbvh::Node *node);

float BKE_pbvh_node_get_tmin(const blender::bke::pbvh::Node *node);

/**
 * Test if AABB is at least partially inside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_contain_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *frustum);
/**
 * Test if AABB is at least partially outside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_exclude_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *frustum);

const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_unique_verts(
    blender::bke::pbvh::BMeshNode *node);
const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_other_verts(
    blender::bke::pbvh::BMeshNode *node);
const blender::Set<BMFace *, 0> &BKE_pbvh_bmesh_node_faces(blender::bke::pbvh::BMeshNode *node);

/**
 * In order to perform operations on the original node coordinates
 * (currently just ray-cast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden.
 */
void BKE_pbvh_bmesh_node_save_orig(BMesh *bm,
                                   BMLog *log,
                                   blender::bke::pbvh::BMeshNode *node,
                                   bool use_original);
void BKE_pbvh_bmesh_after_stroke(BMesh &bm, blender::bke::pbvh::Tree &pbvh);

namespace blender::bke::pbvh {

/**
 * Recalculate node bounding boxes based on the current coordinates. Calculation is only done for
 * affected nodes that have been tagged by #PBVH::tag_positions_changed().
 */
void update_bounds(const Depsgraph &depsgraph, const Object &object, Tree &pbvh);
void update_bounds_mesh(Span<float3> vert_positions, Tree &pbvh);
void update_bounds_grids(const CCGKey &key, Span<float3> positions, Tree &pbvh);
void update_bounds_bmesh(const BMesh &bm, Tree &pbvh);

/**
 * Copy all current node bounds to the original bounds. "Original" bounds are typically from before
 * a brush stroke started (while the "regular" bounds update on every change of positions). These
 * are stored to optimize the BVH traversal for original coordinates enabled by various "use
 * original" arguments in the Tree API.
 */
void store_bounds_orig(Tree &pbvh);

/** Update node "fully masked" and "fully unmasked" values after mask values have been changed. */
void update_mask_mesh(const Mesh &mesh, const IndexMask &node_mask, Tree &pbvh);
void update_mask_grids(const SubdivCCG &subdiv_ccg, const IndexMask &node_mask, Tree &pbvh);
void update_mask_bmesh(const BMesh &bm, const IndexMask &node_mask, Tree &pbvh);

void update_visibility(const Object &object, Tree &pbvh);
void update_normals(const Depsgraph &depsgraph, Object &object_orig, Tree &pbvh);
/** Update geometry normals (potentially on the original object geometry). */
void update_normals_from_eval(Object &object_eval, Tree &pbvh);

}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(const blender::bke::pbvh::Tree &pbvh);
namespace blender::bke::pbvh {
IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        Span<GridsNode> nodes,
                                        const IndexMask &nodes_mask,
                                        IndexMaskMemory &memory);
}

void BKE_pbvh_vert_coords_apply(blender::bke::pbvh::Tree &pbvh,
                                blender::Span<blender::float3> vert_positions);

void BKE_pbvh_node_get_bm_orco_data(const blender::bke::pbvh::BMeshNode &node,
                                    blender::Span<blender::float3> &r_orig_positions,
                                    blender::Span<blender::int3> &r_orig_tris);

namespace blender::bke::pbvh {

/**
 * Retrieve the positions array from the evaluated mesh after deforming modifiers and before
 * topology-changing operations. If there are no deform modifiers, this returns the original mesh's
 * vertex positions.
 */
Span<float3> vert_positions_eval(const Depsgraph &depsgraph, const Object &object_orig);
Span<float3> vert_positions_eval_from_eval(const Object &object_eval);

/**
 * Retrieve write access to the evaluated deform positions, or the original object positions if
 * there are no deformation modifiers. Writing the the evaluated positions is necessary because
 * they are used for drawing and we don't run a full dependency graph update whenever they are
 * changed.
 */
MutableSpan<float3> vert_positions_eval_for_write(const Depsgraph &depsgraph, Object &object_orig);

/**
 * Return the vertex normals corresponding the the positions from #vert_positions_eval. This may be
 * a reference to the normals cache on the original mesh.
 */
Span<float3> vert_normals_eval(const Depsgraph &depsgraph, const Object &object_orig);
Span<float3> vert_normals_eval_from_eval(const Object &object_eval);

Span<float3> face_normals_eval_from_eval(const Object &object_eval);

}  // namespace blender::bke::pbvh

int BKE_pbvh_debug_draw_gen_get(blender::bke::pbvh::Node &node);

namespace blender::bke::pbvh {

/** Return pointers to all the leaf nodes in the BVH tree. */
IndexMask all_leaf_nodes(const Tree &pbvh, IndexMaskMemory &memory);

/** Create a selection of nodes that match the filter function. */
IndexMask search_nodes(const Tree &pbvh,
                       IndexMaskMemory &memory,
                       FunctionRef<bool(const Node &)> filter_fn);

void node_update_mask_mesh(Span<float> mask, MeshNode &node);
void node_update_mask_grids(const CCGKey &key, Span<float> masks, GridsNode &node);
void node_update_mask_bmesh(int mask_offset, BMeshNode &node);

void node_update_visibility_mesh(Span<bool> hide_vert, MeshNode &node);
void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, GridsNode &node);
void node_update_visibility_bmesh(BMeshNode &node);

void update_node_bounds_mesh(Span<float3> positions, MeshNode &node);
void update_node_bounds_grids(int grid_area, Span<float3> positions, GridsNode &node);
void update_node_bounds_bmesh(BMeshNode &node);

/**
 * Run the last step of the BVH bounds recalculation process, propagating updated leaf node bounds
 * to their parent/ancestor inner nodes. This is meant to be used after leaf node bounds have been
 * computed separately.
 */
void flush_bounds_to_parents(Tree &pbvh);

inline Span<int> MeshNode::faces() const
{
  return this->face_indices_;
}
inline Span<int> MeshNode::verts() const
{
  return this->vert_indices_.as_span().slice(0, this->unique_verts_num_);
}
inline Span<int> MeshNode::all_verts() const
{
  return this->vert_indices_;
}
inline int MeshNode::corners_num() const
{
  return corners_num_;
}

inline Span<int> GridsNode::grids() const
{
  return this->prim_indices_;
}

}  // namespace blender::bke::pbvh
