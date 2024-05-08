/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Low-level operations for grease pencil.
 */

#include <atomic>

#include "BLI_array_utils.hh"
#include "BLI_color.hh"
#include "BLI_function_ref.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_shared_cache.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_virtual_array.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"

struct Main;
struct Depsgraph;
struct Scene;
struct Object;
struct Material;

namespace blender::bke {

namespace greasepencil {

/* Previously, Grease Pencil used a radius convention where 1 `px` = 0.001 units. This `px`
 * was the brush size which would be stored in the stroke thickness and then scaled by the
 * point pressure factor. Finally, the render engine would divide this thickness value by
 * 2000 (we're going from a thickness to a radius, hence the factor of two) to convert back
 * into blender units. With Grease Pencil 3, the radius is no longer stored in `px` space,
 * but in blender units (world space) directly. Also note that there is no longer a stroke
 * "thickness" attribute, the radii are directly stored on the points.
 * For compatibility, legacy thickness values have to be multiplied by this factor. */
constexpr float LEGACY_RADIUS_CONVERSION_FACTOR = 1.0f / 2000.0f;

class DrawingRuntime {
 public:
  /**
   * Triangle cache for all the strokes in the drawing.
   */
  mutable SharedCache<Vector<uint3>> triangles_cache;

  /**
   * Normal vector cache for every stroke. Computed using Newell's method.
   */
  mutable SharedCache<Vector<float3>> curve_plane_normals_cache;

  mutable SharedCache<Vector<float4x2>> curve_texture_matrices;

  /**
   * Number of users for this drawing. The users are the frames in the Grease Pencil layers.
   * Different frames can refer to the same drawing, so we need to make sure we count these users
   * and remove a drawing if it has zero users.
   */
  mutable std::atomic<int> user_count = 1;
};

class Drawing : public ::GreasePencilDrawing {
 public:
  Drawing();
  Drawing(const Drawing &other);
  ~Drawing();

  const bke::CurvesGeometry &strokes() const;
  bke::CurvesGeometry &strokes_for_write();
  /**
   * The triangles for all the fills in the geometry.
   */
  Span<uint3> triangles() const;
  /**
   * Normal vectors for a plane that fits the stroke.
   */
  Span<float3> curve_plane_normals() const;
  void tag_texture_matrices_changed();
  void tag_positions_changed();
  void tag_topology_changed();

  /**
   * Returns the matrices that transform from a 3D point in layer-space to a 2D point in
   * texture-space. This is stored per curve.
   */
  Span<float4x2> texture_matrices() const;
  /**
   * Sets the matrices the that transform from a 3D point in layer-space to a 2D point in
   * texture-space
   */
  void set_texture_matrices(Span<float4x2> matrices, const IndexMask &selection);

  /**
   * Radii of the points. Values are expected to be in blender units.
   */
  VArray<float> radii() const;
  MutableSpan<float> radii_for_write();

  /**
   * Opacity array for the points.
   * Used by the render engine as an alpha value so they are expected to
   * be between 0 and 1 inclusive.
   */
  VArray<float> opacities() const;
  MutableSpan<float> opacities_for_write();

  /**
   * Vertex colors of the points. Default is black. This is mixed on top of the base material
   * stroke color.
   */
  VArray<ColorGeometry4f> vertex_colors() const;
  MutableSpan<ColorGeometry4f> vertex_colors_for_write();

  /**
   * Fill colors of the curves. Default is black and fully transparent. This is mixed on top of the
   * base material fill color.
   */
  VArray<ColorGeometry4f> fill_colors() const;
  MutableSpan<ColorGeometry4f> fill_colors_for_write();

