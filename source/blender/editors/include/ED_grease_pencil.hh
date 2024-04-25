/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_grease_pencil.hh"

#include "BLI_generic_span.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_set.hh"

#include "ED_keyframes_edit.hh"

#include "WM_api.hh"

struct bContext;
struct Main;
struct Object;
struct KeyframeEditData;
struct wmKeyConfig;
struct wmOperator;
struct ToolSettings;
struct Scene;
struct UndoType;
struct ViewDepths;
struct View3D;
struct ViewContext;
namespace blender {
namespace bke {
enum class AttrDomain : int8_t;
}
}  // namespace blender

enum {
  LAYER_REORDER_ABOVE,
  LAYER_REORDER_BELOW,
};

/* -------------------------------------------------------------------- */
/** \name C Wrappers
 * \{ */

void ED_operatortypes_grease_pencil();
void ED_operatortypes_grease_pencil_draw();
void ED_operatortypes_grease_pencil_frames();
void ED_operatortypes_grease_pencil_layers();
void ED_operatortypes_grease_pencil_select();
void ED_operatortypes_grease_pencil_edit();
void ED_operatortypes_grease_pencil_material();
void ED_operatortypes_grease_pencil_primitives();
void ED_operatortypes_grease_pencil_weight_paint();
void ED_operatormacros_grease_pencil();
void ED_keymap_grease_pencil(wmKeyConfig *keyconf);
void ED_primitivetool_modal_keymap(wmKeyConfig *keyconf);

void GREASE_PENCIL_OT_stroke_cutter(wmOperatorType *ot);

void ED_undosys_type_grease_pencil(UndoType *undo_type);
/**
 * Get the selection mode for Grease Pencil selection operators: point, stroke, segment.
 */
blender::bke::AttrDomain ED_grease_pencil_selection_domain_get(const ToolSettings *tool_settings);

/** \} */

namespace blender::ed::greasepencil {

enum class DrawingPlacementDepth { ObjectOrigin, Cursor, Surface, NearestStroke };

enum class DrawingPlacementPlane { View, Front, Side, Top, Cursor };

class DrawingPlacement {
  const ARegion *region_;
  const View3D *view3d_;

  DrawingPlacementDepth depth_;
  DrawingPlacementPlane plane_;
  ViewDepths *depth_cache_ = nullptr;
  float surface_offset_;

  float3 placement_loc_;
  float3 placement_normal_;
  float4 placement_plane_;

  float4x4 layer_space_to_world_space_;
  float4x4 world_space_to_layer_space_;

 public:
  DrawingPlacement() = default;
  DrawingPlacement(const Scene &scene,
                   const ARegion &region,
                   const View3D &view3d,
                   const Object &eval_object,
                   const bke::greasepencil::Layer &layer);
  ~DrawingPlacement();

 public:
  bool use_project_to_surface() const;
  bool use_project_to_nearest_stroke() const;

  void cache_viewport_depths(Depsgraph *depsgraph, ARegion *region, View3D *view3d);
  void set_origin_to_nearest_stroke(float2 co);

  /**
   * Projects a screen space coordinate to the local drawing space.
   */
  float3 project(float2 co) const;
  void project(Span<float2> src, MutableSpan<float3> dst) const;
};

void set_selected_frames_type(bke::greasepencil::Layer &layer,
                              const eBezTriple_KeyframeType key_type);

bool snap_selected_frames(GreasePencil &grease_pencil,
                          bke::greasepencil::Layer &layer,
                          Scene &scene,
                          const eEditKeyframes_Snap mode);

bool mirror_selected_frames(GreasePencil &grease_pencil,
                            bke::greasepencil::Layer &layer,
                            Scene &scene,
                            const eEditKeyframes_Mirror mode);

/**
 * Creates duplicate frames for each selected frame in the layer.
 * The duplicates are stored in the LayerTransformData structure of the layer runtime data.
 * This function also deselects the selected frames, while keeping the duplicates selected.
 */
bool duplicate_selected_frames(GreasePencil &grease_pencil, bke::greasepencil::Layer &layer);

bool remove_all_selected_frames(GreasePencil &grease_pencil, bke::greasepencil::Layer &layer);

void select_layer_channel(GreasePencil &grease_pencil, bke::greasepencil::Layer *layer);

struct KeyframeClipboard {
  /* Datatype for use in copy/paste buffer. */
  struct DrawingBufferItem {
    blender::bke::greasepencil::FramesMapKey frame_number;
    bke::greasepencil::Drawing drawing;
    int duration;
  };

