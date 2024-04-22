/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_attribute.hh"
#include "BKE_colortools.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"
#include "BKE_scene.hh"

#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

namespace blender::ed::greasepencil {

DrawingPlacement::DrawingPlacement(const Scene &scene,
                                   const ARegion &region,
                                   const View3D &view3d,
                                   const Object &eval_object,
                                   const bke::greasepencil::Layer &layer)
    : region_(&region), view3d_(&view3d)
{
  layer_space_to_world_space_ = layer.to_world_space(eval_object);
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
      float3x3 mat;
      BKE_scene_cursor_rot_to_mat3(&scene.cursor, mat.ptr());
      placement_normal_ = mat * float3(0, 0, 1);
      break;
    }
  }
  /* Initialize DrawingPlacementDepth from toolsettings. */
  switch (scene.toolsettings->gpencil_v3d_align) {
    case GP_PROJECT_VIEWSPACE:
      depth_ = DrawingPlacementDepth::ObjectOrigin;
      placement_loc_ = layer_space_to_world_space_.location();
      break;
    case (GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR):
      depth_ = DrawingPlacementDepth::Cursor;
      placement_loc_ = float3(scene.cursor.location);
      break;
    case (GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_VIEW):
      depth_ = DrawingPlacementDepth::Surface;
      surface_offset_ = scene.toolsettings->gpencil_surface_offset;
      /* Default to view placement with the object origin if we don't hit a surface. */
      placement_loc_ = layer_space_to_world_space_.location();
      break;
    case (GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_STROKE):
      depth_ = DrawingPlacementDepth::NearestStroke;
      /* Default to view placement with the object origin if we don't hit a stroke. */
      placement_loc_ = layer_space_to_world_space_.location();
      break;
  }

  if (ELEM(plane_,
           DrawingPlacementPlane::Front,
           DrawingPlacementPlane::Side,
           DrawingPlacementPlane::Top,
           DrawingPlacementPlane::Cursor) &&
      ELEM(depth_, DrawingPlacementDepth::ObjectOrigin, DrawingPlacementDepth::Cursor))
  {
    plane_from_point_normal_v3(placement_plane_, placement_loc_, placement_normal_);
  }
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

bool DrawingPlacement::use_project_to_nearest_stroke() const
{
  return depth_ == DrawingPlacementDepth::NearestStroke;
}

void DrawingPlacement::cache_viewport_depths(Depsgraph *depsgraph, ARegion *region, View3D *view3d)
{
  const eV3DDepthOverrideMode mode = (depth_ == DrawingPlacementDepth::Surface) ?
                                         V3D_DEPTH_NO_GPENCIL :
                                         V3D_DEPTH_GPENCIL_ONLY;
  ED_view3d_depth_override(depsgraph, region, view3d, nullptr, mode, &this->depth_cache_);
}

void DrawingPlacement::set_origin_to_nearest_stroke(const float2 co)
{
  BLI_assert(depth_cache_ != nullptr);
  float depth;
  if (ED_view3d_depth_read_cached(depth_cache_, int2(co), 4, &depth)) {
    float3 origin;
    ED_view3d_depth_unproject_v3(region_, int2(co), depth, origin);

    placement_loc_ = origin;
  }
  else {
    /* If nothing was hit, use origin. */
    placement_loc_ = layer_space_to_world_space_.location();
  }
  plane_from_point_normal_v3(placement_plane_, placement_loc_, placement_normal_);
}