  /**
   * Add a user for this drawing. When a drawing has multiple users, both users are allowed to
   * modify this drawings data.
   */
  void add_user() const;
  /**
   * Removes a user from this drawing. Note that this does not handle deleting the drawing if it
   * has not users.
   */
  void remove_user() const;
  /**
   * Returns true for when this drawing has more than one user.
   */
  bool is_instanced() const;
  bool has_users() const;
};

class DrawingReference : public ::GreasePencilDrawingReference {
 public:
  DrawingReference();
  DrawingReference(const DrawingReference &other);
  ~DrawingReference();
};

/**
 * Copies the drawings from one array to another. Assumes that \a dst_drawings is allocated but not
 * initialized, e.g. it will allocate new drawings and store the pointers.
 */
void copy_drawing_array(Span<const GreasePencilDrawingBase *> src_drawings,
                        MutableSpan<GreasePencilDrawingBase *> dst_drawings);

class LayerGroup;
class Layer;

/* Defines the common functions used by #TreeNode, #Layer, and #LayerGroup.
 * NOTE: Because we cannot mix C-style and C++ inheritance (all of these three classes wrap a
 * C-struct that already uses "inheritance"), we define and implement these methods on all these
 * classes individually. This just means that we can call `layer->name()` directly instead of
 * having to write `layer->as_node().name()`. For #Layer and #LayerGroup the calls are just
 * forwarded to #TreeNode. */
#define TREENODE_COMMON_METHODS \
  StringRefNull name() const; \
  void set_name(StringRefNull new_name); \
  bool is_visible() const; \
  void set_visible(bool visible); \
  bool is_locked() const; \
  void set_locked(bool locked); \
  bool is_editable() const; \
  bool is_selected() const; \
  void set_selected(bool selected); \
  bool use_onion_skinning() const; \
  bool use_masks() const; \
  bool is_child_of(const LayerGroup &group) const;

/* Implements the forwarding of the methods defined by #TREENODE_COMMON_METHODS. */
#define TREENODE_COMMON_METHODS_FORWARD_IMPL(class_name) \
  inline StringRefNull class_name::name() const \
  { \
    return this->as_node().name(); \
  } \
  inline void class_name::set_name(StringRefNull new_name) \
  { \
    return this->as_node().set_name(new_name); \
  } \
  inline bool class_name::is_visible() const \
  { \
    return this->as_node().is_visible(); \
  } \
  inline void class_name::set_visible(const bool visible) \
  { \
    this->as_node().set_visible(visible); \
  } \
  inline bool class_name::is_locked() const \
  { \
    return this->as_node().is_locked(); \
  } \
  inline void class_name::set_locked(const bool locked) \
  { \
    this->as_node().set_locked(locked); \
  } \
  inline bool class_name::is_editable() const \
  { \
    return this->as_node().is_editable(); \
  } \
  inline bool class_name::is_selected() const \
  { \
    return this->as_node().is_selected(); \
  } \
  inline void class_name::set_selected(const bool selected) \
  { \
    this->as_node().set_selected(selected); \
  } \
  inline bool class_name::use_onion_skinning() const \
  { \
    return this->as_node().use_onion_skinning(); \
  } \
  inline bool class_name::use_masks() const \
  { \
    return this->as_node().use_masks(); \
  } \
  inline bool class_name::is_child_of(const LayerGroup &group) const \
  { \
    return this->as_node().is_child_of(group); \
  }

/**
 * A TreeNode represents one node in the layer tree.
 * It can either be a layer or a group. The node has zero children if it is a layer or zero or
 * more children if it is a group.
 */
class TreeNode : public ::GreasePencilLayerTreeNode {
 public:
  TreeNode();
  explicit TreeNode(GreasePencilLayerTreeNodeType type);
  explicit TreeNode(GreasePencilLayerTreeNodeType type, StringRefNull name);
  TreeNode(const TreeNode &other);
  ~TreeNode();

 public:
  /* Define the common functions for #TreeNode. */
  TREENODE_COMMON_METHODS;
  /**
   * \returns true if this node is a #LayerGroup.
   */
  bool is_group() const;
  /**
   * \returns true if this node is a #Layer.
   */
  bool is_layer() const;

  /**
   * \returns this node as a #Layer.
   */
  const Layer &as_layer() const;
  Layer &as_layer();