  struct LayerBufferItem {
    Vector<DrawingBufferItem> drawing_buffers;
    blender::bke::greasepencil::FramesMapKey first_frame;
    blender::bke::greasepencil::FramesMapKey last_frame;
  };

  Map<std::string, LayerBufferItem> copy_buffer{};
  int first_frame{std::numeric_limits<int>::max()};
  int last_frame{std::numeric_limits<int>::min()};
  int cfra{0};

  void clear()
  {
    copy_buffer.clear();
    first_frame = std::numeric_limits<int>::max();
    last_frame = std::numeric_limits<int>::min();
    cfra = 0;
  }
};

bool grease_pencil_copy_keyframes(bAnimContext *ac, KeyframeClipboard &clipboard);

bool grease_pencil_paste_keyframes(bAnimContext *ac,
                                   const eKeyPasteOffset offset_mode,
                                   const eKeyMergeMode merge_mode,
                                   const KeyframeClipboard &clipboard);

/**
 * Sets the selection flag, according to \a selection_mode to the frame at \a frame_number in the
 * \a layer if such frame exists. Returns false if no such frame exists.
 */
bool select_frame_at(bke::greasepencil::Layer &layer,
                     const int frame_number,
                     const short select_mode);

void select_frames_at(bke::greasepencil::LayerGroup &layer_group,
                      const int frame_number,
                      const short select_mode);

void select_all_frames(bke::greasepencil::Layer &layer, const short select_mode);

void select_frames_region(KeyframeEditData *ked,
                          bke::greasepencil::TreeNode &node,
                          const short tool,
                          const short select_mode);

void select_frames_range(bke::greasepencil::TreeNode &node,
                         const float min,
                         const float max,
                         const short select_mode);

/**
 * Returns true if any frame of the \a layer is selected.
 */
bool has_any_frame_selected(const bke::greasepencil::Layer &layer);

/**
 * Check for an active keyframe at the current scene time. When there is not,
 * create one when auto-key is on (taking additive drawing setting into account).
 * \return false when no keyframe could be found or created.
 */
bool ensure_active_keyframe(const Scene &scene, GreasePencil &grease_pencil);

void create_keyframe_edit_data_selected_frames_list(KeyframeEditData *ked,
                                                    const bke::greasepencil::Layer &layer);

bool active_grease_pencil_poll(bContext *C);
bool editable_grease_pencil_poll(bContext *C);
bool active_grease_pencil_layer_poll(bContext *C);
bool editable_grease_pencil_point_selection_poll(bContext *C);
bool grease_pencil_painting_poll(bContext *C);
bool grease_pencil_sculpting_poll(bContext *C);
bool grease_pencil_weight_painting_poll(bContext *C);

float opacity_from_input_sample(const float pressure,
                                const Brush *brush,
                                const Scene *scene,
                                const BrushGpencilSettings *settings);
float radius_from_input_sample(const float pressure,
                               const float3 location,
                               ViewContext vc,
                               const Brush *brush,
                               const Scene *scene,
                               const BrushGpencilSettings *settings);
int grease_pencil_draw_operator_invoke(bContext *C, wmOperator *op);

struct DrawingInfo {
  const bke::greasepencil::Drawing &drawing;
  const int layer_index;
  const int frame_number;
  /* This is used by the onion skinning system. A value of 0 means the drawing is on the current
   * frame. Negative values are before the current frame, positive values are drawings after the
   * current frame. The magnitude of the value indicates how far the drawing is from the current
   * frame (either in absolute frames, or in number of keyframes). */
  const int onion_id;
};
struct MutableDrawingInfo {
  bke::greasepencil::Drawing &drawing;
  const int layer_index;
  const int frame_number;
  const float multi_frame_falloff;
};
Vector<MutableDrawingInfo> retrieve_editable_drawings(const Scene &scene,
                                                      GreasePencil &grease_pencil);
Vector<MutableDrawingInfo> retrieve_editable_drawings_with_falloff(const Scene &scene,
                                                                   GreasePencil &grease_pencil);
Array<Vector<MutableDrawingInfo>> retrieve_editable_drawings_grouped_per_frame(
    const Scene &scene, GreasePencil &grease_pencil);
Vector<MutableDrawingInfo> retrieve_editable_drawings_from_layer(
    const Scene &scene, GreasePencil &grease_pencil, const bke::greasepencil::Layer &layer);
Vector<DrawingInfo> retrieve_visible_drawings(const Scene &scene,
                                              const GreasePencil &grease_pencil,
                                              bool do_onion_skinning);

IndexMask retrieve_editable_strokes(Object &grease_pencil_object,
                                    const bke::greasepencil::Drawing &drawing,
                                    IndexMaskMemory &memory);
IndexMask retrieve_editable_strokes_by_material(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                const int mat_i,
                                                IndexMaskMemory &memory);
IndexMask retrieve_editable_points(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory);
IndexMask retrieve_editable_elements(Object &object,
                                     const bke::greasepencil::Drawing &drawing,
                                     bke::AttrDomain selection_domain,
                                     IndexMaskMemory &memory);

IndexMask retrieve_visible_strokes(Object &grease_pencil_object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory);
IndexMask retrieve_visible_points(Object &object,
                                  const bke::greasepencil::Drawing &drawing,
                                  IndexMaskMemory &memory);

IndexMask retrieve_editable_and_selected_strokes(Object &grease_pencil_object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 IndexMaskMemory &memory);
IndexMask retrieve_editable_and_selected_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                IndexMaskMemory &memory);
IndexMask retrieve_editable_and_selected_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  bke::AttrDomain selection_domain,
                                                  IndexMaskMemory &memory);

