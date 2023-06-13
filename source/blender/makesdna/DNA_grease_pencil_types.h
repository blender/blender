/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_curves_types.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
#  include "BLI_function_ref.hh"
#  include "BLI_map.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_span.hh"
namespace blender::bke {
class GreasePencilRuntime;
class GreasePencilDrawingRuntime;
namespace greasepencil {
class DrawingRuntime;
class TreeNode;
class Layer;
class LayerRuntime;
class LayerGroup;
class LayerGroupRuntime;
struct StrokePoint;
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

#ifdef __cplusplus
extern "C" {
#endif

struct GreasePencil;
struct BlendDataReader;
struct BlendWriter;
struct Object;

typedef enum GreasePencilStrokeCapType {
  GP_STROKE_CAP_TYPE_ROUND = 0,
  GP_STROKE_CAP_TYPE_FLAT = 1,
  /* Keep last. */
  GP_STROKE_CAP_TYPE_MAX,
} GreasePencilStrokeCapType;

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
  /**
   * The triangles for all the fills in the geometry.
   */
  blender::Span<blender::uint3> triangles() const;
  void tag_positions_changed();
  /**
   * A buffer for a single stroke while drawing.
   */
  blender::Span<blender::bke::greasepencil::StrokePoint> stroke_buffer() const;
  bool has_stroke_buffer() const;
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
} GreasePencilDrawingReference;

/**
 * Flag for grease pencil frames. #GreasePencilFrame.flag
 */
typedef enum GreasePencilFrameFlag {
  GP_FRAME_SELECTED = (1 << 0),
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
  GP_LAYER_TREE_NODE_USE_ONION_SKINNING = (1 << 5),
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
  /**
   * Color tag.
   */
  uint8_t color[3];
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
  /**
   * Runtime struct pointer.
   */
  GreasePencilLayerRuntimeHandle *runtime;
#ifdef __cplusplus
  blender::bke::greasepencil::Layer &wrap();
  const blender::bke::greasepencil::Layer &wrap() const;
#endif
} GreasePencilLayer;

typedef struct GreasePencilLayerTreeGroup {
  GreasePencilLayerTreeNode base;
  /**
   * List of `GreasePencilLayerTreeNode`.
   */
  ListBase children;
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
  /* TODO */
  GreasePencilFlag_TODO
} GreasePencilFlag;

/**
 * Onion skinning mode. #GreasePencilOnionSkinningSettings.mode
 */
typedef enum GreasePencilOnionSkinningMode {
  GP_ONION_SKINNING_MODE_ABSOLUTE = 0,
  GP_ONION_SKINNING_MODE_RELATIVE = 1,
  GP_ONION_SKINNING_MODE_SELECTED = 2,
} GreasePencilOnionSkinningMode;

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
  /**
   * Onion skinning mode. See `GreasePencilOnionSkinningMode`.
   */
  int8_t mode;
  /**
   * Onion skinning filtering flag. See `GreasePencilOnionSkinningFilter`.
   */
  uint8_t filter;
  char _pad[2];
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
  GreasePencilLayerTreeGroup root_group;

  /**
   * Pointer to the active layer. Can be NULL.
   * This pointer does not own the data.
   */
  GreasePencilLayer *active_layer;

  /**
   * An array of materials.
   */
  struct Material **material_array;
  short material_array_num;
  char _pad2[2];
  /**
   * Global flag on the data-block.
   */
  uint32_t flag;
  /**
   * Onion skinning settings.
   */
  GreasePencilOnionSkinningSettings onion_skinning_settings;
  /**
   * Runtime struct pointer.
   */
  GreasePencilRuntimeHandle *runtime;
#ifdef __cplusplus
  /* GreasePencilDrawingBase array functions. */
  void read_drawing_array(BlendDataReader *reader);
  void write_drawing_array(BlendWriter *writer);
  void free_drawing_array();

  /* Layer tree read/write functions. */
  void read_layer_tree(BlendDataReader *reader);
  void write_layer_tree(BlendWriter *writer);

  /* Drawings read/write access. */
  blender::Span<GreasePencilDrawingBase *> drawings() const;
  blender::MutableSpan<GreasePencilDrawingBase *> drawings_for_write();

  /* Layers read/write access. */
  blender::Span<const blender::bke::greasepencil::Layer *> layers() const;
  blender::Span<blender::bke::greasepencil::Layer *> layers_for_write();

  bool has_active_layer() const;
  blender::bke::greasepencil::Layer &add_layer(blender::bke::greasepencil::LayerGroup &group,
                                               blender::StringRefNull name);

  const blender::bke::greasepencil::Layer *find_layer_by_name(blender::StringRefNull name) const;
  blender::bke::greasepencil::Layer *find_layer_by_name(blender::StringRefNull name);

  void add_empty_drawings(int add_num);
  void remove_drawing(int index);

  void foreach_visible_drawing(int frame,
                               blender::FunctionRef<void(int, GreasePencilDrawing &)> function);
  void foreach_editable_drawing(int frame,
                                blender::FunctionRef<void(int, GreasePencilDrawing &)> function);

  bool bounds_min_max(blender::float3 &min, blender::float3 &max) const;

  /* For debugging purposes. */
  void print_layer_tree();
#endif
} GreasePencil;

#ifdef __cplusplus
}
#endif
