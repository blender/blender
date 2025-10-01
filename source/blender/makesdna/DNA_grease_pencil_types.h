/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_attribute_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
#  include "BLI_bounds_types.hh"
#  include "BLI_function_ref.hh"
#  include "BLI_index_mask_fwd.hh"
#  include "BLI_map.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_memory_counter_fwd.hh"
#  include "BLI_span.hh"
namespace blender::bke {
class AttributeAccessor;
class MutableAttributeAccessor;
class GreasePencilRuntime;
class GreasePencilDrawingRuntime;
namespace greasepencil {
class DrawingRuntime;
class Drawing;
class DrawingReference;
class TreeNode;
class Layer;
class LayerRuntime;
class LayerGroup;
class LayerGroupRuntime;
}  // namespace greasepencil
}  // namespace blender::bke
using GreasePencilRuntimeHandle = blender::bke::GreasePencilRuntime;
using GreasePencilDrawingRuntimeHandle = blender::bke::greasepencil::DrawingRuntime;
using GreasePencilLayerRuntimeHandle = blender::bke::greasepencil::LayerRuntime;
using GreasePencilLayerGroupRuntimeHandle = blender::bke::greasepencil::LayerGroupRuntime;
#else
typedef struct GreasePencilRuntimeHandle GreasePencilRuntimeHandle;
typedef struct GreasePencilDrawingRuntimeHandle GreasePencilDrawingRuntimeHandle;
typedef struct GreasePencilLayerRuntimeHandle GreasePencilLayerRuntimeHandle;
typedef struct GreasePencilLayerGroupRuntimeHandle GreasePencilLayerGroupRuntimeHandle;
#endif

struct Main;
struct GreasePencil;
struct Object;

typedef enum GreasePencilStrokeCapType {
  GP_STROKE_CAP_TYPE_ROUND = 0,
  GP_STROKE_CAP_TYPE_FLAT = 1,
  /* Keep last. */
  GP_STROKE_CAP_TYPE_MAX,
} GreasePencilStrokeCapType;

#define GP_STROKE_MITER_ANGLE_ROUND 0.0f
#define GP_STROKE_MITER_ANGLE_BEVEL DEG2RADF(180.0f)

/**
 * Type of drawing data.
 * If `GP_DRAWING` the node is a `GreasePencilDrawing`,
 * if `GP_DRAWING_REFERENCE` the node is a `GreasePencilDrawingReference`.
 */
typedef enum GreasePencilDrawingType {
  GP_DRAWING = 0,
  GP_DRAWING_REFERENCE = 1,
} GreasePencilDrawingType;

/**
 * Flag for drawings and drawing references. #GreasePencilDrawingBase.flag
 */
typedef enum GreasePencilDrawingBaseFlag {
  /* TODO */
  GreasePencilDrawingBaseFlag_TODO
} GreasePencilDrawingBaseFlag;

/**
 * Base class for drawings and drawing references (drawings from other objects).
 */
typedef struct GreasePencilDrawingBase {
  /**
   * One of `GreasePencilDrawingType`.
   * Indicates if this is an actual drawing or a drawing referenced from another object.
   */
  int8_t type;
  char _pad[3];
  /**
   * Flag. Used to set e.g. the selection status. See `GreasePencilDrawingBaseFlag`.
   */
  uint32_t flag;
} GreasePencilDrawingBase;

/**
 * A grease pencil drawing is a set of strokes. The data is stored using the `CurvesGeometry` data
 * structure and the custom attributes within it.
 */
typedef struct GreasePencilDrawing {
  GreasePencilDrawingBase base;
  /**
   * The stroke data for this drawing.
   */
  CurvesGeometry geometry;
  /**
   * Runtime data on the drawing.
   */
  GreasePencilDrawingRuntimeHandle *runtime;
#ifdef __cplusplus
  blender::bke::greasepencil::Drawing &wrap();
  const blender::bke::greasepencil::Drawing &wrap() const;
#endif
} GreasePencilDrawing;

