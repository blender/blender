/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.hh"

#include "GEO_merge_layers.hh"

#include "RNA_prototypes.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

namespace blender::ed::greasepencil {

DrawingPlacement::DrawingPlacement(const Scene &scene,
                                   const ARegion &region,
                                   const View3D &view3d,
                                   const Object &eval_object,
                                   const bke::greasepencil::Layer *layer)
    : region_(&region), view3d_(&view3d)
{
  layer_space_to_world_space_ = (layer != nullptr) ? layer->to_world_space(eval_object) :
                                                     eval_object.object_to_world();
  world_space_to_layer_space_ = math::invert(layer_space_to_world_space_);
  /* Initialize DrawingPlacementPlane from toolsettings. */
  switch (scene.toolsettings->gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_VIEW:
      plane_ = DrawingPlacementPlane::View;
      break;
    case GP_LOCKAXIS_Y:
      plane_ = DrawingPlacementPlane::Front;
      placement_normal_ = float3(0, 1, 0);
      break;
    case GP_LOCKAXIS_X:
      plane_ = DrawingPlacementPlane::Side;
      placement_normal_ = float3(1, 0, 0);
      break;
    case GP_LOCKAXIS_Z:
      plane_ = DrawingPlacementPlane::Top;
      placement_normal_ = float3(0, 0, 1);
      break;
    case GP_LOCKAXIS_CURSOR: {
      plane_ = DrawingPlacementPlane::Cursor;
      placement_normal_ = scene.cursor.matrix<float3x3>() * float3(0, 0, 1);
      break;
    }
  }

  /* Account for layer transform. */
  if (!ELEM(scene.toolsettings->gp_sculpt.lock_axis, GP_LOCKAXIS_VIEW, GP_LOCKAXIS_CURSOR)) {
    /* Use the transpose inverse for normal. */
    placement_normal_ = math::transform_direction(math::transpose(world_space_to_layer_space_),
                                                  placement_normal_);
  }

  /* Initialize DrawingPlacementDepth from toolsettings. */
  const char align_flag = scene.toolsettings->gpencil_v3d_align;
  if (align_flag & GP_PROJECT_VIEWSPACE) {
    if (align_flag & GP_PROJECT_CURSOR) {
      depth_ = DrawingPlacementDepth::Cursor;
      surface_offset_ = 0.0f;
      placement_loc_ = float3(scene.cursor.location);
    }
    else if (align_flag & GP_PROJECT_DEPTH_VIEW) {
      depth_ = DrawingPlacementDepth::Surface;
      if (align_flag & GP_PROJECT_DEPTH_ONLY_SELECTED) {
        use_project_only_selected_ = true;
      }
      surface_offset_ = scene.toolsettings->gpencil_surface_offset;
      /* Default to view placement with the object origin if we don't hit a surface. */
      placement_loc_ = layer_space_to_world_space_.location();
    }
    else if (align_flag & GP_PROJECT_DEPTH_STROKE) {
      depth_ = DrawingPlacementDepth::Stroke;
      surface_offset_ = 0.0f;
      /* Default to view placement with the object origin if we don't hit a stroke. */
      placement_loc_ = layer_space_to_world_space_.location();
    }
    else {
      depth_ = DrawingPlacementDepth::ObjectOrigin;
      surface_offset_ = 0.0f;
      placement_loc_ = layer_space_to_world_space_.location();
    }
  }
  else {
    depth_ = DrawingPlacementDepth::ObjectOrigin;
    surface_offset_ = 0.0f;
    placement_loc_ = float3(0.0f);
  }

  if (plane_ != DrawingPlacementPlane::View) {
    placement_plane_ = float4();
    plane_from_point_normal_v3(*placement_plane_, placement_loc_, placement_normal_);
  }
}

DrawingPlacement::DrawingPlacement(const Scene &scene,
                                   const ARegion &region,
                                   const View3D &view3d,
                                   const Object &eval_object,
                                   const bke::greasepencil::Layer *layer,
                                   const ReprojectMode reproject_mode,
                                   const float surface_offset,
                                   ViewDepths *view_depths)
    : region_(&region),
      view3d_(&view3d),
      depth_cache_(view_depths),
      surface_offset_(surface_offset)
{
  layer_space_to_world_space_ = (layer != nullptr) ? layer->to_world_space(eval_object) :
                                                     eval_object.object_to_world();
  world_space_to_layer_space_ = math::invert(layer_space_to_world_space_);
  /* Initialize DrawingPlacementPlane from mode. */
  switch (reproject_mode) {
    case ReprojectMode::View:
      plane_ = DrawingPlacementPlane::View;
      break;
    case ReprojectMode::Front:
      plane_ = DrawingPlacementPlane::Front;
      placement_normal_ = float3(0, 1, 0);
      break;
    case ReprojectMode::Side:
      plane_ = DrawingPlacementPlane::Side;
      placement_normal_ = float3(1, 0, 0);
      break;
    case ReprojectMode::Top:
      plane_ = DrawingPlacementPlane::Top;
      placement_normal_ = float3(0, 0, 1);
      break;
    case ReprojectMode::Cursor: {
      plane_ = DrawingPlacementPlane::Cursor;
      placement_normal_ = scene.cursor.matrix<float3x3>() * float3(0, 0, 1);
      break;
    }
    default:
      break;
  }

  /* Account for layer transform. */
  if (!ELEM(reproject_mode, ReprojectMode::View, ReprojectMode::Cursor)) {
    /* Use the transpose inverse for normal. */
    placement_normal_ = math::transform_direction(math::transpose(world_space_to_layer_space_),
                                                  placement_normal_);
  }

  /* Initialize DrawingPlacementDepth from mode. */
  switch (reproject_mode) {
    case ReprojectMode::Cursor:
      depth_ = DrawingPlacementDepth::Cursor;
      surface_offset_ = 0.0f;
      placement_loc_ = float3(scene.cursor.location);
      break;
    case ReprojectMode::View:
      depth_ = DrawingPlacementDepth::ObjectOrigin;
      surface_offset_ = 0.0f;
      placement_loc_ = layer_space_to_world_space_.location();
      break;
    case ReprojectMode::Surface:
      depth_ = DrawingPlacementDepth::Surface;
      placement_loc_ = layer_space_to_world_space_.location();
      break;
    default:
      depth_ = DrawingPlacementDepth::ObjectOrigin;
      surface_offset_ = 0.0f;
      placement_loc_ = layer_space_to_world_space_.location();
      break;
  }

  if (plane_ != DrawingPlacementPlane::View) {
    placement_plane_ = float4();
    plane_from_point_normal_v3(*placement_plane_, placement_loc_, placement_normal_);
  }
}

DrawingPlacement::DrawingPlacement(const DrawingPlacement &other)
{
  region_ = other.region_;
  view3d_ = other.view3d_;

  depth_ = other.depth_;
  plane_ = other.plane_;

  if (other.depth_cache_ != nullptr) {
    depth_cache_ = static_cast<ViewDepths *>(MEM_dupallocN(other.depth_cache_));
    depth_cache_->depths = static_cast<float *>(MEM_dupallocN(other.depth_cache_->depths));
  }
  use_project_only_selected_ = other.use_project_only_selected_;

  surface_offset_ = other.surface_offset_;

  placement_loc_ = other.placement_loc_;
  placement_normal_ = other.placement_normal_;
  placement_plane_ = other.placement_plane_;

  layer_space_to_world_space_ = other.layer_space_to_world_space_;
  world_space_to_layer_space_ = other.world_space_to_layer_space_;
}

DrawingPlacement::DrawingPlacement(DrawingPlacement &&other)
{
  region_ = other.region_;
  view3d_ = other.view3d_;

  depth_ = other.depth_;
  plane_ = other.plane_;

  std::swap(depth_cache_, other.depth_cache_);
  use_project_only_selected_ = other.use_project_only_selected_;

  surface_offset_ = other.surface_offset_;

  placement_loc_ = other.placement_loc_;
  placement_normal_ = other.placement_normal_;
  placement_plane_ = other.placement_plane_;

  layer_space_to_world_space_ = other.layer_space_to_world_space_;
  world_space_to_layer_space_ = other.world_space_to_layer_space_;
}

DrawingPlacement &DrawingPlacement::operator=(const DrawingPlacement &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) DrawingPlacement(other);
  return *this;
}

DrawingPlacement &DrawingPlacement::operator=(DrawingPlacement &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) DrawingPlacement(std::move(other));
  return *this;
}

