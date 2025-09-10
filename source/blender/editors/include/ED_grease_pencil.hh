/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_grease_pencil.hh"

#include "BKE_attribute_filter.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "ED_keyframes_edit.hh"
#include "ED_select_utils.hh"

#include "WM_api.hh"

struct bContext;
struct BrushColorJitterSettings;
struct BrushGpencilSettings;
struct Main;
struct Object;
struct KeyframeEditData;
struct MDeformVert;
struct wmKeyConfig;
struct wmOperator;
struct GPUOffScreen;
struct ToolSettings;
struct Scene;
struct UndoType;
struct ViewDepths;
struct View3D;
struct ViewContext;
struct BVHTree;
struct GreasePencilLineartModifierData;
struct RV3DMatrixStore;

namespace blender {
class RandomNumberGenerator;
namespace bke {
enum class AttrDomain : int8_t;
class CurvesGeometry;
}  // namespace bke
}  // namespace blender

enum {
  LAYER_REORDER_ABOVE,
  LAYER_REORDER_BELOW,
};

/* -------------------------------------------------------------------- */
/** \name C Wrappers
 * \{ */

/**
 * Join selected objects. Called from #OBJECT_OT_join.
 */
wmOperatorStatus ED_grease_pencil_join_objects_exec(bContext *C, wmOperator *op);

void ED_operatortypes_grease_pencil();
void ED_operatortypes_grease_pencil_draw();
void ED_operatortypes_grease_pencil_frames();
void ED_operatortypes_grease_pencil_layers();
void ED_operatortypes_grease_pencil_select();
void ED_operatortypes_grease_pencil_edit();
void ED_operatortypes_grease_pencil_join();
void ED_operatortypes_grease_pencil_material();
void ED_operatortypes_grease_pencil_modes();
void ED_operatortypes_grease_pencil_pen();
void ED_operatortypes_grease_pencil_primitives();
void ED_operatortypes_grease_pencil_weight_paint();
void ED_operatortypes_grease_pencil_vertex_paint();
void ED_operatortypes_grease_pencil_interpolate();
void ED_operatortypes_grease_pencil_lineart();
void ED_operatortypes_grease_pencil_trace();
void ED_operatortypes_grease_pencil_bake_animation();
void ED_operatormacros_grease_pencil();
void ED_keymap_grease_pencil(wmKeyConfig *keyconf);
void ED_primitivetool_modal_keymap(wmKeyConfig *keyconf);
void ED_filltool_modal_keymap(wmKeyConfig *keyconf);
void ED_interpolatetool_modal_keymap(wmKeyConfig *keyconf);
void ED_grease_pencil_pentool_modal_keymap(wmKeyConfig *keyconf);

void GREASE_PENCIL_OT_stroke_trim(wmOperatorType *ot);

void ED_undosys_type_grease_pencil(UndoType *ut);

/**
 * Get the selection mode for Grease Pencil selection operators: point, stroke, segment.
 */
blender::bke::AttrDomain ED_grease_pencil_edit_selection_domain_get(
    const ToolSettings *tool_settings);
blender::bke::AttrDomain ED_grease_pencil_sculpt_selection_domain_get(
    const ToolSettings *tool_settings);
blender::bke::AttrDomain ED_grease_pencil_vertex_selection_domain_get(
    const ToolSettings *tool_settings);
blender::bke::AttrDomain ED_grease_pencil_selection_domain_get(const ToolSettings *tool_settings,
                                                               const Object *object);
/**
 * True if any vertex mask selection is used.
 */
bool ED_grease_pencil_any_vertex_mask_selection(const ToolSettings *tool_settings);

/**
 * True if segment selection is enabled.
 */
bool ED_grease_pencil_edit_segment_selection_enabled(const ToolSettings *tool_settings);
bool ED_grease_pencil_sculpt_segment_selection_enabled(const ToolSettings *tool_settings);
bool ED_grease_pencil_vertex_segment_selection_enabled(const ToolSettings *tool_settings);
bool ED_grease_pencil_segment_selection_enabled(const ToolSettings *tool_settings,
                                                const Object *object);