typedef struct GreasePencilDrawingReference {
  GreasePencilDrawingBase base;
  /**
   * A reference to another GreasePencil data-block.
   * If the data-block has multiple drawings, this drawing references all of them sequentially.
   * See the note in `GreasePencilLayer->frames()` for a detailed explanation of this.
   */
  struct GreasePencil *id_reference;
#ifdef __cplusplus
  blender::bke::greasepencil::DrawingReference &wrap();
  const blender::bke::greasepencil::DrawingReference &wrap() const;
#endif
} GreasePencilDrawingReference;

/**
 * Flag for grease pencil frames. #GreasePencilFrame.flag
 */
typedef enum GreasePencilFrameFlag {
  GP_FRAME_SELECTED = (1 << 0),
  /* When set, the frame is implicitly held until the next frame. E.g. it doesn't have a fixed
   * duration. */
  GP_FRAME_IMPLICIT_HOLD = (1 << 1),
} GreasePencilFrameFlag;

/**
 * A GreasePencilFrame is a single keyframe in the timeline.
 * It references a drawing by index into the drawing array.
 */
typedef struct GreasePencilFrame {
  /**
   * Index into the GreasePencil->drawings array.
   */
  int drawing_index;
  /**
   * Flag. Used to set e.g. the selection.
   */
  uint32_t flag;
  /**
   * Keyframe type. See `eBezTriple_KeyframeType`.
   */
  int8_t type;
  char _pad[3];
#ifdef __cplusplus
  static GreasePencilFrame end();
  bool is_end() const;
  bool is_implicit_hold() const;
  bool is_selected() const;
#endif
} GreasePencilFrame;

typedef enum GreasePencilLayerFramesMapStorageFlag {
  GP_LAYER_FRAMES_STORAGE_DIRTY = (1 << 0),
} GreasePencilLayerFramesMapStorageFlag;

/**
 * Storage for the Map in `blender::bke::greasepencil::Layer`.
 * See the description there for more detail.
 */
typedef struct GreasePencilLayerFramesMapStorage {
  /* Array of `frames` keys (sorted in ascending order). */
  int *keys;
  /* Array of `frames` values (order matches the keys array). */
  GreasePencilFrame *values;
  /* Size of the map (number of key-value pairs). */
  int num;
  /* Flag for the status of the storage. */
  int flag;
} GreasePencilLayerFramesMapStorage;

/**
 * Flag for layer masks. #GreasePencilLayerMask.flag
 */
typedef enum GreasePencilLayerMaskFlag {
  GP_LAYER_MASK_HIDE = (1 << 0),
  GP_LAYER_MASK_INVERT = (1 << 1),
} GreasePencilLayerMaskFlag;

/**
 * A grease pencil layer mask stores the name of a layer that is the mask.
 */
typedef struct GreasePencilLayerMask {
  struct GreasePencilLayerMask *next, *prev;
  /**
   * The name of the layer that is the mask.
   */
  char *layer_name;
  /**
   * Layer mask flag. See `GreasePencilLayerMaskFlag`.
   */
  uint16_t flag;
  char _pad[6];
} GreasePencilLayerMask;

/**
 * Layer blending modes. #GreasePencilLayer.blend_mode
 */
typedef enum GreasePencilLayerBlendMode {
  GP_LAYER_BLEND_NONE = 0,
  GP_LAYER_BLEND_HARDLIGHT = 1,
  GP_LAYER_BLEND_ADD = 2,
  GP_LAYER_BLEND_SUBTRACT = 3,
  GP_LAYER_BLEND_MULTIPLY = 4,
  GP_LAYER_BLEND_DIVIDE = 5,
} GreasePencilLayerBlendMode;

/**
 * Type of layer node.
 * If `GP_LAYER_TREE_LEAF` the node is a `GreasePencilLayerTreeLeaf`,
 * if `GP_LAYER_TREE_GROUP` the node is a `GreasePencilLayerTreeGroup`.
 */
typedef enum GreasePencilLayerTreeNodeType {
  GP_LAYER_TREE_LEAF = 0,
  GP_LAYER_TREE_GROUP = 1,
} GreasePencilLayerTreeNodeType;

/**
 * Flags for layer tree nodes. #GreasePencilLayerTreeNode.flag
 */