  /**
   * \returns this node as a #LayerGroup.
   */
  const LayerGroup &as_group() const;
  LayerGroup &as_group();

  /**
   * \returns the parent layer group or nullptr for the root group.
   */
  const LayerGroup *parent_group() const;
  LayerGroup *parent_group();

  const TreeNode *parent_node() const;
  TreeNode *parent_node();

  /**
   * \returns the number of non-null parents of the node.
   */
  int64_t depth() const;
};

/**
 * A layer mask stores a reference to a layer that will mask other layers.
 */
class LayerMask : public ::GreasePencilLayerMask {
 public:
  LayerMask();
  explicit LayerMask(StringRefNull name);
  LayerMask(const LayerMask &other);
  ~LayerMask();
};

/**
 * Structure used to transform frames in a grease pencil layer.
 */
struct LayerTransformData {
  enum FrameTransformationStatus { TRANS_CLEAR, TRANS_INIT, TRANS_RUNNING };

  /* Map of frame keys describing the transformation of the frames. Keys of the map are the source
   * frame indices, and the values of the map are the destination frame indices. */
  Map<int, int> frames_destination;

  /* Copy of the layer frames, stored in two separate maps :
   * - frames_static contains the frames not affected by the transformation,
   * - frames_transformed contains the frames affected by the transformation.
   * This allows to display the transformation while running, without removing any drawing.
   */
  Map<int, GreasePencilFrame> frames_static;
  Map<int, GreasePencilFrame> frames_transformed;

  /* Map containing the duration (in frames) for each frame in the layer that has a fixed duration,
   * i.e. each frame that is not an implicit hold. */
  Map<int, int> frames_duration;

  /* Temporary copy of duplicated frames before we decide on a place to insert them.
   * Used in the move+duplicate operator. */
  Map<int, GreasePencilFrame> duplicated_frames_buffer;

  FrameTransformationStatus status{TRANS_CLEAR};
};

/* The key of a GreasePencilFrame in the frames map is the starting scene frame number (int) of
 * that frame. */
using FramesMapKey = int;

class LayerRuntime {
 public:
  /**
   * This Map maps a scene frame number (key) to a GreasePencilFrame. This struct holds an index
   * (drawing_index) to the drawing in the GreasePencil->drawings array. The frame number indicates
   * the first frame the drawing is shown. The end time is implicitly defined by the next greater
   * frame number (key) in the map. If the value mapped to (index) is -1, no drawing is shown at
   * this frame.
   *
   *    \example:
   *
   *    {0: 0, 5: 1, 10: -1, 12: 2, 16: -1}
   *
   *    In this example there are three drawings (drawing #0, drawing #1 and drawing #2). The first
   *    drawing starts at frame 0 and ends at frame 5 (exclusive). The second drawing starts at
   *    frame 5 and ends at frame 10. Finally, the third drawing starts at frame 12 and ends at
   *    frame 16.
   *
   *                  | | | | | | | | | | |1|1|1|1|1|1|1|
   *    Scene Frame:  |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|...
   *    Drawing:      [#0      ][#1      ]    [#2    ]
   *
   * \note If a drawing references another data-block, all of the drawings in that data-block are
   * mapped sequentially to the frames (frame-by-frame). If another frame starts, the rest of the
   * referenced drawings are discarded. If the frame is longer than the number of referenced
   * drawings, then the last referenced drawing is held for the rest of the duration.
   */
  Map<FramesMapKey, GreasePencilFrame> frames_;
  /**
   * Caches a sorted vector of the keys of `frames_`.
   */
  mutable SharedCache<Vector<FramesMapKey>> sorted_keys_cache_;
  /**
   * A vector of LayerMask. This layer will be masked by the layers referenced in the masks.
   * A layer can have zero or more layer masks.
   */
  Vector<LayerMask> masks_;

  /* Runtime data used for frame transformations. */
  LayerTransformData trans_data_;