/** \} */

namespace blender::ed::greasepencil {

enum class ReprojectMode : int8_t { Front, Side, Top, View, Cursor, Surface, Keep };

enum class DrawingPlacementDepth : int8_t { ObjectOrigin, Cursor, Surface, Stroke };

enum class DrawingPlacementPlane : int8_t { View, Front, Side, Top, Cursor };

class DrawingPlacement {
  const ARegion *region_;
  const View3D *view3d_;

  DrawingPlacementDepth depth_;
  DrawingPlacementPlane plane_;
  ViewDepths *depth_cache_ = nullptr;
  bool use_project_only_selected_ = false;
  float surface_offset_;

  float3 placement_loc_;
  float3 placement_normal_;
  /* Optional explicit placement plane. */
  std::optional<float4> placement_plane_;

  float4x4 layer_space_to_world_space_;
  float4x4 world_space_to_layer_space_;

 public:
  DrawingPlacement() = default;
  DrawingPlacement(const Scene &scene,
                   const ARegion &region,
                   const View3D &view3d,
                   const Object &eval_object,
                   const bke::greasepencil::Layer *layer);

  /**
   * Construct the object based on a ReprojectMode enum instead of Scene values.
   */
  DrawingPlacement(const Scene &scene,
                   const ARegion &region,
                   const View3D &view3d,
                   const Object &eval_object,
                   const bke::greasepencil::Layer *layer,
                   ReprojectMode reproject_mode,
                   float surface_offset = 0.0f,
                   ViewDepths *view_depths = nullptr);
  DrawingPlacement(const DrawingPlacement &other);
  DrawingPlacement(DrawingPlacement &&other);
  DrawingPlacement &operator=(const DrawingPlacement &other);
  DrawingPlacement &operator=(DrawingPlacement &&other);
  ~DrawingPlacement();

  bool use_project_to_surface() const;
  bool use_project_to_stroke() const;

  void cache_viewport_depths(Depsgraph *depsgraph, ARegion *region, View3D *view3d);

  /**
   * Attempt to project from the depth buffer.
   * \return Un-projected position if a valid depth is found at the screen position.
   */
  std::optional<float3> project_depth(float2 co) const;

  /**
   * Projects a screen space coordinate to the local drawing space.
   */
  float3 project(float2 co, bool &clipped) const;
  float3 project(float2 co) const;
  void project(Span<float2> src, MutableSpan<float3> dst) const;
  /**
   * Projects a screen space coordinate to the local drawing space including camera shift.
   */
  float3 project_with_shift(float2 co) const;

  /**
   * Convert a screen space coordinate with depth to the local drawing space.
   */
  float3 place(float2 co, float depth) const;

  /**
   * Projects a 3D position (in local space) to the drawing plane.
   */
  float3 reproject(float3 pos) const;
  void reproject(Span<float3> src, MutableSpan<float3> dst) const;

  float4x4 to_world_space() const;

  /** Return depth buffer if possible. */
  std::optional<float> get_depth(float2 co) const;

 private:
  /** Return depth buffer projection if possible or "View" placement fallback. */
  float3 try_project_depth(float2 co) const;
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
    blender::bke::greasepencil::FramesMapKeyT frame_number;
    bke::greasepencil::Drawing drawing;
    int duration;
    eBezTriple_KeyframeType keytype;
  };