typedef enum GreasePencilLayerTreeNodeFlag {
  GP_LAYER_TREE_NODE_HIDE = (1 << 0),
  GP_LAYER_TREE_NODE_LOCKED = (1 << 1),
  GP_LAYER_TREE_NODE_SELECT = (1 << 2),
  GP_LAYER_TREE_NODE_MUTE = (1 << 3),
  GP_LAYER_TREE_NODE_USE_LIGHTS = (1 << 4),
  GP_LAYER_TREE_NODE_HIDE_ONION_SKINNING = (1 << 5),
  GP_LAYER_TREE_NODE_EXPANDED = (1 << 6),
  GP_LAYER_TREE_NODE_HIDE_MASKS = (1 << 7),
  GP_LAYER_TREE_NODE_DISABLE_MASKS_IN_VIEWLAYER = (1 << 8),
  GP_LAYER_TREE_NODE_IGNORE_LOCKED_MATERIALS = (1 << 9),
} GreasePencilLayerTreeNodeFlag;

struct GreasePencilLayerTreeGroup;
typedef struct GreasePencilLayerTreeNode {
  /* ListBase pointers. */
  struct GreasePencilLayerTreeNode *next, *prev;
  /* Parent pointer. Can be null. */
  struct GreasePencilLayerTreeGroup *parent;
  /**
   * Name of the layer/group. Dynamic length.
   */
  char *name;
  /**
   * One of `GreasePencilLayerTreeNodeType`.
   * Indicates the type of struct this element is.
   */
  int8_t type;
  char _pad[7];
  /**
   * Channel color for dope-sheet.
   */
  float color[3];
  /**
   * Flag. Used to set e.g. the selection, visibility, ... status.
   * See `GreasePencilLayerTreeNodeFlag`.
   */
  uint32_t flag;
#ifdef __cplusplus
  blender::bke::greasepencil::TreeNode &wrap();
  const blender::bke::greasepencil::TreeNode &wrap() const;
#endif
} GreasePencilLayerTreeNode;

/**
 * A grease pencil layer is a collection of drawings mapped to a specific time on the timeline.
 */
typedef struct GreasePencilLayer {
  GreasePencilLayerTreeNode base;
  /* Only used for storage in the .blend file. */
  GreasePencilLayerFramesMapStorage frames_storage;
  /**
   * Layer blend mode. See `GreasePencilLayerBlendMode`.
   */
  int8_t blend_mode;
  char _pad[3];
  /**
   * Opacity of the layer.
   */
  float opacity;
  /**
   * List of `GreasePencilLayerMask`.
   */
  ListBase masks;
  int active_mask_index;
  char _pad2[4];
  /**
   * Layer parent object. Can be an armature in which case the `parsubstr` is the bone name.
   */
  struct Object *parent;
  char *parsubstr;
  /**
   * Stores the inverse of the parent during parenting to keep the layer in its position.
   * Also referred to as the "keep transform" parenting elsewhere.
   */
  float parentinv[4][4];
  /**
   * Layer transform UI settings. These should *not* be used to do any computation.
   * Use the functions is the `bke::greasepencil::Layer` class instead.
   */
  float translation[3], rotation[3], scale[3];
  char _pad3[4];
  /** Name of the view layer used to filter render output. */
  char *viewlayername;
  /**
   * Runtime struct pointer.
   */
  GreasePencilLayerRuntimeHandle *runtime;
#ifdef __cplusplus
  blender::bke::greasepencil::Layer &wrap();
  const blender::bke::greasepencil::Layer &wrap() const;
#endif
} GreasePencilLayer;

typedef enum GroupColorTag {
  LAYERGROUP_COLOR_NONE = -1,
  LAYERGROUP_COLOR_01,
  LAYERGROUP_COLOR_02,
  LAYERGROUP_COLOR_03,
  LAYERGROUP_COLOR_04,
  LAYERGROUP_COLOR_05,
  LAYERGROUP_COLOR_06,
  LAYERGROUP_COLOR_07,
  LAYERGROUP_COLOR_08,
} GroupColorTag;

typedef struct GreasePencilLayerTreeGroup {
  GreasePencilLayerTreeNode base;
  /**
   * List of `GreasePencilLayerTreeNode`.
   */
  ListBase children;
  /**
   * Icon color tag.
   */
  int8_t color_tag;
  char _pad[7];
  /**
   * Runtime struct pointer.
   */
  GreasePencilLayerGroupRuntimeHandle *runtime;
#ifdef __cplusplus
  blender::bke::greasepencil::LayerGroup &wrap();
  const blender::bke::greasepencil::LayerGroup &wrap() const;
#endif
} GreasePencilLayerTreeGroup;