 public:
  /* Reset all runtime data. */
  void clear();
};

/**
 * A layer maps drawings to scene frames. It can be thought of as one independent channel in the
 * timeline.
 */
class Layer : public ::GreasePencilLayer {
 public:
  Layer();
  explicit Layer(StringRefNull name);
  Layer(const Layer &other);
  ~Layer();

 public:
  /* Define the common functions for #TreeNode. */
  TREENODE_COMMON_METHODS;
  /**
   * \returns the layer as a `TreeNode`.
   */
  const TreeNode &as_node() const;
  TreeNode &as_node();

  /**
   * \returns the parent #LayerGroup of this layer.
   */
  const LayerGroup &parent_group() const;
  LayerGroup &parent_group();

  /**
   * \returns the frames mapping.
   */
  const Map<FramesMapKey, GreasePencilFrame> &frames() const;
  Map<FramesMapKey, GreasePencilFrame> &frames_for_write();

  bool is_empty() const;

  /**
   * Adds a new frame into the layer frames map.
   * Fails if there already exists a frame at \a key that is not a null-frame.
   * Null-frame at \a key and subsequent null-frames are removed.
   *
   * If \a duration is 0, the frame is marked as an implicit hold (see `GP_FRAME_IMPLICIT_HOLD`).
   * Otherwise adds an additional null-frame at \a key + \a duration, if necessary, to
   * indicate the end of the added frame.
   *
   * \returns a pointer to the added frame on success, otherwise nullptr.
   */
  GreasePencilFrame *add_frame(FramesMapKey key, int drawing_index, int duration = 0);
  /**
   * Removes a frame with \a key from the frames map.
   *
   * Fails if the map does not contain a frame with \a key or in the specific case where
   * the previous frame has a fixed duration (is not marked as an implicit hold) and the frame to
   * remove is a null frame.
   *
   * Will remove null frames after the frame to remove.
   * \return true on success.
   */
  bool remove_frame(FramesMapKey key);

  /**
   * Returns the sorted keys (start frame numbers) of the frames of this layer.
   * \note This will cache the keys lazily.
   */
  Span<FramesMapKey> sorted_keys() const;

  /**
   * \returns the index of the active drawing at frame \a frame_number or -1 if there is no
   * drawing. */
  int drawing_index_at(const int frame_number) const;

  /**
   * \returns true if there is a drawing on this layer at \a frame_number.
   */
  bool has_drawing_at(const int frame_number) const;

  /**
   * \returns the key of the active frame at \a frame_number or #std::nullopt if there is no frame.
   */
  std::optional<FramesMapKey> frame_key_at(int frame_number) const;

  /**
   * \returns a pointer to the active frame at \a frame_number or nullptr if there is no frame.
   */
  const GreasePencilFrame *frame_at(const int frame_number) const;
  GreasePencilFrame *frame_at(const int frame_number);

  /**
   * \returns the frame duration of the active frame at \a frame_number or -1 if there is no active
   * frame or the active frame is the last frame.
   */
  int get_frame_duration_at(const int frame_number) const;

  void tag_frames_map_changed();

  /**
   * Should be called whenever the keys in the frames map have changed. E.g. when new keys were
   * added, removed or updated.
   */
  void tag_frames_map_keys_changed();

  /**
   * Prepare the DNA #GreasePencilLayer data before blend-file writing.
   */
  void prepare_for_dna_write();

  /**
   * Update from DNA #GreasePencilLayer data after blend-file reading.
   */
  void update_from_dna_read();

  /**
   * Returns the transformation from layer space to object space.
   */
  float4x4 to_object_space(const Object &object) const;

  /**
   * Returns the transformation from layer space to world space.
   */
  float4x4 to_world_space(const Object &object) const;

  /**
   * Returns the name of the parent bone. Should only be used in case the parent object is an
   * armature.
   */
  StringRefNull parent_bone_name() const;
  void set_parent_bone_name(const char *new_name);

