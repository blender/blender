/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Low-level operations for grease pencil.
 */

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_shared_cache.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_virtual_array.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"

struct Main;
struct Depsgraph;
struct BoundBox;
struct Scene;
struct Object;
struct Material;

namespace blender::bke {

namespace greasepencil {

/**
 * A single point for a stroke that is currently being drawn.
 */
struct StrokePoint {
  float3 position;
  float radius;
  float opacity;
  float4 color;
};

/**
 * Stroke cache for a stroke that is currently being drawn.
 */
struct StrokeCache {
  Vector<StrokePoint> points;
  Vector<uint3> triangles;
  int mat = 0;

  void clear()
  {
    this->points.clear_and_shrink();
    this->triangles.clear_and_shrink();
    this->mat = 0;
  }
};

class DrawingRuntime {
 public:
  /**
   * Triangle cache for all the strokes in the drawing.
   */
  mutable SharedCache<Vector<uint3>> triangles_cache;
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
  void tag_positions_changed();
  void tag_topology_changed();

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
};

class LayerGroup;
class Layer;

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

 public:
  /**
   * \returns true if this node is a LayerGroup.
   */
  bool is_group() const
  {
    return this->type == GP_LAYER_TREE_GROUP;
  }

  /**
   * \returns true if this node is a Layer.
   */
  bool is_layer() const
  {
    return this->type == GP_LAYER_TREE_LEAF;
  }

  /**
   * \returns this tree node as a LayerGroup.
   * \note This results in undefined behavior if the node is not a LayerGroup.
   */
  const LayerGroup &as_group() const;

  /**
   * \returns this tree node as a Layer.
   * \note This results in undefined behavior if the node is not a Layer.
   */
  const Layer &as_layer() const;

  /**
   * \returns this tree node as a mutable LayerGroup.
   * \note This results in undefined behavior if the node is not a LayerGroup.
   */
  LayerGroup &as_group_for_write();

  /**
   * \returns this tree node as a mutable Layer.
   * \note This results in undefined behavior if the node is not a Layer.
   */
  Layer &as_layer_for_write();

  /**
   * \returns the parent layer group or nullptr for the root group.
   */
  LayerGroup *parent_group() const;
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
  Map<int, GreasePencilFrame> frames_;
  /**
   * Caches a sorted vector of the keys of `frames_`.
   */
  mutable SharedCache<Vector<int>> sorted_keys_cache_;
  /**
   * A vector of LayerMask. This layer will be masked by the layers referenced in the masks.
   * A layer can have zero or more layer masks.
   */
  Vector<LayerMask> masks_;
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

  /**
   * \returns the layer name.
   */
  StringRefNull name() const;
  void set_name(StringRefNull new_name);

  /**
   * \returns the parent layer group.
   */
  LayerGroup &parent_group() const;

  /**
   * \returns the layer as a `TreeNode`.
   */
  const TreeNode &as_node() const;
  TreeNode &as_node();

  /**
   * \returns the frames mapping.
   */
  const Map<int, GreasePencilFrame> &frames() const;
  Map<int, GreasePencilFrame> &frames_for_write();

  bool is_visible() const;
  bool is_locked() const;
  bool is_editable() const;
  bool is_empty() const;
  bool is_selected() const;

  /**
   * Adds a new frame into the layer frames map.
   * Fails if there already exists a frame at \a frame_number that is not a null-frame.
   * Null-frame at \a frame_number and subsequent null-frames are removed.
   *
   * If \a duration is 0, the frame is marked as an implicit hold (see `GP_FRAME_IMPLICIT_HOLD`).
   * Otherwise adds an additional null-frame at \a frame_number + \a duration, if necessary, to
   * indicate the end of the added frame.
   *
   * \returns a pointer to the added frame on success, otherwise nullptr.
   */
  GreasePencilFrame *add_frame(int frame_number, int drawing_index, int duration = 0);
  /**
   * Removes a frame with \a start_frame_number from the frames map.
   *
   * Fails if the map does not contain a frame with \a frame_number or in the specific case where
   * the previous frame has a fixed duration (is not marked as an implicit hold) and the frame to
   * remove is a null frame.
   *
   * Will remove null frames after the frame to remove.
   * \param start_frame_number: the first frame number of the frame to be removed.
   * \return true on success.
   */
  bool remove_frame(int start_frame_number);

  /**
   * Returns the sorted (start) frame numbers of the frames of this layer.
   * \note This will cache the keys lazily.
   */
  Span<int> sorted_keys() const;

  /**
   * \returns the index of the active drawing at frame \a frame_number or -1 if there is no
   * drawing. */
  int drawing_index_at(const int frame_number) const;

  /**
   * \returns a pointer to the active frame at \a frame_number or nullptr if there is no frame.
   */
  const GreasePencilFrame *frame_at(const int frame_number) const;
  GreasePencilFrame *frame_at(const int frame_number);

  void tag_frames_map_changed();

  /**
   * Should be called whenever the keys in the frames map have changed. E.g. when new keys were
   * added, removed or updated.
   */
  void tag_frames_map_keys_changed();