/**
 * Flag for the grease pencil data-block. #GreasePencil.flag
 */
typedef enum GreasePencilFlag {
  GREASE_PENCIL_ANIM_CHANNEL_EXPANDED = (1 << 0),
  GREASE_PENCIL_AUTOLOCK_LAYERS = (1 << 1),
  GREASE_PENCIL_STROKE_ORDER_3D = (1 << 2),
} GreasePencilFlag;

/**
 * Onion skinning mode. #GreasePencilOnionSkinningSettings.mode
 */
typedef enum GreasePencilOnionSkinningMode {
  GP_ONION_SKINNING_MODE_ABSOLUTE = 0,
  GP_ONION_SKINNING_MODE_RELATIVE = 1,
  GP_ONION_SKINNING_MODE_SELECTED = 2,
} GreasePencilOnionSkinningMode;

typedef enum GreasePencilOnionSkinningFlag {
  /* Use custom colors (per object-data) for onion skinning. */
  GP_ONION_SKINNING_USE_CUSTOM_COLORS = (1 << 0),
  /* Fade the opacity of ghost frames further away from the current frame. */
  GP_ONION_SKINNING_USE_FADE = (1 << 1),
  /* Show looping frames in onion skinning. */
  GP_ONION_SKINNING_SHOW_LOOP = (1 << 2),
} GreasePencilOnionSkinningFlag;

/**
 * Flag for filtering the onion skinning per keyframe type.
 * #GreasePencilOnionSkinningSettings.filter
 * \note needs to match order of `eBezTriple_KeyframeType`.
 */
typedef enum GreasePencilOnionSkinningFilter {
  GP_ONION_SKINNING_FILTER_KEYTYPE_KEYFRAME = (1 << 0),
  GP_ONION_SKINNING_FILTER_KEYTYPE_EXTREME = (1 << 1),
  GP_ONION_SKINNING_FILTER_KEYTYPE_BREAKDOWN = (1 << 2),
  GP_ONION_SKINNING_FILTER_KEYTYPE_JITTER = (1 << 3),
  GP_ONION_SKINNING_FILTER_KEYTYPE_MOVEHOLD = (1 << 4),
} GreasePencilOnionSkinningFilter;

#define GREASE_PENCIL_ONION_SKINNING_FILTER_ALL \
  (GP_ONION_SKINNING_FILTER_KEYTYPE_KEYFRAME | GP_ONION_SKINNING_FILTER_KEYTYPE_EXTREME | \
   GP_ONION_SKINNING_FILTER_KEYTYPE_BREAKDOWN | GP_ONION_SKINNING_FILTER_KEYTYPE_JITTER | \
   GP_ONION_SKINNING_FILTER_KEYTYPE_MOVEHOLD)

/**
 * Per data-block Grease Pencil onion skinning settings.
 */
typedef struct GreasePencilOnionSkinningSettings {
  /**
   * Opacity for the ghost frames.
   */
  float opacity;
  /* #GreasePencilOnionSkinningMode. */
  int8_t mode;
  /* #GreasePencilOnionSkinningFlag. */
  uint8_t flag;
  /* #GreasePencilOnionSkinningFilter. */
  uint8_t filter;
  char _pad[1];
  /**
   * Number of ghost frames shown before.
   */
  int16_t num_frames_before;
  /**
   * Number of ghost frames shown after.
   */
  int16_t num_frames_after;
  /**
   * Color of the ghost frames before.
   */
  float color_before[3];
  /**
   * Color of the ghost frames after.
   */
  float color_after[3];
  char _pad2[4];
} GreasePencilOnionSkinningSettings;

/**
 * The grease pencil data-block.
 */