  /**
   * Returns the view layer name that this layer should be rendered in or an empty
   * `StringRefNull` if no such name is set.
   */
  StringRefNull view_layer_name() const;
  void set_view_layer_name(const char *new_name);

 private:
  using SortedKeysIterator = const int *;

 private:
  GreasePencilFrame *add_frame_internal(int frame_number, int drawing_index);

  /**
   * Removes null frames starting from \a begin until \a end (excluded) or until a non-null frame
   * is reached. \param begin, end: Iterators into the `sorted_keys` span. \returns an iterator to
   * the element after the last null-frame that was removed.
   */
  SortedKeysIterator remove_leading_null_frames_in_range(SortedKeysIterator begin,
                                                         SortedKeysIterator end);

  float4x4 parent_inverse() const;

  /**
   * The local transform of the layer (in layer space, not object space).
   */
  float4x4 local_transform() const;

  /**
   * Get the parent to world matrix for this layer.
   */
  float4x4 parent_to_world(const Object &parent) const;
};

class LayerGroupRuntime {
 public:
  /**
   * CacheMutex for `nodes_cache_` and `layer_cache_`;
   */
  mutable CacheMutex nodes_cache_mutex_;
  /**
   * Caches all the nodes of this group in a single pre-ordered vector.
   */
  mutable Vector<TreeNode *> nodes_cache_;
  /**
   * Caches all the layers in this group in a single pre-ordered vector.
   */
  mutable Vector<Layer *> layer_cache_;
  /**
   * Caches all the layer groups in this group in a single pre-ordered vector.
   */
  mutable Vector<LayerGroup *> layer_group_cache_;
};

/**
 * A LayerGroup is a grouping of zero or more Layers.
 */
class LayerGroup : public ::GreasePencilLayerTreeGroup {
  friend struct ::GreasePencil;

 public:
  LayerGroup();
  explicit LayerGroup(StringRefNull name);
  LayerGroup(const LayerGroup &other);
  ~LayerGroup();

  LayerGroup &operator=(const LayerGroup &other);

 public:
  /* Define the common functions for #TreeNode. */
  TREENODE_COMMON_METHODS;
  /**
   * \returns the group as a `TreeNode`.
   */
  const TreeNode &as_node() const;
  TreeNode &as_node();

  /**
   * Returns the number of direct nodes in this group.
   */
  int64_t num_direct_nodes() const;

  /**
   * Returns the total number of nodes in this group.
   */
  int64_t num_nodes_total() const;

  /**
   * Returns a `Span` of pointers to all the `TreeNode`s in this group.
   */
  Span<const TreeNode *> nodes() const;
  Span<TreeNode *> nodes_for_write();

  /**
   * Returns a `Span` of pointers to all the `Layer`s in this group.
   */
  Span<const Layer *> layers() const;
  Span<Layer *> layers_for_write();

  /**
   * Returns a `Span` of pointers to all the `LayerGroups`s in this group.
   */
  Span<const LayerGroup *> groups() const;
  Span<LayerGroup *> groups_for_write();

  /**
   * Returns a pointer to the node with \a name. If no such node was found, returns nullptr.
   */
  const TreeNode *find_node_by_name(StringRefNull name) const;
  TreeNode *find_node_by_name(StringRefNull name);

  /**
   * Print the nodes. For debugging purposes.
   */
  void print_nodes(StringRefNull header) const;

  /**
   * Prepare the DNA #GreasePencilLayerTreeGroup data before blend-file writing.
   */
  void prepare_for_dna_write();

  /**
   * Update from DNA #GreasePencilLayerTreeGroup data after blend-file reading.
   */
  void update_from_dna_read();

 protected:
  /**
   * Adds a new layer named \a name at the end of this group and returns it.
   */
  Layer &add_layer(StringRefNull name);
  Layer &add_layer(const Layer &duplicate_layer);
  /**
   * Adds a new group named \a name at the end of this group and returns it.
   */
  LayerGroup &add_group(StringRefNull name);
  /**
   * Adds an existing \a node at the end of this group.
   */
  TreeNode &add_node(TreeNode &node);