DrawingPlacement::~DrawingPlacement()
{
  if (depth_cache_ != nullptr) {
    ED_view3d_depths_free(depth_cache_);
  }
}

bool DrawingPlacement::use_project_to_surface() const
{
  return depth_ == DrawingPlacementDepth::Surface;
}

bool DrawingPlacement::use_project_to_stroke() const
{
  return depth_ == DrawingPlacementDepth::Stroke;
}

void DrawingPlacement::cache_viewport_depths(Depsgraph *depsgraph, ARegion *region, View3D *view3d)
{
  const short previous_gp_flag = view3d->gp_flag;
  eV3DDepthOverrideMode mode = V3D_DEPTH_GPENCIL_ONLY;

  if (use_project_to_surface()) {
    if (use_project_only_selected_) {
      mode = V3D_DEPTH_SELECTED_ONLY;
    }
    else {
      mode = V3D_DEPTH_NO_GPENCIL;
    }
  }
  if (use_project_to_stroke()) {
    /* Enforce render engine to use 3D stroke order, otherwise depth buffer values are not in 3D
     * space. */
    view3d->gp_flag |= V3D_GP_FORCE_STROKE_ORDER_3D;
  }

  ED_view3d_depth_override(depsgraph, region, view3d, nullptr, mode, false, &this->depth_cache_);

  view3d->gp_flag = previous_gp_flag;
}

std::optional<float3> DrawingPlacement::project_depth(const float2 co) const
{
  std::optional<float> depth = get_depth(co);
  if (!depth) {
    return std::nullopt;
  }

  float3 proj_point;
  if (ED_view3d_depth_unproject_v3(region_, int2(co), *depth, proj_point)) {
    float3 view_normal;
    ED_view3d_win_to_vector(region_, co, view_normal);
    proj_point -= view_normal * surface_offset_;
    return proj_point;
  }
  return std::nullopt;
}

std::optional<float> DrawingPlacement::get_depth(float2 co) const
{
  float depth;
  if (depth_cache_ != nullptr && ED_view3d_depth_read_cached(depth_cache_, int2(co), 4, &depth)) {
    return depth;
  }
  return std::nullopt;
}

float3 DrawingPlacement::try_project_depth(const float2 co) const
{
  if (std::optional<float3> proj_point = this->project_depth(co)) {
    return *proj_point;
  }

  float3 proj_point;
  /* Fall back to `View` placement. */
  ED_view3d_win_to_3d(view3d_, region_, placement_loc_, co, proj_point);
  return proj_point;
}

float3 DrawingPlacement::project(const float2 co, bool &r_clipped) const
{
  float3 proj_point;
  if (depth_ == DrawingPlacementDepth::Surface) {
    /* Project using the viewport depth cache. */
    proj_point = this->try_project_depth(co);
    r_clipped = false;
  }
  else {
    if (placement_plane_) {
      r_clipped = !ED_view3d_win_to_3d_on_plane(region_, *placement_plane_, co, true, proj_point);
    }
    else {
      ED_view3d_win_to_3d(view3d_, region_, placement_loc_, co, proj_point);
      r_clipped = false;
    }
  }
  return math::transform_point(world_space_to_layer_space_, proj_point);
}
float3 DrawingPlacement::project(const float2 co) const
{
  [[maybe_unused]] bool clipped_unused;
  return this->project(co, clipped_unused);
}

float3 DrawingPlacement::project_with_shift(const float2 co) const
{
  float3 proj_point;
  if (depth_ == DrawingPlacementDepth::Surface) {
    /* Project using the viewport depth cache. */
    proj_point = this->try_project_depth(co);
  }
  else {
    if (placement_plane_) {
      ED_view3d_win_to_3d_on_plane(region_, *placement_plane_, co, false, proj_point);
    }
    else {
      ED_view3d_win_to_3d_with_shift(view3d_, region_, placement_loc_, co, proj_point);
    }
  }
  return math::transform_point(world_space_to_layer_space_, proj_point);
}

void DrawingPlacement::project(const Span<float2> src, MutableSpan<float3> dst) const
{
  threading::parallel_for(src.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = this->project(src[i]);
    }
  });
}

float3 DrawingPlacement::place(const float2 co, const float depth) const
{
  float3 loc;
  ED_view3d_unproject_v3(region_, co.x, co.y, depth, loc);
  return math::transform_point(world_space_to_layer_space_, loc);
}

float3 DrawingPlacement::reproject(const float3 pos) const
{
  const float3 world_pos = math::transform_point(layer_space_to_world_space_, pos);
  float3 proj_point;
  if (depth_ == DrawingPlacementDepth::Surface) {
    /* First project the position into view space. */
    float2 co;
    if (ED_view3d_project_float_global(region_, world_pos, co, V3D_PROJ_TEST_NOP)) {
      /* Can't reproject the point. */
      return pos;
    }
    /* Project using the viewport depth cache. */
    proj_point = this->try_project_depth(co);
  }
  else {
    /* Reproject the point onto the `placement_plane_` from the current view. */
    RegionView3D *rv3d = static_cast<RegionView3D *>(region_->regiondata);

    float3 ray_no;
    if (rv3d->is_persp) {
      ray_no = math::normalize(world_pos - float3(rv3d->viewinv[3]));
    }
    else {
      ray_no = -float3(rv3d->viewinv[2]);
    }
    float4 plane;
    if (placement_plane_) {
      plane = *placement_plane_;
    }
    else {
      plane_from_point_normal_v3(plane, placement_loc_, rv3d->viewinv[2]);
    }

    float lambda;
    if (isect_ray_plane_v3(world_pos, ray_no, plane, &lambda, false)) {
      proj_point = world_pos + ray_no * lambda;
    }
    else {
      return pos;
    }
  }
  return math::transform_point(world_space_to_layer_space_, proj_point);
}

void DrawingPlacement::reproject(const Span<float3> src, MutableSpan<float3> dst) const
{
  threading::parallel_for(src.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = this->reproject(src[i]);
    }
  });
}

float4x4 DrawingPlacement::to_world_space() const
{
  return layer_space_to_world_space_;
}

static float get_frame_falloff(const bool use_multi_frame_falloff,
                               const int frame_number,
                               const int active_frame,
                               const std::optional<Bounds<int>> frame_bounds,
                               const CurveMapping *falloff_curve)
{
  if (!use_multi_frame_falloff || !frame_bounds.has_value() || falloff_curve == nullptr) {
    return 1.0f;
  }

  const int min_frame = frame_bounds->min;
  const int max_frame = frame_bounds->max;

  /* Frame right of the center frame. */
  if (frame_number < active_frame) {
    const float frame_factor = 0.5f * float(frame_number - min_frame) / (active_frame - min_frame);
    return BKE_curvemapping_evaluateF(falloff_curve, 0, frame_factor);
  }
  /* Frame left of the center frame. */
  if (frame_number > active_frame) {
    const float frame_factor = 0.5f * float(frame_number - active_frame) /
                               (max_frame - active_frame);
    return BKE_curvemapping_evaluateF(falloff_curve, 0, frame_factor + 0.5f);
  }
  /* Frame at center. */
  return BKE_curvemapping_evaluateF(falloff_curve, 0, 0.5f);
}

static std::optional<Bounds<int>> get_selected_frame_number_bounds(
    const bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  if (!layer.is_editable()) {
    return {};
  }
  Vector<int> frame_numbers;
  for (const auto [frame_number, frame] : layer.frames().items()) {
    if (frame.is_selected()) {
      frame_numbers.append(frame_number);
    }
  }
  return bounds::min_max<int>(frame_numbers);
}

static int get_active_frame_for_falloff(const bke::greasepencil::Layer &layer,
                                        const std::optional<Bounds<int>> frame_bounds,
                                        const int current_frame)
{
  std::optional<int> current_start_frame = layer.start_frame_at(current_frame);
  if (!current_start_frame && frame_bounds) {
    return math::clamp(current_frame, frame_bounds->min, frame_bounds->max);
  }
  return *current_start_frame;
}