typedef struct GreasePencil {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_GP;
#endif

  ID id;
  /** Animation data. */
  struct AnimData *adt;

  /**
   * An array of pointers to drawings. The drawing can own its data or reference it from another
   * data-block. Note that the order of this array is arbitrary. The mapping of drawings to frames
   * is done by the layers. See the `Layer` class in `BKE_grease_pencil.hh`.
   */
  GreasePencilDrawingBase **drawing_array;
  int drawing_array_num;
  char _pad[4];

  /* Root group of the layer tree. */
  GreasePencilLayerTreeGroup *root_group_ptr;

  /** Used only for backward compatibility with old files. */
  CustomData layers_data_legacy;

  /**
   * Layer domain attributes.
   */
  struct AttributeStorage attribute_storage;

  /**
   * The index of the active attribute in the UI.
   *
   * Set to -1 when none is active.
   */
  int attributes_active_index;
  char _pad2[4];

  /**
   * Pointer to the active node. Can be NULL.
   * This pointer does not own the data.
   */
  GreasePencilLayerTreeNode *active_node;

  /**
   * An array of materials.
   */
  struct Material **material_array;
  short material_array_num;
  char _pad3[2];
  /**
   * Global flag on the data-block.
   */
  uint32_t flag;

  ListBase vertex_group_names;
  int vertex_group_active_index;
  char _pad4[4];

  /**
   * Onion skinning settings.
   */
  GreasePencilOnionSkinningSettings onion_skinning_settings;
  /**
   * Runtime struct pointer.
   */
  GreasePencilRuntimeHandle *runtime;
#ifdef __cplusplus
  /* Root group. */
  const blender::bke::greasepencil::LayerGroup &root_group() const;
  blender::bke::greasepencil::LayerGroup &root_group();

  /* Drawings read/write access. */
  blender::Span<const GreasePencilDrawingBase *> drawings() const;
  blender::MutableSpan<GreasePencilDrawingBase *> drawings();
  const GreasePencilDrawingBase *drawing(int64_t index) const;
  GreasePencilDrawingBase *drawing(int64_t index);

  /* Layers, layer groups and nodes read/write access. */
  blender::Span<const blender::bke::greasepencil::Layer *> layers() const;
  blender::Span<blender::bke::greasepencil::Layer *> layers_for_write();
  const blender::bke::greasepencil::Layer &layer(int64_t index) const;
  blender::bke::greasepencil::Layer &layer(int64_t index);

  blender::Span<const blender::bke::greasepencil::LayerGroup *> layer_groups() const;
  blender::Span<blender::bke::greasepencil::LayerGroup *> layer_groups_for_write();

  blender::Span<const blender::bke::greasepencil::TreeNode *> nodes() const;
  blender::Span<blender::bke::greasepencil::TreeNode *> nodes_for_write();

  /* Return the index of the layer if it's found, otherwise `std::nullopt`. */
  std::optional<int> get_layer_index(const blender::bke::greasepencil::Layer &layer) const;

  /* Active layer functions. */
  bool has_active_layer() const;
  const blender::bke::greasepencil::Layer *get_active_layer() const;
  blender::bke::greasepencil::Layer *get_active_layer();
  void set_active_layer(blender::bke::greasepencil::Layer *layer);
  bool is_layer_active(const blender::bke::greasepencil::Layer *layer) const;
  void autolock_inactive_layers();

  /* Active group functions. */
  bool has_active_group() const;
  const blender::bke::greasepencil::LayerGroup *get_active_group() const;
  blender::bke::greasepencil::LayerGroup *get_active_group();

  /* Active node functions. */
  const blender::bke::greasepencil::TreeNode *get_active_node() const;
  blender::bke::greasepencil::TreeNode *get_active_node();
  void set_active_node(blender::bke::greasepencil::TreeNode *node);

  /* Adding layers and layer groups. */
  /** Adds a new layer with the given name to the top of root group. */
  blender::bke::greasepencil::Layer &add_layer(blender::StringRef name,
                                               bool check_name_is_unique = true);
  /** Adds a new layer with the given name to the top of the given group. */
  blender::bke::greasepencil::Layer &add_layer(
      blender::bke::greasepencil::LayerGroup &parent_group,
      blender::StringRef name,
      bool check_name_is_unique = true);
  /** Duplicates a layer from the same object to the top of the root group. */
  blender::bke::greasepencil::Layer &duplicate_layer(
      const blender::bke::greasepencil::Layer &duplicate_layer,
      bool duplicate_frames = false,
      bool duplicate_drawings = false);
  /** Duplicates a layer from the same object to the top of the given group. */
  blender::bke::greasepencil::Layer &duplicate_layer(
      blender::bke::greasepencil::LayerGroup &parent_group,
      const blender::bke::greasepencil::Layer &duplicate_layer,
      bool duplicate_frames = false,
      bool duplicate_drawings = false);
  /** Add new layer group into the root group. */
  blender::bke::greasepencil::LayerGroup &add_layer_group(blender::StringRef name,
                                                          bool check_name_is_unique = true);
  blender::bke::greasepencil::LayerGroup &add_layer_group(
      blender::bke::greasepencil::LayerGroup &parent_group,
      blender::StringRef name,
      bool check_name_is_unique = true);

  /**
   *  Adds multiple layers with an empty name.
   *  NOTE: Evaluated Grease Pencil geometry is allowed to have layers with the same name.
   */
  void add_layers_for_eval(int num_new_layers);

  /* Moving nodes. */
  void move_node_up(blender::bke::greasepencil::TreeNode &node, int step = 1);
  void move_node_down(blender::bke::greasepencil::TreeNode &node, int step = 1);
  void move_node_top(blender::bke::greasepencil::TreeNode &node);
  void move_node_bottom(blender::bke::greasepencil::TreeNode &node);

  void move_node_after(blender::bke::greasepencil::TreeNode &node,
                       blender::bke::greasepencil::TreeNode &target_node);
  void move_node_before(blender::bke::greasepencil::TreeNode &node,
                        blender::bke::greasepencil::TreeNode &target_node);
  void move_node_into(blender::bke::greasepencil::TreeNode &node,
                      blender::bke::greasepencil::LayerGroup &parent_group);

  /* Search functions. */
  const blender::bke::greasepencil::TreeNode *find_node_by_name(blender::StringRef name) const;
  blender::bke::greasepencil::TreeNode *find_node_by_name(blender::StringRef name);
  blender::IndexMask layer_selection_by_name(blender::StringRef name,
                                             blender::IndexMaskMemory &memory) const;

  void rename_node(Main &bmain,
                   blender::bke::greasepencil::TreeNode &node,
                   blender::StringRef new_name);

  void remove_layer(blender::bke::greasepencil::Layer &layer);
  void remove_group(blender::bke::greasepencil::LayerGroup &group, bool keep_children = false);

  std::string unique_layer_name(blender::StringRef name);

  /* Frames API functions. */

  /**
   * Insert a new keyframe in \a layer. If successful, this will also create a new drawing.
   *
   * \param frame_number: The frame number at which the keyframe is inserted (the start frame).
   * \param duration: Duration of the keyframe. If set to 0, then the keyframe is implicitly held
   * (until the next keyframe).
   * \param keytype: The keyframe type (used to render different colored keys).
   *
   * \returns A pointer to a drawing if the keyframe was inserted, otherwise nullptr.
   */
  blender::bke::greasepencil::Drawing *insert_frame(
      blender::bke::greasepencil::Layer &layer,
      int frame_number,
      int duration = 0,
      eBezTriple_KeyframeType keytype = BEZT_KEYTYPE_KEYFRAME);

  /**
   * Same as #insert_frame but insert a new keyframe in each layer in \a layers. This will also
   * create a new drawing for each of the created keyframes.
   */
  void insert_frames(blender::Span<blender::bke::greasepencil::Layer *> layers,
                     int frame_number,
                     int duration = 0,
                     eBezTriple_KeyframeType keytype = BEZT_KEYTYPE_KEYFRAME);
  /**
   * Removes all the frames with \a frame_numbers in the \a layer.
   * \returns true if any frame was removed.
   */
  bool remove_frames(blender::bke::greasepencil::Layer &layer, blender::Span<int> frame_numbers);

  void copy_frames_from_layer(blender::bke::greasepencil::Layer &dst_layer,
                              const GreasePencil &src_grease_pencil,
                              const blender::bke::greasepencil::Layer &src_layer,
                              const std::optional<int> frame_select = std::nullopt);
  /**
   * Adds multiple layers each with its own empty drawing. This can be more efficient than adding
   * every layer and drawing one by one.
   */
  void add_layers_with_empty_drawings_for_eval(int num);

  /**
   * Low-level resizing of drawings array. Only allocates new entries in the array, no drawings are
   * created in case of size increase. In case of size decrease, the removed drawings are deleted.
   */
  void resize_drawings(int new_num);
  /** Add `add_num` new empty geometry drawings. */
  void add_empty_drawings(int add_num);
  void add_duplicate_drawings(int duplicate_num,
                              const blender::bke::greasepencil::Drawing &drawing);
  bool insert_duplicate_frame(blender::bke::greasepencil::Layer &layer,
                              int src_frame_number,
                              int dst_frame_number,
                              bool do_instance);

  /**
   * Move a set of frames in a \a layer.
   *
   * \param frame_number_destinations: describes all transformations that should be applied on the
   * frame keys.
   *
   * If a transformation overlaps another frames, the frame will be overwritten, and the
   * corresponding drawing may be removed, if it no longer has users.
   */
  void move_frames(blender::bke::greasepencil::Layer &layer,
                   const blender::Map<int, int> &frame_number_destinations);

  /**
   * Moves and/or inserts duplicates of a set of frames in a \a layer.
   *
   * \param frame_number_destination: describes all transformations that should be applied on the
   * frame keys.
   * \param duplicate_frames: the frames that should be duplicated instead of moved.
   * Keys of the map are the keys of the corresponding source frames.
   * Frames will be inserted at the key given by the map \a frame_number_destination.
   *
   * If a transformation overlaps another frames, the frame will be overwritten, and the
   * corresponding drawing may be removed, if it no longer has users.
   */
  void move_duplicate_frames(blender::bke::greasepencil::Layer &layer,
                             const blender::Map<int, int> &frame_number_destinations,
                             const blender::Map<int, GreasePencilFrame> &duplicate_frames);

  /**
   * Removes all the drawings that have no users. Will free the drawing data and shrink the
   * drawings array.
   */
  void remove_drawings_with_no_users();
  /**
   * Makes sure all the drawings that the layer points to have a user.
   */
  void update_drawing_users_for_layer(const blender::bke::greasepencil::Layer &layer);

  /**
   * Returns a drawing on \a layer at frame \a frame_number or `nullptr` if no such
   * drawing exists.
   */
  const blender::bke::greasepencil::Drawing *get_drawing_at(
      const blender::bke::greasepencil::Layer &layer, int frame_number) const;
  blender::bke::greasepencil::Drawing *get_drawing_at(
      const blender::bke::greasepencil::Layer &layer, int frame_number);
  /**
   * Returns an editable drawing on \a layer at frame \a frame_number or `nullptr` if no such
   * drawing exists.
   */
  blender::bke::greasepencil::Drawing *get_editable_drawing_at(
      const blender::bke::greasepencil::Layer &layer, int frame_number);

  /**
   * Returns a drawing on \a layer at the frame this grease pencil was evaluated at or `nullptr` if
   * no such drawing exists.
   */
  const blender::bke::greasepencil::Drawing *get_eval_drawing(
      const blender::bke::greasepencil::Layer &layer) const;
  blender::bke::greasepencil::Drawing *get_eval_drawing(
      const blender::bke::greasepencil::Layer &layer);

  std::optional<blender::Bounds<blender::float3>> bounds_min_max(int frame,
                                                                 bool use_radius = true) const;
  std::optional<blender::Bounds<blender::float3>> bounds_min_max_eval(
      bool use_radius = true) const;

  blender::bke::AttributeAccessor attributes() const;
  blender::bke::MutableAttributeAccessor attributes_for_write();

  /**
   * Get the largest material index used by the evaluated layers or `nullopt` if they are empty.
   */
  std::optional<int> material_index_max_eval() const;

  void count_memory(blender::MemoryCounter &memory) const;

  /**
   * Compute the user counts of the drawings by iterating through the keyframes of all the layers
   * and counting the number of references to each drawing.
   */
  blender::Array<int> count_frame_users_for_drawings() const;

  /* For debugging purposes. */
  void print_layer_tree();
  void validate_drawing_user_counts();
#endif
} GreasePencil;