  struct LayerBufferItem {
    Vector<DrawingBufferItem> drawing_buffers;
    blender::bke::greasepencil::FramesMapKeyT first_frame;
    blender::bke::greasepencil::FramesMapKeyT last_frame;
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

bool grease_pencil_layer_parent_set(bke::greasepencil::Layer &layer,
                                    Object *parent,
                                    StringRefNull bone,
                                    bool keep_transform);

void grease_pencil_layer_parent_clear(bke::greasepencil::Layer &layer, bool keep_transform);

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
bool ensure_active_keyframe(const Scene &scene,
                            GreasePencil &grease_pencil,
                            bke::greasepencil::Layer &layer,
                            bool duplicate_previous_key,
                            bool &r_inserted_keyframe);

void create_keyframe_edit_data_selected_frames_list(KeyframeEditData *ked,
                                                    const bke::greasepencil::Layer &layer);

bool grease_pencil_context_poll(bContext *C);
bool active_grease_pencil_poll(bContext *C);
bool active_grease_pencil_material_poll(bContext *C);
bool editable_grease_pencil_poll(bContext *C);
bool editable_grease_pencil_with_region_view3d_poll(bContext *C);
bool active_grease_pencil_layer_poll(bContext *C);
bool active_grease_pencil_layer_group_poll(bContext *C);
bool editable_grease_pencil_point_selection_poll(bContext *C);
bool grease_pencil_selection_poll(bContext *C);
bool grease_pencil_painting_poll(bContext *C);
bool grease_pencil_edit_poll(bContext *C);
bool grease_pencil_sculpting_poll(bContext *C);
bool grease_pencil_weight_painting_poll(bContext *C);
bool grease_pencil_vertex_painting_poll(bContext *C);

float opacity_from_input_sample(const float pressure,
                                const Brush *brush,
                                const BrushGpencilSettings *settings);
float radius_from_input_sample(const RegionView3D *rv3d,
                               const ARegion *region,
                               const Brush *brush,
                               float pressure,
                               const float3 &location,
                               const float4x4 &to_world,
                               const BrushGpencilSettings *settings);
wmOperatorStatus grease_pencil_draw_operator_invoke(bContext *C,
                                                    wmOperator *op,
                                                    bool use_duplicate_previous_key);
float4x2 calculate_texture_space(const Scene *scene,
                                 const ARegion *region,
                                 const float2 &mouse,
                                 const DrawingPlacement &placement);

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
Vector<MutableDrawingInfo> retrieve_editable_drawings_from_layer_with_falloff(
    const Scene &scene, GreasePencil &grease_pencil, const bke::greasepencil::Layer &layer);
Vector<DrawingInfo> retrieve_visible_drawings(const Scene &scene,
                                              const GreasePencil &grease_pencil,
                                              bool do_onion_skinning);

IndexMask retrieve_editable_strokes(Object &grease_pencil_object,
                                    const bke::greasepencil::Drawing &drawing,
                                    int layer_index,
                                    IndexMaskMemory &memory);
IndexMask retrieve_editable_fill_strokes(Object &grease_pencil_object,
                                         const bke::greasepencil::Drawing &drawing,
                                         int layer_index,
                                         IndexMaskMemory &memory);
IndexMask retrieve_editable_strokes_by_material(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                const int mat_i,
                                                IndexMaskMemory &memory);
IndexMask retrieve_editable_points(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   int layer_index,
                                   IndexMaskMemory &memory);
IndexMask retrieve_editable_elements(Object &object,
                                     const MutableDrawingInfo &info,
                                     bke::AttrDomain selection_domain,
                                     IndexMaskMemory &memory);

IndexMask retrieve_visible_strokes(Object &grease_pencil_object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory);
IndexMask retrieve_visible_points(Object &object,
                                  const bke::greasepencil::Drawing &drawing,
                                  IndexMaskMemory &memory);

IndexMask retrieve_visible_bezier_strokes(Object &object,
                                          const bke::greasepencil::Drawing &drawing,
                                          IndexMaskMemory &memory);
IndexMask retrieve_visible_bezier_points(Object &object,
                                         const bke::greasepencil::Drawing &drawing,
                                         IndexMaskMemory &memory);

IndexMask retrieve_visible_bezier_handle_strokes(Object &object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 int handle_display,
                                                 IndexMaskMemory &memory);
IndexMask retrieve_visible_bezier_handle_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                int layer_index,
                                                int handle_display,
                                                IndexMaskMemory &memory);
IndexMask retrieve_visible_bezier_handle_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  int layer_index,
                                                  bke::AttrDomain selection_domain,
                                                  int handle_display,
                                                  IndexMaskMemory &memory);