  /**
   * Adds an existing \a node before \a link of this group.
   */
  void add_node_before(TreeNode &node, TreeNode &link);
  /**
   * Adds an existing \a node after \a link of this group.
   */
  void add_node_after(TreeNode &node, TreeNode &link);

  /**
   * Move child \a node up/down by \a step.
   */
  void move_node_up(TreeNode &node, int step = 1);
  void move_node_down(TreeNode &node, int step = 1);
  /**
   * Move child \a node to the top/bottom.
   */
  void move_node_top(TreeNode &node);
  void move_node_bottom(TreeNode &node);

  /**
   * Unlink the node from the list of nodes in this group.
   * \returns true, if the node was successfully unlinked.
   */
  bool unlink_node(TreeNode &link);

 private:
  void ensure_nodes_cache() const;
  void tag_nodes_cache_dirty() const;
};

inline void Drawing::add_user() const
{
  this->runtime->user_count.fetch_add(1, std::memory_order_relaxed);
}
inline void Drawing::remove_user() const
{
  this->runtime->user_count.fetch_sub(1, std::memory_order_relaxed);
}
inline bool Drawing::is_instanced() const
{
  return this->runtime->user_count.load(std::memory_order_relaxed) > 1;
}
inline bool Drawing::has_users() const
{
  return this->runtime->user_count.load(std::memory_order_relaxed) > 0;
}

inline bool TreeNode::is_group() const
{
  return this->type == GP_LAYER_TREE_GROUP;
}
inline bool TreeNode::is_layer() const
{
  return this->type == GP_LAYER_TREE_LEAF;
}
inline bool TreeNode::is_visible() const
{
  return ((this->flag & GP_LAYER_TREE_NODE_HIDE) == 0) &&
         (!this->parent_group() || this->parent_group()->as_node().is_visible());
}
inline void TreeNode::set_visible(const bool visible)
{
  SET_FLAG_FROM_TEST(this->flag, !visible, GP_LAYER_TREE_NODE_HIDE);
}
inline bool TreeNode::is_locked() const
{
  return ((this->flag & GP_LAYER_TREE_NODE_LOCKED) != 0) ||
         (this->parent_group() && this->parent_group()->as_node().is_locked());
}
inline void TreeNode::set_locked(const bool locked)
{
  SET_FLAG_FROM_TEST(this->flag, locked, GP_LAYER_TREE_NODE_LOCKED);
}
inline bool TreeNode::is_editable() const
{
  return this->is_visible() && !this->is_locked();
}
inline bool TreeNode::is_selected() const
{
  return (this->flag & GP_LAYER_TREE_NODE_SELECT) != 0;
}
inline void TreeNode::set_selected(const bool selected)
{
  SET_FLAG_FROM_TEST(this->flag, selected, GP_LAYER_TREE_NODE_SELECT);
}
inline bool TreeNode::use_onion_skinning() const
{
  return ((this->flag & GP_LAYER_TREE_NODE_HIDE_ONION_SKINNING) == 0) &&
         (!this->parent_group() || this->parent_group()->as_node().use_onion_skinning());
}
inline bool TreeNode::use_masks() const
{
  return ((this->flag & GP_LAYER_TREE_NODE_HIDE_MASKS) == 0) &&
         (!this->parent_group() || this->parent_group()->as_node().use_masks());
}
inline bool TreeNode::is_child_of(const LayerGroup &group) const
{
  if (const LayerGroup *parent = this->parent_group()) {
    if (parent == &group) {
      return true;
    }
    return parent->is_child_of(group);
  }
  return false;
}
inline StringRefNull TreeNode::name() const
{
  return (this->GreasePencilLayerTreeNode::name != nullptr) ?
             this->GreasePencilLayerTreeNode::name :
             StringRefNull();
}
inline const TreeNode &LayerGroup::as_node() const
{
  return *reinterpret_cast<const TreeNode *>(this);
}
inline TreeNode &LayerGroup::as_node()
{
  return *reinterpret_cast<TreeNode *>(this);
}

inline const TreeNode &Layer::as_node() const
{
  return *reinterpret_cast<const TreeNode *>(this);
}
inline TreeNode &Layer::as_node()
{
  return *reinterpret_cast<TreeNode *>(this);
}

TREENODE_COMMON_METHODS_FORWARD_IMPL(Layer);
inline bool Layer::is_empty() const
{
  return (this->frames().is_empty());
}
inline const LayerGroup &Layer::parent_group() const
{
  return *this->as_node().parent_group();
}
inline LayerGroup &Layer::parent_group()
{
  return *this->as_node().parent_group();
}

TREENODE_COMMON_METHODS_FORWARD_IMPL(LayerGroup);

}  // namespace greasepencil

class GreasePencilRuntime {
 public:
  /**
   * Allocated and freed by the drawing code. See `DRW_grease_pencil_batch_cache_*` functions.
   */
  void *batch_cache = nullptr;
  /* The frame on which the object was evaluated (only valid for evaluated object). */
  int eval_frame = 0;