void create_blank(Main &bmain, Object &object, int frame_number);
void create_stroke(Main &bmain, Object &object, const float4x4 &matrix, int frame_number);
void create_suzanne(Main &bmain, Object &object, const float4x4 &matrix, int frame_number);

/**
 * An implementation of the Ramer-Douglas-Peucker algorithm.
 *
 * \param range: The range to simplify.
 * \param epsilon: The threshold distance from the coord between two points for when a point
 * in-between needs to be kept.
 * \param dist_function: A function that computes the distance to a point at an index in the range.
 * The IndexRange is a subrange of \a range and the index is an index relative to the subrange.
 * \param points_to_delete: Writes true to the indices for which the points should be removed.
 * \returns the total number of points to remove.
 */
int64_t ramer_douglas_peucker_simplify(IndexRange range,
                                       float epsilon,
                                       FunctionRef<float(int64_t, int64_t, int64_t)> dist_function,
                                       MutableSpan<bool> points_to_delete);

Array<float2> polyline_fit_curve(Span<float2> points,
                                 float error_threshold,
                                 const IndexMask &corner_mask);

IndexMask polyline_detect_corners(Span<float2> points,
                                  float radius_min,
                                  float radius_max,
                                  int samples_max,
                                  float angle_threshold,
                                  IndexMaskMemory &memory);

/**
 * Structure describing a point in the destination relatively to the source.
 * If a point in the destination \a is_src_point, then it corresponds
 * exactly to the point at \a src_point index in the source geometry.
 * Otherwise, it is a linear combination of points at \a src_point and \a src_next_point in the
 * source geometry, with the given \a factor.
 * A point in the destination is a \a cut if it splits the source curves geometry, meaning it is
 * the first point of a new curve in the destination.
 */
struct PointTransferData {
  int src_point;
  int src_next_point;
  float factor;
  bool is_src_point;
  bool is_cut;

  /**
   * Source point is the last of the curve.
   */
  bool is_src_end_point() const
  {
    /* The src_next_point index increments for all points except the last, where it is set to the
     * first point index. This can be used to detect the curve end from the source index alone.
     */
    return is_src_point && src_point >= src_next_point;
  }
};

/**
 * Computes a \a dst curves geometry by applying a change of topology from a \a src curves
 * geometry.
 * The change of topology is described by \a src_to_dst_points, which size should be
 * equal to the number of points in the source.
 * For each point in the source, the corresponding vector in \a src_to_dst_points contains a set
 * of destination points (PointTransferData), which can correspond to points of the source, or
 * linear combination of them. Note that this vector can be empty, if we want to remove points
 * for example. Curves can also be split if a destination point is marked as a cut.
 *
 * \returns an array containing the same elements as \a src_to_dst_points, but in the destination
 * points domain.
 */
Array<PointTransferData> compute_topology_change(
    const bke::CurvesGeometry &src,
    bke::CurvesGeometry &dst,
    const Span<Vector<PointTransferData>> src_to_dst_points,
    const bool keep_caps);

/** Returns a set of vertex group names that are deformed by a bone in an armature. */
Set<std::string> get_bone_deformed_vertex_group_names(const Object &object);

/** For a point in a stroke, normalize the weights of vertex groups deformed by bones so that the
 * sum is 1.0f. */
void normalize_vertex_weights(MDeformVert &dvert,
                              int active_vertex_group,
                              Span<bool> vertex_group_is_locked,
                              Span<bool> vertex_group_is_bone_deformed);

}  // namespace blender::ed::greasepencil