IndexMask retrieve_editable_and_selected_strokes(Object &grease_pencil_object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 int layer_index,
                                                 IndexMaskMemory &memory);
IndexMask retrieve_editable_and_selected_fill_strokes(Object &grease_pencil_object,
                                                      const bke::greasepencil::Drawing &drawing,
                                                      int layer_index,
                                                      IndexMaskMemory &memory);
IndexMask retrieve_editable_and_selected_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                int layer_index,
                                                IndexMaskMemory &memory);
IndexMask retrieve_editable_and_selected_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  int layer_index,
                                                  bke::AttrDomain selection_domain,
                                                  IndexMaskMemory &memory);
IndexMask retrieve_editable_and_all_selected_points(Object &object,
                                                    const bke::greasepencil::Drawing &drawing,
                                                    int layer_index,
                                                    int handle_display,
                                                    IndexMaskMemory &memory);
bool has_editable_layer(const GreasePencil &grease_pencil);

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
 * Merge points that are close together on each selected curve.
 * Points are not merged across curves.
 */
bke::CurvesGeometry curves_merge_by_distance(const bke::CurvesGeometry &src_curves,
                                             const float merge_distance,
                                             const IndexMask &selection,
                                             const bke::AttributeFilter &attribute_filter);

/**
 * Merge points on the same curve that are close together.
 */
int curve_merge_by_distance(const IndexRange points,
                            const Span<float> distances,
                            const IndexMask &selection,
                            const float merge_distance,
                            MutableSpan<int> r_merge_indices);

/**
 * Connect selected curve endpoints with the closest endpoints of other curves.
 */
bke::CurvesGeometry curves_merge_endpoints_by_distance(
    const ARegion &region,
    const bke::CurvesGeometry &src_curves,
    const float4x4 &layer_to_world,
    const float merge_distance,
    const IndexMask &selection,
    const bke::AttributeFilter &attribute_filter);

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
  /* Additional attributes changes that can be stored to be used after a call to
   * compute_topology_change.
   * Note that they won't be automatically updated in the destination's attributes.
   */
  float opacity;

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

/** Adds vertex groups for the bones in the armature (with matching names). */
bool add_armature_vertex_groups(Object &object, const Object &armature);
/** Create vertex groups for the bones in the armature and use the bone envelopes to assign
 * weights. */
void add_armature_envelope_weights(Scene &scene, Object &object, const Object &ob_armature);
/** Create vertex groups for the bones in the armature and use a simple distance based algorithm to
 * assign automatic weights. */
void add_armature_automatic_weights(Scene &scene, Object &object, const Object &ob_armature);

void clipboard_free();
/**
 * Paste all the strokes in the clipboard layers into \a drawing.
 */
IndexRange paste_all_strokes_from_clipboard(Main &bmain,
                                            Object &object,
                                            const float4x4 &object_to_paste_layer,
                                            bool keep_world_transform,
                                            bool paste_back,
                                            bke::greasepencil::Drawing &drawing);

/**
 * Method used by the Fill tool to fit the render buffer to strokes.
 */
enum FillToolFitMethod {
  /* Use the current view projection unchanged. */
  None,
  /* Fit all strokes into the view (may change pixel size). */
  FitToView,
};

struct ExtensionData {
  struct {
    Vector<float3> starts;
    Vector<float3> ends;
  } lines;
  struct {
    Vector<float3> centers;
    Vector<float> radii;
  } circles;
};