float3 DrawingPlacement::project(const float2 co) const
{
  float3 proj_point;
  if (depth_ == DrawingPlacementDepth::Surface) {
    /* Project using the viewport depth cache. */
    BLI_assert(depth_cache_ != nullptr);
    float depth;
    if (ED_view3d_depth_read_cached(depth_cache_, int2(co), 4, &depth)) {
      ED_view3d_depth_unproject_v3(region_, int2(co), depth, proj_point);
      float3 normal;
      ED_view3d_depth_read_cached_normal(region_, depth_cache_, int2(co), normal);
      proj_point += normal * surface_offset_;
    }
    /* If we didn't hit anything, use the view plane for placement. */
    else {
      ED_view3d_win_to_3d(view3d_, region_, placement_loc_, co, proj_point);
    }
  }
  else {
    if (ELEM(plane_,
             DrawingPlacementPlane::Front,
             DrawingPlacementPlane::Side,
             DrawingPlacementPlane::Top,
             DrawingPlacementPlane::Cursor))
    {
      ED_view3d_win_to_3d_on_plane(region_, placement_plane_, co, false, proj_point);
    }
    else if (plane_ == DrawingPlacementPlane::View) {
      ED_view3d_win_to_3d(view3d_, region_, placement_loc_, co, proj_point);
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

static float get_multi_frame_falloff(const int frame_number,
                                     const int center_frame,
                                     const int min_frame,
                                     const int max_frame,
                                     const CurveMapping *falloff_curve)
{
  if (falloff_curve == nullptr) {
    return 1.0f;
  }

  /* Frame right of the center frame. */
  if (frame_number > center_frame) {
    const float frame_factor = 0.5f * float(center_frame - min_frame) / (frame_number - min_frame);
    return BKE_curvemapping_evaluateF(falloff_curve, 0, frame_factor);
  }
  /* Frame left of the center frame. */
  if (frame_number < center_frame) {
    const float frame_factor = 0.5f * float(center_frame - frame_number) /
                               (max_frame - frame_number);
    return BKE_curvemapping_evaluateF(falloff_curve, 0, frame_factor + 0.5f);
  }
  /* Frame at center. */
  return BKE_curvemapping_evaluateF(falloff_curve, 0, 0.5f);
}

static std::pair<int, int> get_minmax_selected_frame_numbers(const GreasePencil &grease_pencil,
                                                             const int current_frame)
{
  using namespace blender::bke::greasepencil;
  int frame_min = current_frame;
  int frame_max = current_frame;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_editable()) {
      continue;
    }
    for (const auto [frame_number, frame] : layer.frames().items()) {
      if (frame_number != current_frame && frame.is_selected()) {
        frame_min = math::min(frame_min, frame_number);
        frame_max = math::max(frame_max, frame_number);
      }
    }
  }
  return std::pair<int, int>(frame_min, frame_max);
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
  const std::optional<bke::greasepencil::FramesMapKey> current_frame_key = layer.frame_key_at(
      current_frame);
  const int current_frame_index = current_frame_key.has_value() ?
                                      sorted_keys.first_index(*current_frame_key) :
                                      0;
  const int last_frame = sorted_keys.last();
  const int last_frame_index = sorted_keys.index_range().last();
  const bool is_before_first = (current_frame < sorted_keys.first());
  for (const int frame_i : sorted_keys.index_range()) {
    const int frame_number = sorted_keys[frame_i];
    if (frame_number == current_frame) {
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

static Array<int> get_editable_frames_for_layer(const bke::greasepencil::Layer &layer,
                                                const int current_frame,
                                                const bool use_multi_frame_editing)
{
  Vector<int> frame_numbers;
  if (use_multi_frame_editing) {
    bool current_frame_is_covered = false;
    const int drawing_index_at_current_frame = layer.drawing_index_at(current_frame);
    for (const auto [frame_number, frame] : layer.frames().items()) {
      if (!frame.is_selected()) {
        continue;
      }
      frame_numbers.append(frame_number);
      current_frame_is_covered |= (frame.drawing_index == drawing_index_at_current_frame);
    }
    if (current_frame_is_covered) {
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
        layer, current_frame, use_multi_frame_editing);
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
  int center_frame;
  std::pair<int, int> minmax_frame;
  if (use_multi_frame_falloff) {
    BKE_curvemapping_init(toolsettings->gp_sculpt.cur_falloff);
    minmax_frame = get_minmax_selected_frame_numbers(grease_pencil, current_frame);
    center_frame = math::clamp(current_frame, minmax_frame.first, minmax_frame.second);
  }

  Vector<MutableDrawingInfo> editable_drawings;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer &layer = *layers[layer_i];
    if (!layer.is_editable()) {
      continue;
    }
    const Array<int> frame_numbers = get_editable_frames_for_layer(
        layer, current_frame, use_multi_frame_editing);
    for (const int frame_number : frame_numbers) {
      if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
        const float falloff = use_multi_frame_falloff ?
                                  get_multi_frame_falloff(frame_number,
                                                          center_frame,
                                                          minmax_frame.first,
                                                          minmax_frame.second,
                                                          toolsettings->gp_sculpt.cur_falloff) :
                                  1.0f;
        editable_drawings.append({*drawing, layer_i, frame_number, falloff});
      }
    }
  }

  return editable_drawings;
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
      layer, current_frame, use_multi_frame_editing);
  for (const int frame_number : frame_numbers) {
    if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
      editable_drawings.append({*drawing, layer_index, frame_number, 1.0f});
    }
  }

  return editable_drawings;
}

Vector<DrawingInfo> retrieve_visible_drawings(const Scene &scene,
                                              const GreasePencil &grease_pencil,
                                              const bool do_onion_skinning)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
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

static VectorSet<int> get_editable_material_indices(Object &object)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  VectorSet<int> editable_material_indices;
  for (const int mat_i : IndexRange(object.totcol)) {
    Material *material = BKE_object_material_get(&object, mat_i + 1);
    /* The editable materials are unlocked and not hidden. */
    if (material != nullptr && material->gp_style != nullptr &&
        (material->gp_style->flag & GP_MATERIAL_LOCKED) == 0 &&
        (material->gp_style->flag & GP_MATERIAL_HIDE) == 0)
    {
      editable_material_indices.add_new(mat_i);
    }
  }
  return editable_material_indices;
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

IndexMask retrieve_editable_strokes(Object &object,
                                    const bke::greasepencil::Drawing &drawing,
                                    IndexMaskMemory &memory)
{
  using namespace blender;

  /* Get all the editable material indices */
  VectorSet<int> editable_material_indices = get_editable_material_indices(object);
  if (editable_material_indices.is_empty()) {
    return {};
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = drawing.strokes().curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  const VArray<int> materials = *attributes.lookup<int>("material_index", bke::AttrDomain::Curve);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (editable_material_indices.contains(0)) {
      return curves_range;
    }
    return {};
  }
  /* Get all the strokes that have their material unlocked. */
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        return editable_material_indices.contains(material_index);
      });
}