static std::optional<int> get_frame_id(const bke::greasepencil::Layer &layer,
                                       const GreasePencilFrame &frame,
                                       const int frame_number,
                                       const int frame_index,
                                       const int current_frame,
                                       const int current_frame_index,
                                       const int last_frame,
                                       const int last_frame_index,
                                       const bool use_multi_frame_editing,
                                       const bool do_onion_skinning,
                                       const bool is_before_first,
                                       const GreasePencilOnionSkinningSettings onion_settings)
{
  if (use_multi_frame_editing) {
    if (frame.is_selected()) {
      if (do_onion_skinning) {
        return (frame_number < current_frame) ? -1 : 1;
      }
      return 0;
    }
    return {};
  }
  if (do_onion_skinning && layer.use_onion_skinning()) {
    /* Keyframe type filter. */
    if (onion_settings.filter != 0 && (onion_settings.filter & (1 << frame.type)) == 0) {
      return {};
    }
    /* Selected mode filter. */
    if (onion_settings.mode == GP_ONION_SKINNING_MODE_SELECTED && !frame.is_selected()) {
      return {};
    }

    int delta = 0;
    if (onion_settings.mode == GP_ONION_SKINNING_MODE_ABSOLUTE) {
      delta = frame_number - current_frame;
    }
    else {
      delta = frame_index - current_frame_index;
    }

    if (is_before_first) {
      delta++;
    }
    if ((onion_settings.flag & GP_ONION_SKINNING_SHOW_LOOP) != 0 &&
        (-delta > onion_settings.num_frames_before || delta > onion_settings.num_frames_after))
    {
      /* We wrap the value using the last frame and 0 as reference. */
      /* FIXME: This might not be good for animations not starting at 0. */
      int shift = 0;
      if (onion_settings.mode == GP_ONION_SKINNING_MODE_ABSOLUTE) {
        shift = last_frame;
      }
      else {
        shift = last_frame_index;
      }
      delta += (delta < 0) ? (shift + 1) : -(shift + 1);
    }
    /* Frame range filter. */
    if (ELEM(onion_settings.mode,
             GP_ONION_SKINNING_MODE_ABSOLUTE,
             GP_ONION_SKINNING_MODE_RELATIVE) &&
        (-delta > onion_settings.num_frames_before || delta > onion_settings.num_frames_after))
    {
      return {};
    }

    return delta;
  }
  return {};
}

static Array<std::pair<int, int>> get_visible_frames_for_layer(
    const GreasePencil &grease_pencil,
    const bke::greasepencil::Layer &layer,
    const int current_frame,
    const bool use_multi_frame_editing,
    const bool do_onion_skinning)
{
  GreasePencilOnionSkinningSettings onion_settings = grease_pencil.onion_skinning_settings;
  Vector<std::pair<int, int>> frame_numbers;
  const Span<int> sorted_keys = layer.sorted_keys();
  if (sorted_keys.is_empty()) {
    return {};
  }
  const int current_frame_index = std::max(layer.sorted_keys_index_at(current_frame), 0);
  const int last_frame = sorted_keys.last();
  const int last_frame_index = sorted_keys.index_range().last();
  const bool is_before_first = (current_frame < sorted_keys.first());
  const std::optional<int> current_start_frame = layer.start_frame_at(current_frame);
  for (const int frame_i : sorted_keys.index_range()) {
    const int frame_number = sorted_keys[frame_i];
    if (current_start_frame && *current_start_frame == frame_number) {
      continue;
    }
    const GreasePencilFrame &frame = layer.frames().lookup(frame_number);
    const std::optional<int> frame_id = get_frame_id(layer,
                                                     frame,
                                                     frame_number,
                                                     frame_i,
                                                     current_frame,
                                                     current_frame_index,
                                                     last_frame,
                                                     last_frame_index,
                                                     use_multi_frame_editing,
                                                     do_onion_skinning,
                                                     is_before_first,
                                                     onion_settings);
    if (!frame_id.has_value()) {
      /* Drawing on this frame is not visible. */
      continue;
    }

    frame_numbers.append({frame_number, *frame_id});
  }

  frame_numbers.append({current_frame, 0});

  return frame_numbers.as_span();
}

static Array<int> get_editable_frames_for_layer(const GreasePencil &grease_pencil,
                                                const bke::greasepencil::Layer &layer,
                                                const int current_frame,
                                                const bool use_multi_frame_editing)
{
  using namespace blender::bke::greasepencil;
  Vector<int> frame_numbers;
  Set<const Drawing *> added_drawings;
  if (use_multi_frame_editing) {
    const Drawing *current_drawing = grease_pencil.get_drawing_at(layer, current_frame);
    for (const auto [frame_number, frame] : layer.frames().items()) {
      if (!frame.is_selected()) {
        continue;
      }
      frame_numbers.append(frame_number);
      added_drawings.add(grease_pencil.get_drawing_at(layer, frame_number));
    }
    if (added_drawings.contains(current_drawing)) {
      return frame_numbers.as_span();
    }
  }

  frame_numbers.append(current_frame);
  return frame_numbers.as_span();
}

Vector<MutableDrawingInfo> retrieve_editable_drawings(const Scene &scene,
                                                      GreasePencil &grease_pencil)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;

  Vector<MutableDrawingInfo> editable_drawings;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_editable()) {
      continue;
    }
    const Array<int> frame_numbers = get_editable_frames_for_layer(
        grease_pencil, layer, current_frame, use_multi_frame_editing);
    for (const int frame_number : frame_numbers) {
      if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
        editable_drawings.append({*drawing, layer_i, frame_number, 1.0f});
      }
    }
  }

  return editable_drawings;
}

Vector<MutableDrawingInfo> retrieve_editable_drawings_with_falloff(const Scene &scene,
                                                                   GreasePencil &grease_pencil)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;
  const bool use_multi_frame_falloff = use_multi_frame_editing &&
                                       (toolsettings->gp_sculpt.flag &
                                        GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;
  if (use_multi_frame_falloff) {
    BKE_curvemapping_init(toolsettings->gp_sculpt.cur_falloff);
  }

  Vector<MutableDrawingInfo> editable_drawings;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_editable()) {
      continue;
    }
    const std::optional<Bounds<int>> frame_bounds = get_selected_frame_number_bounds(layer);
    const int active_frame = get_active_frame_for_falloff(layer, frame_bounds, current_frame);
    const Array<int> frame_numbers = get_editable_frames_for_layer(
        grease_pencil, layer, current_frame, use_multi_frame_editing);
    for (const int frame_number : frame_numbers) {
      if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
        const float falloff = get_frame_falloff(use_multi_frame_falloff,
                                                frame_number,
                                                active_frame,
                                                frame_bounds,
                                                toolsettings->gp_sculpt.cur_falloff);
        editable_drawings.append({*drawing, layer_i, frame_number, falloff});
      }
    }
  }

  return editable_drawings;
}

Array<Vector<MutableDrawingInfo>> retrieve_editable_drawings_grouped_per_frame(
    const Scene &scene, GreasePencil &grease_pencil)
{
  using namespace blender::bke::greasepencil;
  int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;
  const bool use_multi_frame_falloff = use_multi_frame_editing &&
                                       (toolsettings->gp_sculpt.flag &
                                        GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;
  if (use_multi_frame_falloff) {
    BKE_curvemapping_init(toolsettings->gp_sculpt.cur_falloff);
  }

  /* Get a set of unique frame numbers with editable drawings on them. */
  VectorSet<int> selected_frames;
  Span<const Layer *> layers = grease_pencil.layers();
  if (use_multi_frame_editing) {
    for (const int layer_i : layers.index_range()) {
      const Layer &layer = *layers[layer_i];
      if (!layer.is_editable()) {
        continue;
      }
      for (const auto [frame_number, frame] : layer.frames().items()) {
        if (frame_number != current_frame && frame.is_selected()) {
          selected_frames.add(frame_number);
        }
      }
    }
  }
  selected_frames.add(current_frame);

  /* Get drawings grouped per frame. */
  Array<Vector<MutableDrawingInfo>> drawings_grouped_per_frame(selected_frames.size());
  Set<const Drawing *> added_drawings;
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_editable()) {
      continue;
    }
    const std::optional<Bounds<int>> frame_bounds = get_selected_frame_number_bounds(layer);
    const int active_frame = get_active_frame_for_falloff(layer, frame_bounds, current_frame);

    /* In multi frame editing mode, add drawings at selected frames. */
    if (use_multi_frame_editing) {
      for (const auto [frame_number, frame] : layer.frames().items()) {
        Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number);
        if (!frame.is_selected() || drawing == nullptr || added_drawings.contains(drawing)) {
          continue;
        }
        const float falloff = get_frame_falloff(use_multi_frame_falloff,
                                                frame_number,
                                                active_frame,
                                                frame_bounds,
                                                toolsettings->gp_sculpt.cur_falloff);
        const int frame_group = selected_frames.index_of(frame_number);
        drawings_grouped_per_frame[frame_group].append({*drawing, layer_i, frame_number, falloff});
        added_drawings.add_new(drawing);
      }
    }

    /* Add drawing at current frame. */
    Drawing *current_drawing = grease_pencil.get_drawing_at(layer, current_frame);
    if (current_drawing != nullptr && !added_drawings.contains(current_drawing)) {
      const float falloff = get_frame_falloff(use_multi_frame_falloff,
                                              current_frame,
                                              active_frame,
                                              frame_bounds,
                                              toolsettings->gp_sculpt.cur_falloff);
      const int frame_group = selected_frames.index_of(current_frame);
      drawings_grouped_per_frame[frame_group].append(
          {*current_drawing, layer_i, current_frame, falloff});
      added_drawings.add_new(current_drawing);
    }
  }

  return drawings_grouped_per_frame;
}