/**
 * Fill tool for generating strokes in empty areas.
 *
 * This uses an approximate render of strokes and boundaries,
 * then fills the image starting from the mouse position.
 * The outlines of the filled pixel areas are returned as curves.
 *
 * \param layer: The layer containing the new stroke, used for reprojecting from images.
 * \param boundary_layers: Layers that are purely for boundaries, regular strokes are not rendered.
 * \param src_drawings: Drawings to include as boundary strokes.
 * \param invert: Construct boundary around empty areas instead.
 * \param alpha_threshold: Render transparent stroke where opacity is below the threshold.
 * \param fill_point: Point from which to start the bucket fill.
 * \param fit_method: View fitting method to include all strokes.
 * \param stroke_material_index: Material index to use for the new strokes.
 * \param keep_images: Keep the image data block after generating curves.
 */
bke::CurvesGeometry fill_strokes(const ViewContext &view_context,
                                 const Brush &brush,
                                 const Scene &scene,
                                 const bke::greasepencil::Layer &layer,
                                 const VArray<bool> &boundary_layers,
                                 Span<DrawingInfo> src_drawings,
                                 bool invert,
                                 const std::optional<float> alpha_threshold,
                                 const float2 &fill_point,
                                 const ExtensionData &extensions,
                                 FillToolFitMethod fit_method,
                                 int stroke_material_index,
                                 bool keep_images);

namespace image_render {

/** Region size to restore after rendering. */
struct RegionViewData {
  int2 winsize;
  rcti winrct;
  RV3DMatrixStore *rv3d_store;
};

/**
 * Set up region to match the render buffer size.
 */
RegionViewData region_init(ARegion &region, const int2 &win_size);
/**
 * Restore original region size after rendering.
 */
void region_reset(ARegion &region, const RegionViewData &data);

/**
 * Create and off-screen buffer for rendering.
 */
GPUOffScreen *image_render_begin(const int2 &win_size);
/**
 * Finish rendering and convert the off-screen buffer into an image.
 */
Image *image_render_end(Main &bmain, GPUOffScreen *buffer);

/**
 * Set up the view matrix for world space rendering.
 *
 * \param win_size: Size of the render window.
 * \param zoom: Zoom factor to render a smaller or larger part of the view.
 * \param offset: Offset of the view relative to the overall size.
 */
void compute_view_matrices(const ViewContext &view_context,
                           const Scene &scene,
                           const int2 &win_size,
                           const float2 &zoom,
                           const float2 &offset);

void set_view_matrix(const RegionView3D &rv3d);
void clear_view_matrix();
void set_projection_matrix(const RegionView3D &rv3d);
void clear_projection_matrix();

/**
 * Draw a dot with a given size and color.
 */
void draw_dot(const float4x4 &transform,
              const float3 &position,
              float point_size,
              const ColorGeometry4f &color);

/**
 * Draw a poly line from points.
 */
void draw_polyline(const float4x4 &transform,
                   IndexRange indices,
                   Span<float3> positions,
                   const VArray<ColorGeometry4f> &colors,
                   bool cyclic,
                   float line_width);

/**
 * Draw points as circles.
 */
void draw_circles(const float4x4 &transform,
                  const IndexRange indices,
                  Span<float3> centers,
                  const VArray<float> &radii,
                  const VArray<ColorGeometry4f> &colors,
                  const float2 &viewport_size,
                  const float line_width,
                  const bool fill);

/**
 * Draw lines with start and end points.
 */
void draw_lines(const float4x4 &transform,
                IndexRange indices,
                Span<float3> start_positions,
                Span<float3> end_positions,
                const VArray<ColorGeometry4f> &colors,
                float line_width);

/**
 * Draw curves geometry.
 * \param mode: Mode of \a eMaterialGPencilStyle_Mode.
 */
void draw_grease_pencil_strokes(const RegionView3D &rv3d,
                                const int2 &win_size,
                                const Object &object,
                                const bke::greasepencil::Drawing &drawing,
                                const float4x4 &transform,
                                const IndexMask &strokes_mask,
                                const VArray<ColorGeometry4f> &colors,
                                bool use_xray,
                                float radius_scale = 1.0f);

}  // namespace image_render