 public:
  GreasePencilRuntime() {}
  ~GreasePencilRuntime() {}
};

class GreasePencilDrawingEditHints {
 public:
  const greasepencil::Drawing *drawing_orig;
  ImplicitSharingPtrAndData positions_data;

  std::optional<Span<float3>> positions() const;
  std::optional<MutableSpan<float3>> positions_for_write();
};

/**
 * Used to propagate deformation data through modifier evaluation.
 */
class GreasePencilEditHints {
 public:
  /**
   * Original data that the edit hints below are meant to be used for.
   */
  const GreasePencil &grease_pencil_id_orig;

  GreasePencilEditHints(const GreasePencil &grease_pencil_id_orig)
      : grease_pencil_id_orig(grease_pencil_id_orig)
  {
  }

  /**
   * Array of #GreasePencilDrawingEditHints. There is one edit hint for each evaluated drawing.
   * \note The index for each element is the layer index.
   */
  std::optional<Array<GreasePencilDrawingEditHints>> drawing_hints;
};

}  // namespace blender::bke

inline blender::bke::greasepencil::Drawing &GreasePencilDrawing::wrap()
{
  return *reinterpret_cast<blender::bke::greasepencil::Drawing *>(this);
}
inline const blender::bke::greasepencil::Drawing &GreasePencilDrawing::wrap() const
{
  return *reinterpret_cast<const blender::bke::greasepencil::Drawing *>(this);
}

inline blender::bke::greasepencil::DrawingReference &GreasePencilDrawingReference::wrap()
{
  return *reinterpret_cast<blender::bke::greasepencil::DrawingReference *>(this);
}
inline const blender::bke::greasepencil::DrawingReference &GreasePencilDrawingReference::wrap()
    const
{
  return *reinterpret_cast<const blender::bke::greasepencil::DrawingReference *>(this);
}

inline GreasePencilFrame GreasePencilFrame::null()
{
  return GreasePencilFrame{-1, 0, 0};
}

inline bool GreasePencilFrame::is_null() const
{
  return this->drawing_index == -1;
}

inline bool GreasePencilFrame::is_implicit_hold() const
{
  return (this->flag & GP_FRAME_IMPLICIT_HOLD) != 0;
}

inline bool GreasePencilFrame::is_selected() const
{
  return (this->flag & GP_FRAME_SELECTED) != 0;
}

inline blender::bke::greasepencil::TreeNode &GreasePencilLayerTreeNode::wrap()
{
  return *reinterpret_cast<blender::bke::greasepencil::TreeNode *>(this);
}
inline const blender::bke::greasepencil::TreeNode &GreasePencilLayerTreeNode::wrap() const
{
  return *reinterpret_cast<const blender::bke::greasepencil::TreeNode *>(this);
}