Vector<MutableDrawingInfo> retrieve_editable_drawings_from_layer(
    const Scene &scene,
    GreasePencil &grease_pencil,
    const blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;
  const int layer_index = *grease_pencil.get_layer_index(layer);

  Vector<MutableDrawingInfo> editable_drawings;
  const Array<int> frame_numbers = get_editable_frames_for_layer(
      grease_pencil, layer, current_frame, use_multi_frame_editing);
  for (const int frame_number : frame_numbers) {
    if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
      editable_drawings.append({*drawing, layer_index, frame_number, 1.0f});
    }
  }

  return editable_drawings;
}

Vector<MutableDrawingInfo> retrieve_editable_drawings_from_layer_with_falloff(
    const Scene &scene,
    GreasePencil &grease_pencil,
    const blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;
  const bool use_multi_frame_falloff = use_multi_frame_editing &&
                                       (toolsettings->gp_sculpt.flag &
                                        GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;
  const int layer_index = *grease_pencil.get_layer_index(layer);
  std::optional<Bounds<int>> frame_bounds;
  if (use_multi_frame_falloff) {
    BKE_curvemapping_init(toolsettings->gp_sculpt.cur_falloff);
    frame_bounds = get_selected_frame_number_bounds(layer);
  }

  const int active_frame = get_active_frame_for_falloff(layer, frame_bounds, current_frame);

  Vector<MutableDrawingInfo> editable_drawings;
  const Array<int> frame_numbers = get_editable_frames_for_layer(
      grease_pencil, layer, current_frame, use_multi_frame_editing);
  for (const int frame_number : frame_numbers) {
    if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
      const float falloff = get_frame_falloff(use_multi_frame_falloff,
                                              frame_number,
                                              active_frame,
                                              frame_bounds,
                                              toolsettings->gp_sculpt.cur_falloff);
      editable_drawings.append({*drawing, layer_index, frame_number, falloff});
    }
  }

  return editable_drawings;
}

Vector<DrawingInfo> retrieve_visible_drawings(const Scene &scene,
                                              const GreasePencil &grease_pencil,
                                              const bool do_onion_skinning)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = BKE_scene_ctime_get(&scene);
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;

  Vector<DrawingInfo> visible_drawings;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_visible()) {
      continue;
    }
    const Array<std::pair<int, int>> frames = get_visible_frames_for_layer(
        grease_pencil, layer, current_frame, use_multi_frame_editing, do_onion_skinning);
    for (const auto &[frame_number, onion_id] : frames) {
      if (const Drawing *drawing = grease_pencil.get_drawing_at(layer, frame_number)) {
        visible_drawings.append({*drawing, layer_i, frame_number, onion_id});
      }
    }
  }

  return visible_drawings;
}

static VectorSet<int> get_locked_material_indices(Object &object)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  VectorSet<int> locked_material_indices;
  for (const int mat_i : IndexRange(object.totcol)) {
    Material *material = BKE_object_material_get(&object, mat_i + 1);
    /* The editable materials are unlocked and not hidden. */
    if (material != nullptr && material->gp_style != nullptr &&
        ((material->gp_style->flag & GP_MATERIAL_LOCKED) != 0 ||
         (material->gp_style->flag & GP_MATERIAL_HIDE) != 0))
    {
      locked_material_indices.add_new(mat_i);
    }
  }
  return locked_material_indices;
}

static VectorSet<int> get_hidden_material_indices(Object &object)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  VectorSet<int> hidden_material_indices;
  for (const int mat_i : IndexRange(object.totcol)) {
    Material *material = BKE_object_material_get(&object, mat_i + 1);
    if (material != nullptr && material->gp_style != nullptr &&
        (material->gp_style->flag & GP_MATERIAL_HIDE) != 0)
    {
      hidden_material_indices.add_new(mat_i);
    }
  }
  return hidden_material_indices;
}

static VectorSet<int> get_fill_material_indices(Object &object)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  VectorSet<int> fill_material_indices;
  for (const int mat_i : IndexRange(object.totcol)) {
    Material *material = BKE_object_material_get(&object, mat_i + 1);
    if (material != nullptr && material->gp_style != nullptr &&
        (material->gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0)
    {
      fill_material_indices.add_new(mat_i);
    }
  }
  return fill_material_indices;
}

IndexMask retrieve_editable_strokes(Object &object,
                                    const bke::greasepencil::Drawing &drawing,
                                    int layer_index,
                                    IndexMaskMemory &memory)
{
  using namespace blender;
  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = curves.curves_range();

  if (object.totcol == 0) {
    return IndexMask(curves_range);
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const bke::greasepencil::Layer &layer = *grease_pencil.layers()[layer_index];

  /* If we're not using material locking, the entire curves range is editable. */
  if (layer.ignore_locked_materials()) {
    return IndexMask(curves_range);
  }

  /* Get all the editable material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);
  if (locked_material_indices.is_empty()) {
    return curves_range;
  }

  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (locked_material_indices.contains(0)) {
      return {};
    }
    return curves_range;
  }
  /* Get all the strokes that have their material unlocked. */
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        return !locked_material_indices.contains(materials[curve_i]);
      });
}

IndexMask retrieve_editable_fill_strokes(Object &object,
                                         const bke::greasepencil::Drawing &drawing,
                                         int layer_index,
                                         IndexMaskMemory &memory)
{
  using namespace blender;
  const IndexMask editable_strokes = retrieve_editable_strokes(
      object, drawing, layer_index, memory);
  if (editable_strokes.is_empty()) {
    return {};
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = curves.curves_range();

  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  const VectorSet<int> fill_material_indices = get_fill_material_indices(object);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (editable_strokes.contains(0) && fill_material_indices.contains(0)) {
      return curves_range;
    }
    return {};
  }
  const IndexMask fill_strokes = IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        return fill_material_indices.contains(material_index);
      });
  return IndexMask::from_intersection(editable_strokes, fill_strokes, memory);
}

IndexMask retrieve_editable_strokes_by_material(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                const int mat_i,
                                                IndexMaskMemory &memory)
{
  using namespace blender;

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = curves.curves_range();

  /* Get all the editable material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);

  const bke::AttributeAccessor attributes = curves.attributes();

  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (locked_material_indices.contains(0)) {
      return {};
    }
    return curves_range;
  }
  /* Get all the strokes that share the same material and have it unlocked. */
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        if (material_index == mat_i) {
          return !locked_material_indices.contains(material_index);
        }
        return false;
      });
}

IndexMask retrieve_editable_points(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   int layer_index,
                                   IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points_range = curves.points_range();

  if (object.totcol == 0) {
    return IndexMask(points_range);
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const bke::greasepencil::Layer &layer = *grease_pencil.layers()[layer_index];

  /* If we're not using material locking, the entire points range is editable. */
  if (layer.ignore_locked_materials()) {
    return IndexMask(points_range);
  }

  /* Get all the editable material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);
  if (locked_material_indices.is_empty()) {
    return points_range;
  }

  /* Propagate the material index to the points. */
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Point, 0);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (locked_material_indices.contains(0)) {
      return {};
    }
    return points_range;
  }
  /* Get all the points that are part of a stroke with an unlocked material. */
  return IndexMask::from_predicate(
      points_range, GrainSize(4096), memory, [&](const int64_t point_i) {
        return !locked_material_indices.contains(materials[point_i]);
      });
}

IndexMask retrieve_editable_elements(Object &object,
                                     const MutableDrawingInfo &info,
                                     const bke::AttrDomain selection_domain,
                                     IndexMaskMemory &memory)
{

  const bke::greasepencil::Drawing &drawing = info.drawing;
  if (selection_domain == bke::AttrDomain::Curve) {
    return ed::greasepencil::retrieve_editable_strokes(object, drawing, info.layer_index, memory);
  }
  if (selection_domain == bke::AttrDomain::Point) {
    return ed::greasepencil::retrieve_editable_points(object, drawing, info.layer_index, memory);
  }
  return {};
}