enum class InterpolateFlipMode : int8_t {
  /* No flip. */
  None = 0,
  /* Flip always. */
  Flip,
  /* Flip if needed. */
  FlipAuto,
};

enum class InterpolateLayerMode : int8_t {
  /* Only interpolate on the active layer. */
  Active = 0,
  /* Interpolate strokes on every layer. */
  All,
};

/**
 * Create new strokes tracing the rendered outline of existing strokes.
 * \param drawing: Drawing with input strokes.
 * \param strokes: Selection curves to trace.
 * \param transform: Transform to apply to strokes.
 * \param corner_subdivisions: Subdivisions for corners and start/end cap.
 * \param outline_radius: Radius of the new outline strokes.
 * \param outline_offset: Offset of the outline from the original stroke.
 * \param material_index: The material index for the new outline strokes.
 */
bke::CurvesGeometry create_curves_outline(const bke::greasepencil::Drawing &drawing,
                                          const IndexMask &strokes,
                                          const float4x4 &transform,
                                          int corner_subdivisions,
                                          float outline_radius,
                                          float outline_offset,
                                          int material_index);

/* Function that generates an update mask for a selection operation. */
using SelectionUpdateFunc = FunctionRef<IndexMask(const ed::greasepencil::MutableDrawingInfo &info,
                                                  const IndexMask &universe,
                                                  StringRef attribute_name,
                                                  IndexMaskMemory &memory)>;

bool selection_update(const ViewContext *vc,
                      const eSelectOp sel_op,
                      SelectionUpdateFunc select_operation);

/* BVHTree and associated data for 2D curve projection. */
struct Curves2DBVHTree {
  BVHTree *tree = nullptr;
  /* Projected coordinates for each tree element. */
  Array<float2> start_positions;
  Array<float2> end_positions;
  /* BVH element index range for each drawing. */
  Array<int> drawing_offsets;
};

/**
 * Construct a 2D BVH tree from the screen space line segments of visible curves.
 */
Curves2DBVHTree build_curves_2d_bvh_from_visible(const ViewContext &vc,
                                                 const Object &object,
                                                 const GreasePencil &grease_pencil,
                                                 Span<MutableDrawingInfo> drawings,
                                                 int frame_number);
void free_curves_2d_bvh_data(Curves2DBVHTree &data);

/**
 * Find intersections between curves and accurate cut positions.
 *
 * Note: Index masks for target and intersecting curves can have any amount of overlap,
 * including equal or fully separate masks. A curve can be self-intersecting by being in both
 * masks.
 *
 * \param curves: Curves geometry for both target and cutter curves.
 * \param screen_space_positions: Screen space positions computed in advance.
 * \param target_curves: Set of curves that will be intersected.
 * \param intersecting_curves: Set of curves that create cuts on target curves.
 * \param r_hits: True for points with at least one intersection.
 * \param r_first_intersect_factors: Smallest cut factor in the interval (optional).
 * \param r_last_intersect_factors: Largest cut factor in the interval (optional).
 */
void find_curve_intersections(const bke::CurvesGeometry &curves,
                              const IndexMask &curve_mask,
                              const Span<float2> screen_space_positions,
                              const Curves2DBVHTree &tree_data,
                              IndexRange tree_data_range,
                              MutableSpan<bool> r_hits,
                              std::optional<MutableSpan<float>> r_first_intersect_factors,
                              std::optional<MutableSpan<float>> r_last_intersect_factors);