IndexMask retrieve_editable_strokes_by_material(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                const int mat_i,
                                                IndexMaskMemory &memory)
{
  using namespace blender;

  /* Get all the editable material indices */
  VectorSet<int> editable_material_indices = get_editable_material_indices(object);
  if (editable_material_indices.is_empty()) {
    return {};
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = drawing.strokes().curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  const VArray<int> materials = *attributes.lookup<int>("material_index", bke::AttrDomain::Curve);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (editable_material_indices.contains(0)) {
      return curves_range;
    }
    return {};
  }
  /* Get all the strokes that share the same material and have it unlocked. */
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        if (material_index == mat_i) {
          return editable_material_indices.contains(material_index);
        }
        return false;
      });
}

IndexMask retrieve_editable_points(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory)
{
  /* Get all the editable material indices */
  VectorSet<int> editable_material_indices = get_editable_material_indices(object);
  if (editable_material_indices.is_empty()) {
    return {};
  }

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points_range = drawing.strokes().points_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Propagate the material index to the points. */
  const VArray<int> materials = *attributes.lookup<int>("material_index", bke::AttrDomain::Point);
  if (!materials) {
    /* If the attribute does not exist then the default is the first material. */
    if (editable_material_indices.contains(0)) {
      return points_range;
    }
    return {};
  }
  /* Get all the points that are part of a stroke with an unlocked material. */
  return IndexMask::from_predicate(
      points_range, GrainSize(4096), memory, [&](const int64_t point_i) {
        const int material_index = materials[point_i];
        return editable_material_indices.contains(material_index);
      });
}

IndexMask retrieve_editable_elements(Object &object,
                                     const bke::greasepencil::Drawing &drawing,
                                     const bke::AttrDomain selection_domain,
                                     IndexMaskMemory &memory)
{
  if (selection_domain == bke::AttrDomain::Curve) {
    return ed::greasepencil::retrieve_editable_strokes(object, drawing, memory);
  }
  else if (selection_domain == bke::AttrDomain::Point) {
    return ed::greasepencil::retrieve_editable_points(object, drawing, memory);
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
      "material_index", bke::AttrDomain::Curve, -1);
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        return !hidden_material_indices.contains(material_index);
      });
}

IndexMask retrieve_editable_and_selected_strokes(Object &object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 IndexMaskMemory &memory)
{
  using namespace blender;

  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_strokes = retrieve_editable_strokes(object, drawing, memory);
  const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves, memory);

  return IndexMask::from_intersection(editable_strokes, selected_strokes, memory);
}

IndexMask retrieve_editable_and_selected_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = drawing.strokes();

  const IndexMask editable_points = retrieve_editable_points(object, drawing, memory);
  const IndexMask selected_points = ed::curves::retrieve_selected_points(curves, memory);

  return IndexMask::from_intersection(editable_points, selected_points, memory);
}

IndexMask retrieve_editable_and_selected_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  const bke::AttrDomain selection_domain,
                                                  IndexMaskMemory &memory)
{
  if (selection_domain == bke::AttrDomain::Curve) {
    return ed::greasepencil::retrieve_editable_and_selected_strokes(object, drawing, memory);
  }
  else if (selection_domain == bke::AttrDomain::Point) {
    return ed::greasepencil::retrieve_editable_and_selected_points(object, drawing, memory);
  }
  return {};
}

}  // namespace blender::ed::greasepencil