IndexMask retrieve_visible_strokes(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory)
{
  using namespace blender;

  /* Get all the hidden material indices. */
  VectorSet<int> hidden_material_indices = get_hidden_material_indices(object);

  if (hidden_material_indices.is_empty()) {
    return drawing.strokes().curves_range();
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = drawing.strokes().curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Get all the strokes that have their material visible. */
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        return !hidden_material_indices.contains(material_index);
      });
}

IndexMask retrieve_visible_points(Object &object,
                                  const bke::greasepencil::Drawing &drawing,
                                  IndexMaskMemory &memory)
{
  /* Get all the hidden material indices. */
  VectorSet<int> hidden_material_indices = get_hidden_material_indices(object);

  if (hidden_material_indices.is_empty()) {
    return drawing.strokes().points_range();
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points_range = curves.points_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Propagate the material index to the points. */
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Point, 0);
  if (const std::optional<int> single_material = materials.get_if_single()) {
    if (!hidden_material_indices.contains(*single_material)) {
      return points_range;
    }
    return {};
  }

  /* Get all the points that are part of a stroke with a visible material. */
  return IndexMask::from_predicate(
      points_range, GrainSize(4096), memory, [&](const int64_t point_i) {
        const int material_index = materials[point_i];
        return !hidden_material_indices.contains(material_index);
      });
}

IndexMask retrieve_visible_bezier_strokes(Object &object,
                                          const bke::greasepencil::Drawing &drawing,
                                          IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return IndexMask(0);
  }

  const IndexRange curves_range = curves.curves_range();
  const VArray<int8_t> curve_types = curves.curve_types();
  const std::array<int, CURVE_TYPES_NUM> type_counts = curves.curve_type_counts();

  const IndexMask bezier_strokes = bke::curves::indices_for_type(
      curve_types, type_counts, CURVE_TYPE_BEZIER, curves_range, memory);

  const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
      object, drawing, memory);

  return IndexMask::from_intersection(visible_strokes, bezier_strokes, memory);
}

IndexMask retrieve_visible_bezier_points(Object &object,
                                         const bke::greasepencil::Drawing &drawing,
                                         IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return IndexMask(0);
  }

  const IndexMask visible_bezier_strokes = retrieve_visible_bezier_strokes(
      object, drawing, memory);

  return IndexMask::from_ranges(curves.points_by_curve(), visible_bezier_strokes, memory);
}

IndexMask retrieve_visible_bezier_handle_strokes(Object &object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 const int handle_display,
                                                 IndexMaskMemory &memory)
{
  if (handle_display == CURVE_HANDLE_NONE) {
    return IndexMask(0);
  }

  const bke::CurvesGeometry &curves = drawing.strokes();

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return IndexMask(0);
  }

  const IndexMask visible_bezier_strokes = retrieve_visible_bezier_strokes(
      object, drawing, memory);

  if (handle_display == CURVE_HANDLE_ALL) {
    return visible_bezier_strokes;
  }

  /* handle_display == CURVE_HANDLE_SELECTED */
  const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves, memory);
  return IndexMask::from_intersection(visible_bezier_strokes, selected_strokes, memory);
}

IndexMask retrieve_visible_bezier_handle_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                const int layer_index,
                                                const int handle_display,
                                                IndexMaskMemory &memory)
{
  if (handle_display == CURVE_HANDLE_NONE) {
    return IndexMask(0);
  }
  else if (handle_display == CURVE_HANDLE_ALL) {
    return retrieve_visible_bezier_points(object, drawing, memory);
  }
  /* else handle_display == CURVE_HANDLE_SELECTED */

  const bke::CurvesGeometry &curves = drawing.strokes();

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return IndexMask(0);
  }

  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  const VArray<int8_t> types = curves.curve_types();

  const VArray<bool> selected_point = *curves.attributes().lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  const VArray<bool> selected_left = *curves.attributes().lookup_or_default<bool>(
      ".selection_handle_left", bke::AttrDomain::Point, true);
  const VArray<bool> selected_right = *curves.attributes().lookup_or_default<bool>(
      ".selection_handle_right", bke::AttrDomain::Point, true);

  const IndexMask editable_points = ed::greasepencil::retrieve_editable_points(
      object, drawing, layer_index, memory);

  const IndexMask selected_points = IndexMask::from_predicate(
      curves.points_range(), GrainSize(4096), memory, [&](const int64_t point_i) {
        const bool is_selected = selected_point[point_i] || selected_left[point_i] ||
                                 selected_right[point_i];
        const bool is_bezier = types[point_to_curve_map[point_i]] == CURVE_TYPE_BEZIER;
        return is_selected && is_bezier;
      });

  return IndexMask::from_intersection(editable_points, selected_points, memory);
}

IndexMask retrieve_visible_bezier_handle_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  const int layer_index,
                                                  const bke::AttrDomain selection_domain,
                                                  const int handle_display,
                                                  IndexMaskMemory &memory)
{
  if (selection_domain == bke::AttrDomain::Curve) {
    return ed::greasepencil::retrieve_visible_bezier_handle_strokes(
        object, drawing, handle_display, memory);
  }
  if (selection_domain == bke::AttrDomain::Point) {
    return ed::greasepencil::retrieve_visible_bezier_handle_points(
        object, drawing, layer_index, handle_display, memory);
  }
  return {};
}

IndexMask retrieve_editable_and_selected_strokes(Object &object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 int layer_index,
                                                 IndexMaskMemory &memory)
{
  using namespace blender;
  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_strokes = retrieve_editable_strokes(
      object, drawing, layer_index, memory);
  const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves, memory);

  return IndexMask::from_intersection(editable_strokes, selected_strokes, memory);
}

IndexMask retrieve_editable_and_selected_fill_strokes(Object &object,
                                                      const bke::greasepencil::Drawing &drawing,
                                                      int layer_index,
                                                      IndexMaskMemory &memory)
{
  using namespace blender;
  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_strokes = retrieve_editable_fill_strokes(
      object, drawing, layer_index, memory);
  const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves, memory);

  return IndexMask::from_intersection(editable_strokes, selected_strokes, memory);
}

IndexMask retrieve_editable_and_selected_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                int layer_index,
                                                IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_points = retrieve_editable_points(object, drawing, layer_index, memory);
  const IndexMask selected_points = ed::curves::retrieve_selected_points(curves, memory);

  return IndexMask::from_intersection(editable_points, selected_points, memory);
}

IndexMask retrieve_editable_and_selected_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  int layer_index,
                                                  const bke::AttrDomain selection_domain,
                                                  IndexMaskMemory &memory)
{
  if (selection_domain == bke::AttrDomain::Curve) {
    return ed::greasepencil::retrieve_editable_and_selected_strokes(
        object, drawing, layer_index, memory);
  }
  if (selection_domain == bke::AttrDomain::Point) {
    return ed::greasepencil::retrieve_editable_and_selected_points(
        object, drawing, layer_index, memory);
  }
  return {};
}

IndexMask retrieve_editable_and_all_selected_points(Object &object,
                                                    const bke::greasepencil::Drawing &drawing,
                                                    int layer_index,
                                                    int handle_display,
                                                    IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_points = retrieve_editable_points(object, drawing, layer_index, memory);
  const IndexMask selected_points = ed::curves::retrieve_all_selected_points(
      curves, handle_display, memory);

  return IndexMask::from_intersection(editable_points, selected_points, memory);
}

bool has_editable_layer(const GreasePencil &grease_pencil)
{
  using namespace blender::bke::greasepencil;
  for (const Layer *layer : grease_pencil.layers()) {
    if (layer->is_editable()) {
      return true;
    }
  }
  return false;
}