/**
 * Segmentation of curves into fractional ranges.
 *
 * Segments are defined by a point index and a fraction of the following line segment. The actual
 * start point is found by interpolating between the start point and the next point on the curve. A
 * curve can have no segments at all, in which case the full curve is cyclic and has a single
 * segment. Segments can start and end on the same point, making them shorter than a line segment.
 * A curve is fully partitioned into segments, each segment ends at the start of the next segment
 * with no gaps. The last segment is wrapped around to connect to the first segment.
 *
 * curves:   0---------------1-----------------------2-------
 * points:   0       1       2       3       4       5
 * segments: ┌>0────>1──────┐┌──>2────────────>3──>4┐┌─────>┐
 *           └──────────────┘└──────────────────────┘└──────┘
 *
 * segment_offsets = [0, 2, 5]
 * segment_start_points = [0, 1, 2, 4, 4]
 * segment_start_fractions = [.25, .0, .5, .25, .75]
 */
struct CurveSegmentsData {
  /* Segment start index for each curve, can be used as \a OffsetIndices. */
  Array<int> segment_offsets;
  /* Point indices where new segments start. */
  Array<int> segment_start_points;
  /* Fraction of the start point on the line segment to the next point. */
  Array<float> segment_start_fractions;
};

/**
 * Find segments between intersections.
 *
 * Note: Index masks for target and intersecting curves can have any amount of overlap,
 * including equal or fully separate masks. A curve can be self-intersecting by being in both
 * masks.
 *
 * \param curves: Curves geometry for both target and cutter curves.
 * \param curve_mask: Set of curves that will be intersected.
 * \param screen_space_positions: Screen space positions computed in advance.
 * \param tree_data: Screen-space BVH tree of the intersecting curves.
 * \param r_curve_starts: Start index for segments of each curve.
 *        Shift the curve points index range to ensure contiguous segments with cyclic curves.
 * \param r_segments_by_curve: Offsets for segments in each curve.
 * \param r_points_by_segment: Offsets for point range of each segment. Index ranges can exceed
 *        original curve range and must be wrapped around.
 * \param r_start_factors: Factor (-1..0) previous segment to prepend.
 * \param r_end_factors: Factor (0..1) of last segment to append.
 */
CurveSegmentsData find_curve_segments(const bke::CurvesGeometry &curves,
                                      const IndexMask &curve_mask,
                                      const Span<float2> screen_space_positions,
                                      const Curves2DBVHTree &tree_data,
                                      IndexRange tree_data_range);

bool apply_mask_as_selection(bke::CurvesGeometry &curves,
                             const IndexMask &selection,
                             bke::AttrDomain selection_domain,
                             StringRef attribute_name,
                             GrainSize grain_size,
                             eSelectOp sel_op);

bool apply_mask_as_segment_selection(bke::CurvesGeometry &curves,
                                     const IndexMask &point_selection,
                                     StringRef attribute_name,
                                     const Curves2DBVHTree &tree_data,
                                     IndexRange tree_data_range,
                                     GrainSize grain_size,
                                     eSelectOp sel_op);

namespace trim {
bke::CurvesGeometry trim_curve_segments(const bke::CurvesGeometry &src,
                                        Span<float2> screen_space_positions,
                                        Span<rcti> screen_space_curve_bounds,
                                        const IndexMask &curve_selection,
                                        const Vector<Vector<int>> &selected_points_in_curves,
                                        bool keep_caps);
};  // namespace trim

void merge_layers(const GreasePencil &src_grease_pencil,
                  const Span<Vector<int>> src_layer_indices_by_dst_layer,
                  GreasePencil &dst_grease_pencil);

/* Lineart */

/* Stores the maximum calculation range in the whole modifier stack for line art so the cache can
 * cover everything that will be visible. */
struct LineartLimitInfo {
  int16_t edge_types;
  uint8_t min_level;
  uint8_t max_level;
  uint8_t shadow_selection;
  uint8_t silhouette_selection;
};

void get_lineart_modifier_limits(const Object &ob, LineartLimitInfo &info);
void set_lineart_modifier_limits(GreasePencilLineartModifierData &lmd,
                                 const LineartLimitInfo &info,
                                 const bool cache_is_ready);