 private:
  using SortedKeysIterator = const int *;
 private:
  GreasePencilFrame *add_frame_internal(int frame_number, int drawing_index);
  int frame_index_at(int frame_number) const;
  /**
   * Removes null frames starting from \a begin until \a end (excluded) or until a non-null frame is reached.
   * \param begin, end: Iterators into the `sorted_keys` span.
   * \returns an iterator to the element after the last null-frame that was removed.
   */
  SortedKeysIterator remove_leading_null_frames_in_range(SortedKeysIterator begin, SortedKeysIterator end);
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
 public:
  LayerGroup();
  explicit LayerGroup(StringRefNull name);
  LayerGroup(const LayerGroup &other);
  ~LayerGroup();

 public:
  StringRefNull name() const;
  void set_name(StringRefNull new_name);

  bool is_visible() const;
  bool is_locked() const;

  /**
   * \returns the group as a `TreeNode`.
   */
  const TreeNode &as_node() const;
  TreeNode &as_node();

  /**
   * Adds a group at the end of this group.
   */
  LayerGroup &add_group(LayerGroup *group);
  LayerGroup &add_group(StringRefNull name);

  /**
   * Adds a layer group after \a link and returns it.
   */
  LayerGroup &add_group_after(LayerGroup *group, TreeNode *link);
  LayerGroup &add_group_after(StringRefNull name, TreeNode *link);

  /**
   * Adds a layer at the end of this group and returns it.
   */
  Layer &add_layer(Layer *layer);
  Layer &add_layer(StringRefNull name);

  /**
   * Adds a layer before \a link and returns it.
   */
  Layer &add_layer_before(Layer *layer, TreeNode *link);
  Layer &add_layer_before(StringRefNull name, TreeNode *link);

  /**
   * Adds a layer after \a link and returns it.
   */
  Layer &add_layer_after(Layer *layer, TreeNode *link);
  Layer &add_layer_after(StringRefNull name, TreeNode *link);

  /**
   * Returns the number of direct nodes in this group.
   */
  int64_t num_direct_nodes() const;

  /**
   * Returns the total number of nodes in this group.
   */
  int64_t num_nodes_total() const;

  /**
   * Tries to unlink the layer from the list of nodes in this group.
   * \returns true, if the layer was successfully unlinked.
   */
  bool unlink_node(TreeNode *link);

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
   * Returns a pointer to the layer with \a name. If no such layer was found, returns nullptr.
   */
  const Layer *find_layer_by_name(StringRefNull name) const;
  Layer *find_layer_by_name(StringRefNull name);

  /**
   * Returns a pointer to the group with \a name. If no such group was found, returns nullptr.
   */
  const LayerGroup *find_group_by_name(StringRefNull name) const;
  LayerGroup *find_group_by_name(StringRefNull name);

  /**
   * Print the nodes. For debugging purposes.
   */
  void print_nodes(StringRefNull header) const;

 private:
  void ensure_nodes_cache() const;
  void tag_nodes_cache_dirty() const;
};

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

inline StringRefNull Layer::name() const
{
  return this->base.name;
}

inline LayerGroup &Layer::parent_group() const
{
  return this->base.parent->wrap();
}

inline StringRefNull LayerGroup::name() const
{
  return this->base.name;
}

namespace convert {

void legacy_gpencil_frame_to_grease_pencil_drawing(const bGPDframe &gpf,
                                                   GreasePencilDrawing &r_drawing);
void legacy_gpencil_to_grease_pencil(Main &main, GreasePencil &grease_pencil, bGPdata &gpd);

}  // namespace convert

}  // namespace greasepencil

class GreasePencilRuntime {
 public:
  /**
   * Allocated and freed by the drawing code. See `DRW_grease_pencil_batch_cache_*` functions.
   */
  void *batch_cache = nullptr;
  bke::greasepencil::StrokeCache stroke_cache;

 public:
  GreasePencilRuntime() {}
  ~GreasePencilRuntime() {}

  /**
   * A buffer for a single stroke while drawing.
   */
  Span<bke::greasepencil::StrokePoint> stroke_buffer() const;
  bool has_stroke_buffer() const;
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

inline GreasePencilDrawingBase *GreasePencil::drawings(int64_t index) const
{
  return this->drawings()[index];
}
inline GreasePencilDrawingBase *GreasePencil::drawings(int64_t index)
{
  return this->drawings()[index];
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
  return (this->active_layer != nullptr);
}

void *BKE_grease_pencil_add(Main *bmain, const char *name);
GreasePencil *BKE_grease_pencil_new_nomain();
GreasePencil *BKE_grease_pencil_copy_for_eval(const GreasePencil *grease_pencil_src);
BoundBox *BKE_grease_pencil_boundbox_get(Object *ob);
void BKE_grease_pencil_data_update(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *object);

int BKE_grease_pencil_object_material_index_get_by_name(Object *ob, const char *name);
Material *BKE_grease_pencil_object_material_new(Main *bmain,
                                                Object *ob,
                                                const char *name,
                                                int *r_index);
Material *BKE_grease_pencil_object_material_ensure_by_name(Main *bmain,
                                                           Object *ob,
                                                           const char *name,
                                                           int *r_index);

bool BKE_grease_pencil_references_cyclic_check(const GreasePencil *id_reference,
                                               const GreasePencil *grease_pencil);