Array<PointTransferData> compute_topology_change(
    const bke::CurvesGeometry &src,
    bke::CurvesGeometry &dst,
    const Span<Vector<PointTransferData>> src_to_dst_points,
    const bool keep_caps)
{
  const int src_curves_num = src.curves_num();
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  const VArray<bool> src_cyclic = src.cyclic();

  int dst_points_num = 0;
  for (const Vector<PointTransferData> &src_transfer_data : src_to_dst_points) {
    dst_points_num += src_transfer_data.size();
  }
  if (dst_points_num == 0) {
    dst.resize(0, 0);
    return Array<PointTransferData>(0);
  }

  /* Set the intersection parameters in the destination domain : a pair of int and float
   * numbers for which the integer is the index of the corresponding segment in the
   * source curves, and the float part is the (0,1) factor representing its position in
   * the segment.
   */
  Array<PointTransferData> dst_transfer_data(dst_points_num);

  Array<int> src_pivot_point(src_curves_num, -1);
  Array<int> dst_interm_curves_offsets(src_curves_num + 1, 0);
  int dst_point = -1;
  for (const int src_curve : src.curves_range()) {
    const IndexRange src_points = src_points_by_curve[src_curve];

    for (const int src_point : src_points) {
      for (const PointTransferData &dst_point_transfer : src_to_dst_points[src_point]) {
        if (dst_point_transfer.is_src_point) {
          dst_transfer_data[++dst_point] = dst_point_transfer;
          continue;
        }

        /* Add an intersection with the eraser and mark it as a cut. */
        dst_transfer_data[++dst_point] = dst_point_transfer;

        /* For cyclic curves, mark the pivot point as the last intersection with the eraser
         * that starts a new segment in the destination.
         */
        if (src_cyclic[src_curve] && dst_point_transfer.is_cut) {
          src_pivot_point[src_curve] = dst_point;
        }
      }
    }
    /* We store intermediate curve offsets represent an intermediate state of the
     * destination curves before cutting the curves at eraser's intersection. Thus, it
     * contains the same number of curves than in the source, but the offsets are
     * different, because points may have been added or removed. */
    dst_interm_curves_offsets[src_curve + 1] = dst_point + 1;
  }

  /* Cyclic curves. */
  Array<bool> src_now_cyclic(src_curves_num);
  threading::parallel_for(src.curves_range(), 4096, [&](const IndexRange src_curves) {
    for (const int src_curve : src_curves) {
      const int pivot_point = src_pivot_point[src_curve];

      if (pivot_point == -1) {
        /* Either the curve was not cyclic or it wasn't cut : no need to change it. */
        src_now_cyclic[src_curve] = src_cyclic[src_curve];
        continue;
      }

      /* A cyclic curve was cut :
       *  - this curve is not cyclic anymore,
       *  - and we have to shift points to keep the closing segment.
       */
      src_now_cyclic[src_curve] = false;

      const int dst_interm_first = dst_interm_curves_offsets[src_curve];
      const int dst_interm_last = dst_interm_curves_offsets[src_curve + 1];
      std::rotate(dst_transfer_data.begin() + dst_interm_first,
                  dst_transfer_data.begin() + pivot_point,
                  dst_transfer_data.begin() + dst_interm_last);
    }
  });

  /* Compute the destination curve offsets. */
  Vector<int> dst_curves_offset;
  Vector<int> dst_to_src_curve;
  dst_curves_offset.append(0);
  for (int src_curve : src.curves_range()) {
    const IndexRange dst_points(dst_interm_curves_offsets[src_curve],
                                dst_interm_curves_offsets[src_curve + 1] -
                                    dst_interm_curves_offsets[src_curve]);
    int length_of_current = 0;

    for (int dst_point : dst_points) {

      if ((length_of_current > 0) && dst_transfer_data[dst_point].is_cut) {
        /* This is the new first point of a curve. */
        dst_curves_offset.append(dst_point);
        dst_to_src_curve.append(src_curve);
        length_of_current = 0;
      }
      ++length_of_current;
    }

    if (length_of_current != 0) {
      /* End of a source curve. */
      dst_curves_offset.append(dst_points.one_after_last());
      dst_to_src_curve.append(src_curve);
    }
  }
  const int dst_curves_num = dst_curves_offset.size() - 1;
  if (dst_curves_num == 0) {
    dst.resize(0, 0);
    return dst_transfer_data;
  }

  /* Build destination curves geometry. */
  dst.resize(dst_points_num, dst_curves_num);
  array_utils::copy(dst_curves_offset.as_span(), dst.offsets_for_write());
  const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();

  /* Vertex group names. */
  BLI_assert(BLI_listbase_count(&dst.vertex_group_names) == 0);
  BKE_defgroup_copy_list(&dst.vertex_group_names, &src.vertex_group_names);

  /* Attributes. */
  const bke::AttributeAccessor src_attributes = src.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

  /* Copy curves attributes. */
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic"}),
                         dst_to_src_curve,
                         dst_attributes);
  if (src_cyclic.get_if_single().value_or(true)) {
    array_utils::gather(
        src_now_cyclic.as_span(), dst_to_src_curve.as_span(), dst.cyclic_for_write());
  }

  dst.update_curve_types();

  /* Display intersections with flat caps. */
  if (!keep_caps) {
    bke::SpanAttributeWriter<int8_t> dst_start_caps =
        dst_attributes.lookup_or_add_for_write_span<int8_t>("start_cap", bke::AttrDomain::Curve);
    bke::SpanAttributeWriter<int8_t> dst_end_caps =
        dst_attributes.lookup_or_add_for_write_span<int8_t>("end_cap", bke::AttrDomain::Curve);

    threading::parallel_for(dst.curves_range(), 4096, [&](const IndexRange dst_curves) {
      for (const int dst_curve : dst_curves) {
        const IndexRange dst_curve_points = dst_points_by_curve[dst_curve];
        const PointTransferData &start_point_transfer =
            dst_transfer_data[dst_curve_points.first()];
        const PointTransferData &end_point_transfer = dst_transfer_data[dst_curve_points.last()];

        if (dst_start_caps && start_point_transfer.is_cut) {
          dst_start_caps.span[dst_curve] = GP_STROKE_CAP_TYPE_FLAT;
        }
        /* The is_cut flag does not work for end points, but any end point that isn't the source
         * point must also be a cut. */
        if (dst_end_caps && !end_point_transfer.is_src_end_point()) {
          dst_end_caps.span[dst_curve] = GP_STROKE_CAP_TYPE_FLAT;
        }
      }
    });

    dst_start_caps.finish();
    dst_end_caps.finish();
  }

  /* Copy/Interpolate point attributes. */
  for (bke::AttributeTransferData &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, dst_attributes, {bke::AttrDomain::Point}))
  {
    bke::attribute_math::convert_to_static_type(attribute.dst.span.type(), [&](auto dummy) {
      using T = decltype(dummy);
      auto src_attr = attribute.src.typed<T>();
      auto dst_attr = attribute.dst.span.typed<T>();

      threading::parallel_for(dst.points_range(), 4096, [&](const IndexRange dst_points) {
        for (const int dst_point : dst_points) {
          const PointTransferData &point_transfer = dst_transfer_data[dst_point];
          if (point_transfer.is_src_point) {
            dst_attr[dst_point] = src_attr[point_transfer.src_point];
          }
          else {
            dst_attr[dst_point] = bke::attribute_math::mix2<T>(
                point_transfer.factor,
                src_attr[point_transfer.src_point],
                src_attr[point_transfer.src_next_point]);
          }
        }
      });

      attribute.dst.finish();
    });
  }

  return dst_transfer_data;
}

static float pixel_radius_to_world_space_radius(const RegionView3D *rv3d,
                                                const ARegion *region,
                                                const float3 center,
                                                const float4x4 to_world,
                                                const float pixel_radius)
{
  const float2 xy_delta = float2(pixel_radius, 0.0f);
  const float3 loc = math::transform_point(to_world, center);

  const float zfac = ED_view3d_calc_zfac(rv3d, loc);
  float3 delta;
  ED_view3d_win_to_delta(region, xy_delta, zfac, delta);

  const float scale = math::length(
      math::transform_direction(to_world, float3(math::numbers::inv_sqrt3)));

  return math::safe_divide(math::length(delta), scale);
}

static float brush_radius_at_location(const RegionView3D *rv3d,
                                      const ARegion *region,
                                      const Brush *brush,
                                      const float3 location,
                                      const float4x4 to_world)
{
  if ((brush->flag & BRUSH_LOCK_SIZE) == 0) {
    return pixel_radius_to_world_space_radius(
        rv3d, region, location, to_world, float(brush->size) / 2.0f);
  }
  return brush->unprojected_size / 2.0f;
}

float radius_from_input_sample(const RegionView3D *rv3d,
                               const ARegion *region,
                               const Brush *brush,
                               const float pressure,
                               const float3 &location,
                               const float4x4 &to_world,
                               const BrushGpencilSettings *settings)
{
  float radius = brush_radius_at_location(rv3d, region, brush, location, to_world);
  if (BKE_brush_use_size_pressure(brush)) {
    radius *= BKE_curvemapping_evaluateF(settings->curve_sensitivity, 0, pressure);
  }
  return radius;
}