GreasePencilLineartModifierData *get_first_lineart_modifier(const Object &ob);

GreasePencil *from_context(bContext &C);

/* Make sure selection domain is updated to match the current selection mode. */
bool ensure_selection_domain(ToolSettings *ts, Object *object);

/**
 * Creates a new curve with one point at the beginning or end.
 * \note Does not initialize the new curve or points.
 */
void add_single_curve(bke::CurvesGeometry &curves, bool at_end);

/**
 * Resize the first or last curve to `new_points_num` number of points.
 * \note Does not initialize the new points.
 */
void resize_single_curve(bke::CurvesGeometry &curves, bool at_end, int new_points_num);

/**
 * Calculate a randomized radius value for a point.
 * \param stroke_factor: Random seed value in [-1, 1] per stroke.
 * \param distance: Screen-space length in pixels along the curve.
 * \param radius: Base radius to be randomized.
 * \param pressure: Pressure factor.
 */
float randomize_radius(const BrushGpencilSettings &settings,
                       float stroke_factor,
                       float distance,
                       float radius,
                       float pressure);
/**
 * Calculate a randomized opacity value for a point.
 * \param stroke_factor: Random seed value in [-1, 1] per stroke.
 * \param distance: Screen-space length in pixels along the curve.
 * \param opacity: Base opacity to be randomized.
 * \param pressure: Pressure factor.
 */
float randomize_opacity(const BrushGpencilSettings &settings,
                        float stroke_factor,
                        float distance,
                        float opacity,
                        float pressure);
/**
 * Calculate a randomized rotation for a point.
 * \param stroke_factor: Random seed value in [-1, 1] per stroke.
 * \param distance: Screen-space length in pixels along the curve.
 * \param pressure: Pressure factor.
 */
float randomize_rotation(const BrushGpencilSettings &settings,
                         float stroke_factor,
                         float distance,
                         float pressure);
/**
 * Calculate a randomized rotation for a point.
 * \param rng: Random number generator instance.
 * \param stroke_factor: Random seed value in [-1, 1] per stroke.
 * \param pressure: Pressure factor.
 */
float randomize_rotation(const BrushGpencilSettings &settings,
                         blender::RandomNumberGenerator &rng,
                         float stroke_factor,
                         float pressure);
/**
 * Calculate a randomized opacity value for a point.
 * \param stroke_hue_factor: Random seed value in [-1, 1] per stroke for color hue.
 * \param stroke_saturation_factor: Random seed value in [-1, 1] per stroke for color saturation.
 * \param stroke_value_factor: Random seed value in [-1, 1] per stroke for color value.
 * \param distance: Screen-space length in pixels along the curve.
 * \param color: Base color to be randomized.
 * \param pressure: Pressure factor.
 */
ColorGeometry4f randomize_color(const BrushGpencilSettings &settings,
                                const std::optional<BrushColorJitterSettings> &jitter,
                                float stroke_hue_factor,
                                float stroke_saturation_factor,
                                float stroke_value_factor,
                                float distance,
                                ColorGeometry4f color,
                                float pressure);

/**
 * Applies the \a eval_grease_pencil onto the \a orig_grease_pencil at the \a eval_frame.
 * The \a orig_grease_pencil is modified in-place.
 * The mapping between the layers is created based on the layer name.
 * \param eval_grease_pencil: The source Grease Pencil data.
 * \param eval_frame: The frame at which to apply the data.
 * \param orig_layers: Selection of original layers to modify.
 * \param orig_grease_pencil: The destination Grease Pencil data.
 */
void apply_eval_grease_pencil_data(const GreasePencil &eval_grease_pencil,
                                   int eval_frame,
                                   const IndexMask &orig_layers,
                                   GreasePencil &orig_grease_pencil);

/**
 * Remove all the strokes that are marked as fill guides.
 */
bool remove_fill_guides(bke::CurvesGeometry &curves);

}  // namespace blender::ed::greasepencil