inline blender::bke::greasepencil::Layer &GreasePencilLayer::wrap()
{
  return *reinterpret_cast<blender::bke::greasepencil::Layer *>(this);
}
inline const blender::bke::greasepencil::Layer &GreasePencilLayer::wrap() const
{
  return *reinterpret_cast<const blender::bke::greasepencil::Layer *>(this);
}

inline blender::bke::greasepencil::LayerGroup &GreasePencilLayerTreeGroup::wrap()
{
  return *reinterpret_cast<blender::bke::greasepencil::LayerGroup *>(this);
}
inline const blender::bke::greasepencil::LayerGroup &GreasePencilLayerTreeGroup::wrap() const
{
  return *reinterpret_cast<const blender::bke::greasepencil::LayerGroup *>(this);
}

inline const GreasePencilDrawingBase *GreasePencil::drawing(const int64_t index) const
{
  BLI_assert(index >= 0 && index < this->drawings().size());
  return this->drawings()[index];
}
inline GreasePencilDrawingBase *GreasePencil::drawing(const int64_t index)
{
  BLI_assert(index >= 0 && index < this->drawings().size());
  return this->drawings()[index];
}

inline const blender::bke::greasepencil::Layer *GreasePencil::layer(const int64_t index) const
{
  BLI_assert(index >= 0 && index < this->layers().size());
  return this->layers()[index];
}
inline blender::bke::greasepencil::Layer *GreasePencil::layer(const int64_t index)
{
  BLI_assert(index >= 0 && index < this->layers().size());
  return this->layers_for_write()[index];
}

inline const blender::bke::greasepencil::LayerGroup &GreasePencil::root_group() const
{
  return this->root_group_ptr->wrap();
}
inline blender::bke::greasepencil::LayerGroup &GreasePencil::root_group()
{
  return this->root_group_ptr->wrap();
}

inline bool GreasePencil::has_active_layer() const
{
  return (this->active_node != nullptr) && (this->active_node->wrap().is_layer());
}

void *BKE_grease_pencil_add(Main *bmain, const char *name);
GreasePencil *BKE_grease_pencil_new_nomain();
GreasePencil *BKE_grease_pencil_copy_for_eval(const GreasePencil *grease_pencil_src);
void BKE_grease_pencil_data_update(Depsgraph *depsgraph, Scene *scene, Object *object);
void BKE_grease_pencil_duplicate_drawing_array(const GreasePencil *grease_pencil_src,
                                               GreasePencil *grease_pencil_dst);

int BKE_grease_pencil_object_material_index_get_by_name(Object *ob, const char *name);
Material *BKE_grease_pencil_object_material_new(Main *bmain,
                                                Object *ob,
                                                const char *name,
                                                int *r_index);
Material *BKE_grease_pencil_object_material_from_brush_get(Object *ob, Brush *brush);
Material *BKE_grease_pencil_object_material_ensure_by_name(Main *bmain,
                                                           Object *ob,
                                                           const char *name,
                                                           int *r_index);
Material *BKE_grease_pencil_brush_material_get(Brush *brush);
Material *BKE_grease_pencil_object_material_ensure_from_brush(Main *bmain,
                                                              Object *ob,
                                                              Brush *brush);
Material *BKE_grease_pencil_object_material_ensure_from_active_input_brush(Main *bmain,
                                                                           Object *ob,
                                                                           Brush *brush);
Material *BKE_grease_pencil_object_material_ensure_from_active_input_material(Object *ob);
Material *BKE_grease_pencil_object_material_ensure_active(Object *ob);
void BKE_grease_pencil_material_remap(GreasePencil *grease_pencil, const uint *remap, int totcol);
void BKE_grease_pencil_material_index_remove(GreasePencil *grease_pencil, int index);

bool BKE_grease_pencil_references_cyclic_check(const GreasePencil *id_reference,
                                               const GreasePencil *grease_pencil);
bool BKE_grease_pencil_material_index_used(GreasePencil *grease_pencil, int index);