float opacity_from_input_sample(const float pressure,
                                const Brush *brush,
                                const BrushGpencilSettings *settings)
{
  float opacity = brush->alpha;
  if (BKE_brush_use_alpha_pressure(brush)) {
    opacity *= BKE_curvemapping_evaluateF(settings->curve_strength, 0, pressure);
  }
  return opacity;
}

wmOperatorStatus grease_pencil_draw_operator_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const bool use_duplicate_previous_key)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!grease_pencil.has_active_layer()) {
    BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer");
    return OPERATOR_CANCELLED;
  }

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();

  if (!active_layer.is_editable()) {
    BKE_report(op->reports, RPT_ERROR, "Active layer is locked or hidden");
    return OPERATOR_CANCELLED;
  }

  /* Ensure a drawing at the current keyframe. */
  bool inserted_keyframe = false;
  if (!ed::greasepencil::ensure_active_keyframe(
          *scene, grease_pencil, active_layer, use_duplicate_previous_key, inserted_keyframe))
  {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
    return OPERATOR_CANCELLED;
  }
  if (inserted_keyframe) {
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
  }
  return OPERATOR_RUNNING_MODAL;
}

float4x2 calculate_texture_space(const Scene *scene,
                                 const ARegion *region,
                                 const float2 &mouse,
                                 const DrawingPlacement &placement)
{
  float3 u_dir;
  float3 v_dir;
  /* Set the texture space origin to be the first point. */
  float3 origin = placement.project(mouse);
  /* Align texture with the drawing plane. */
  switch (scene->toolsettings->gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_VIEW:
      u_dir = math::normalize(placement.project(float2(region->winx, 0.0f) + mouse) - origin);
      v_dir = math::normalize(placement.project(float2(0.0f, region->winy) + mouse) - origin);
      break;
    case GP_LOCKAXIS_Y:
      u_dir = float3(1.0f, 0.0f, 0.0f);
      v_dir = float3(0.0f, 0.0f, 1.0f);
      break;
    case GP_LOCKAXIS_X:
      u_dir = float3(0.0f, 1.0f, 0.0f);
      v_dir = float3(0.0f, 0.0f, 1.0f);
      break;
    case GP_LOCKAXIS_Z:
      u_dir = float3(1.0f, 0.0f, 0.0f);
      v_dir = float3(0.0f, 1.0f, 0.0f);
      break;
    case GP_LOCKAXIS_CURSOR: {
      const float3x3 mat = scene->cursor.matrix<float3x3>();
      u_dir = mat * float3(1.0f, 0.0f, 0.0f);
      v_dir = mat * float3(0.0f, 1.0f, 0.0f);
      origin = float3(scene->cursor.location);
      break;
    }
  }

  return math::transpose(float2x4(float4(u_dir, -math::dot(u_dir, origin)),
                                  float4(v_dir, -math::dot(v_dir, origin))));
}

GreasePencil *from_context(bContext &C)
{
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(
      CTX_data_pointer_get_type(&C, "grease_pencil", &RNA_GreasePencil).data);

  if (grease_pencil == nullptr) {
    Object *object = CTX_data_active_object(&C);
    if (object && object->type == OB_GREASE_PENCIL) {
      grease_pencil = static_cast<GreasePencil *>(object->data);
    }
  }
  return grease_pencil;
}

void add_single_curve(bke::CurvesGeometry &curves, const bool at_end)
{
  if (at_end) {
    const int num_old_points = curves.points_num();
    curves.resize(curves.points_num() + 1, curves.curves_num() + 1);
    curves.offsets_for_write().last(1) = num_old_points;
    return;
  }

  curves.resize(curves.points_num() + 1, curves.curves_num() + 1);
  MutableSpan<int> offsets = curves.offsets_for_write();
  offsets.first() = 0;

  /* Loop through backwards to not overwrite the data. */
  for (int i = curves.curves_num() - 2; i >= 0; i--) {
    offsets[i + 1] = offsets[i] + 1;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(iter.name);
    GMutableSpan attribute_data = dst.span;

    bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
      using T = decltype(dummy);
      MutableSpan<T> span_data = attribute_data.typed<T>();

      /* Loop through backwards to not overwrite the data. */
      for (int i = span_data.size() - 2; i >= 0; i--) {
        span_data[i + 1] = span_data[i];
      }
    });
    dst.finish();
  });
}

void resize_single_curve(bke::CurvesGeometry &curves, const bool at_end, const int new_points_num)
{
  BLI_assert(new_points_num >= 0);
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const int curve_index = at_end ? curves.curves_range().last() : 0;
  const int current_points_num = points_by_curve[curve_index].size();
  if (new_points_num == current_points_num) {
    return;
  }

  if (at_end) {
    const int diff_points_num = new_points_num - current_points_num;
    curves.resize(curves.points_num() + diff_points_num, curves.curves_num());
    curves.offsets_for_write().last() = curves.points_num();
    return;
  }

  if (current_points_num < new_points_num) {
    const int last_active_point = points_by_curve[0].last();

    const int added_points_num = new_points_num - current_points_num;

    curves.resize(curves.points_num() + added_points_num, curves.curves_num());
    MutableSpan<int> offsets = curves.offsets_for_write();
    for (const int src_curve : curves.curves_range().drop_front(1)) {
      offsets[src_curve] = offsets[src_curve] + added_points_num;
    }
    offsets.last() = curves.points_num();

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.domain != bke::AttrDomain::Point) {
        return;
      }

      bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(iter.name);
      GMutableSpan attribute_data = dst.span;

      bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
        using T = decltype(dummy);
        MutableSpan<T> span_data = attribute_data.typed<T>();

        /* Loop through backwards to not overwrite the data. */
        for (int i = span_data.size() - 1 - added_points_num; i >= last_active_point; i--) {
          span_data[i + added_points_num] = span_data[i];
        }
      });
      dst.finish();
    });
  }
  else {
    /* First move the attribute data, then resize. */
    const int removed_points_num = current_points_num - new_points_num;
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.domain != bke::AttrDomain::Point) {
        return;
      }

      bke::GSpanAttributeWriter dst = attributes.lookup_for_write_span(iter.name);
      GMutableSpan attribute_data = dst.span;

      bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
        using T = decltype(dummy);
        MutableSpan<T> span_data = attribute_data.typed<T>();

        for (const int i :
             span_data.index_range().drop_front(new_points_num).drop_back(removed_points_num))
        {
          span_data[i] = span_data[i + removed_points_num];
        }
      });
      dst.finish();
    });

    curves.resize(curves.points_num() - removed_points_num, curves.curves_num());
    MutableSpan<int> offsets = curves.offsets_for_write();
    for (const int src_curve : curves.curves_range().drop_front(1)) {
      offsets[src_curve] = offsets[src_curve] - removed_points_num;
    }
    offsets.last() = curves.points_num();
  }
}

void apply_eval_grease_pencil_data(const GreasePencil &eval_grease_pencil,
                                   const int eval_frame,
                                   const IndexMask &orig_layers,
                                   GreasePencil &orig_grease_pencil)
{
  using namespace bke;
  using namespace bke::greasepencil;
  /* Build a set of pointers to the layers that we want to apply. */
  Set<const Layer *> orig_layers_to_apply;
  orig_layers.foreach_index([&](const int layer_i) {
    const Layer &layer = orig_grease_pencil.layer(layer_i);
    orig_layers_to_apply.add(&layer);
  });

  /* Ensure that the layer names are unique by merging layers with the same name. */
  const int old_layers_num = eval_grease_pencil.layers().size();
  Vector<Vector<int>> layers_map;
  Map<StringRef, int> new_layer_index_by_name;
  for (const int layer_i : IndexRange(old_layers_num)) {
    const Layer &layer = eval_grease_pencil.layer(layer_i);
    const int new_layer_index = new_layer_index_by_name.lookup_or_add_cb(
        layer.name(), [&]() { return layers_map.append_and_get_index_as(); });
    layers_map[new_layer_index].append(layer_i);
  }
  GreasePencil &merged_layers_grease_pencil = *geometry::merge_layers(
      eval_grease_pencil, layers_map, {});

  Map<const Layer *, const Layer *> eval_to_orig_layer_map;
  {
    /* Set of orig layers that require the drawing on `eval_frame` to be cleared. These are layers
     * that existed in original geometry but were removed in the evaluated data. */
    Set<Layer *> orig_layers_to_clear;
    for (Layer *layer : orig_grease_pencil.layers_for_write()) {
      /* Only allow clearing a layer if it is visible. */
      if (layer->is_visible()) {
        orig_layers_to_clear.add(layer);
      }
    }
    for (const TreeNode *node_eval : merged_layers_grease_pencil.nodes()) {
      /* Check if the original geometry has a layer with the same name. */
      TreeNode *node_orig = orig_grease_pencil.find_node_by_name(node_eval->name());

      BLI_assert(node_eval != nullptr);
      if (!node_eval->is_layer()) {
        continue;
      }
      /* If the orig layer isn't valid then a new layer with a unique name will be generated. */
      const bool has_valid_orig_layer = (node_orig != nullptr && node_orig->is_layer());
      if (!has_valid_orig_layer) {
        /* Note: This name might be empty! This has to be resolved at a later stage! */
        Layer &layer_orig = orig_grease_pencil.add_layer(node_eval->name(), true);
        orig_layers_to_apply.add(&layer_orig);
        /* Make sure to add a new keyframe with a new drawing. */
        orig_grease_pencil.insert_frame(layer_orig, eval_frame);
        node_orig = &layer_orig.as_node();
      }
      BLI_assert(node_orig != nullptr);
      Layer &layer_orig = node_orig->as_layer();
      /* This layer has a matching evaluated layer, so don't clear its keyframe. */
      orig_layers_to_clear.remove(&layer_orig);
      /* Only map layers in `eval_to_orig_layer_map` that we want to apply. */
      if (orig_layers_to_apply.contains(&layer_orig)) {
        /* Copy layer properties to original geometry. */
        const Layer &layer_eval = node_eval->as_layer();
        layer_orig.opacity = layer_eval.opacity;
        layer_orig.set_local_transform(layer_eval.local_transform());

        /* Add new mapping for `layer_eval` -> `layer_orig`. */
        eval_to_orig_layer_map.add_new(&layer_eval, &layer_orig);
      }
    }

    /* Clear the keyframe of all the original layers that don't have a matching evaluated layer,
     * e.g. the ones that were "deleted" in the evaluated data. */
    for (Layer *layer_orig : orig_layers_to_clear) {
      /* Try inserting a frame. */
      Drawing *drawing_orig = orig_grease_pencil.insert_frame(*layer_orig, eval_frame);
      if (drawing_orig == nullptr) {
        /* If that fails, get the drawing for this frame. */
        drawing_orig = orig_grease_pencil.get_drawing_at(*layer_orig, eval_frame);
      }
      /* Clear the existing drawing. */
      drawing_orig->strokes_for_write() = {};
      drawing_orig->tag_topology_changed();
    }
  }

  /* Gather the original vertex group names. */
  Set<StringRef> orig_vgroup_names;
  LISTBASE_FOREACH (bDeformGroup *, dg, &orig_grease_pencil.vertex_group_names) {
    orig_vgroup_names.add(dg->name);
  }

  /* Update the drawings. */
  VectorSet<Drawing *> all_updated_drawings;

  Set<StringRef> new_vgroup_names;
  for (auto [layer_eval, layer_orig] : eval_to_orig_layer_map.items()) {
    Drawing *drawing_eval = merged_layers_grease_pencil.get_drawing_at(*layer_eval, eval_frame);
    Drawing *drawing_orig = orig_grease_pencil.get_drawing_at(*layer_orig, eval_frame);

    if (drawing_orig && drawing_eval) {
      CurvesGeometry &eval_strokes = drawing_eval->strokes_for_write();

      /* Check for new vertex groups in CurvesGeometry. */
      LISTBASE_FOREACH (bDeformGroup *, dg, &eval_strokes.vertex_group_names) {
        if (!orig_vgroup_names.contains(dg->name)) {
          new_vgroup_names.add(dg->name);
        }
      }

      /* Write the data to the original drawing. */
      drawing_orig->strokes_for_write() = std::move(eval_strokes);
      /* Anonymous attributes shouldn't be available on original geometry. */
      drawing_orig->strokes_for_write().attributes_for_write().remove_anonymous();
      drawing_orig->tag_topology_changed();
      all_updated_drawings.add_new(drawing_orig);
    }
  }

  /* Add new vertex groups to GreasePencil object. */
  for (StringRef new_vgroup_name : new_vgroup_names) {
    bDeformGroup *dst = MEM_callocN<bDeformGroup>(__func__);
    new_vgroup_name.copy_utf8_truncated(dst->name);
    BLI_addtail(&orig_grease_pencil.vertex_group_names, dst);
  }

  /* Get the original material pointers from the result geometry. */
  VectorSet<Material *> original_materials;
  const Span<Material *> eval_materials = Span{eval_grease_pencil.material_array,
                                               eval_grease_pencil.material_array_num};
  for (Material *eval_material : eval_materials) {
    if (!eval_material) {
      return;
    }
    original_materials.add(DEG_get_original(eval_material));
  }

  /* Build material indices mapping. This maps the materials indices on the original geometry to
   * the material indices used in the result geometry. The material indices for the drawings in
   * the result geometry are already correct, but this might not be the case for all drawings in
   * the original geometry (like for drawings that are not visible on the frame that the data is
   * being applied on). */
  const IndexRange orig_material_indices = IndexRange(orig_grease_pencil.material_array_num);
  Array<int> material_indices_map(orig_grease_pencil.material_array_num, -1);
  for (const int mat_i : orig_material_indices) {
    Material *material = orig_grease_pencil.material_array[mat_i];
    const int map_index = original_materials.index_of_try(material);
    if (map_index != -1) {
      material_indices_map[mat_i] = map_index;
    }
  }

  /* Remap material indices for all other drawings. */
  if (!material_indices_map.is_empty() &&
      !array_utils::indices_are_range(material_indices_map, orig_material_indices))
  {
    for (GreasePencilDrawingBase *base : orig_grease_pencil.drawings()) {
      if (base->type != GP_DRAWING) {
        continue;
      }
      Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      if (all_updated_drawings.contains(&drawing)) {
        /* Skip remapping drawings that already have been updated. */
        continue;
      }
      MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
      if (!attributes.contains("material_index")) {
        continue;
      }
      SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
          "material_index", AttrDomain::Curve);
      for (int &material_index : material_indices.span) {
        if (material_indices_map.index_range().contains(material_index)) {
          material_index = material_indices_map[material_index];
        }
      }
      material_indices.finish();
    }
  }

  /* Convert the layer map into an index mapping. */
  Map<int, int> eval_to_orig_layer_indices_map;
  for (const int layer_eval_i : merged_layers_grease_pencil.layers().index_range()) {
    const Layer *layer_eval = &merged_layers_grease_pencil.layer(layer_eval_i);
    if (eval_to_orig_layer_map.contains(layer_eval)) {
      const Layer *layer_orig = eval_to_orig_layer_map.lookup(layer_eval);
      const int layer_orig_index = *orig_grease_pencil.get_layer_index(*layer_orig);
      eval_to_orig_layer_indices_map.add(layer_eval_i, layer_orig_index);
    }
  }

  /* Propagate layer attributes. */
  AttributeAccessor src_attributes = merged_layers_grease_pencil.attributes();
  MutableAttributeAccessor dst_attributes = orig_grease_pencil.attributes_for_write();
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    /* Anonymous attributes shouldn't be available on original geometry. */
    if (attribute_name_is_anonymous(iter.name)) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    const GVArraySpan src = *iter.get(AttrDomain::Layer);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, AttrDomain::Layer, iter.data_type);
    if (!dst) {
      return;
    }
    attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
      using T = decltype(dummy);
      Span<T> src_span = src.typed<T>();
      MutableSpan<T> dst_span = dst.span.typed<T>();
      for (const auto [src_i, dst_i] : eval_to_orig_layer_indices_map.items()) {
        dst_span[dst_i] = src_span[src_i];
      }
    });
    dst.finish();
  });

  /* Free temporary grease pencil struct. */
  BKE_id_free(nullptr, &merged_layers_grease_pencil);
}

bool remove_fill_guides(bke::CurvesGeometry &curves)
{
  if (!curves.attributes().contains(".is_fill_guide")) {
    return false;
  }

  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<bool> is_fill_guide = *attributes.lookup<bool>(".is_fill_guide",
                                                              bke::AttrDomain::Curve);

  IndexMaskMemory memory;
  const IndexMask fill_guides = IndexMask::from_bools(is_fill_guide, memory);
  curves.remove_curves(fill_guides, {});

  curves.attributes_for_write().remove(".is_fill_guide");

  return true;
}

}  // namespace blender::ed::greasepencil
