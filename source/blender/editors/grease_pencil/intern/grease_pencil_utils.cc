/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"

#include "BLI_bit_span_ops.hh"
#include "BLI_bit_vector.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

namespace blender::ed::greasepencil {

static float3 drawing_origin(const Scene *scene, const Object *object, char align_flag)
{
  BLI_assert(object != nullptr && object->type == OB_GREASE_PENCIL);
  if (align_flag & GP_PROJECT_VIEWSPACE) {
    if (align_flag & GP_PROJECT_CURSOR) {
      return float3(scene->cursor.location);
    }
    /* Use the object location. */
    return float3(object->object_to_world[3]);
  }
  return float3(scene->cursor.location);
}

static float3 screen_space_to_3d(
    const Scene *scene, const ARegion *region, const View3D *v3d, const Object *object, float2 co)
{
  float3 origin = drawing_origin(scene, object, scene->toolsettings->gpencil_v3d_align);
  float3 r_co;
  ED_view3d_win_to_3d(v3d, region, origin, co, r_co);
  return r_co;
}

float brush_radius_world_space(bContext &C, int x, int y)
{
  ARegion *region = CTX_wm_region(&C);
  View3D *v3d = CTX_wm_view3d(&C);
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  Brush *brush = scene->toolsettings->gp_paint->paint.brush;

  /* Default radius. */
  float radius = 2.0f;
  if (brush == nullptr || object->type != OB_GREASE_PENCIL) {
    return radius;
  }

  /* Use an (arbitrary) screen space offset in the x direction to measure the size. */
  const int x_offest = 64;
  const float brush_size = float(BKE_brush_size_get(scene, brush));

  /* Get two 3d coordinates to measure the distance from. */
  const float2 screen1(x, y);
  const float2 screen2(x + x_offest, y);
  const float3 pos1 = screen_space_to_3d(scene, region, v3d, object, screen1);
  const float3 pos2 = screen_space_to_3d(scene, region, v3d, object, screen2);

  /* Clip extreme zoom level (and avoid division by zero). */
  const float distance = math::max(math::distance(pos1, pos2), 0.001f);

  /* Calculate the radius of the brush in world space. */
  radius = (1.0f / distance) * (brush_size / 64.0f);

  return radius;
}

static Array<int> get_frame_numbers_for_layer(const bke::greasepencil::Layer &layer,
                                              const int current_frame,
                                              const bool use_multi_frame_editing)
{
  Vector<int> frame_numbers({current_frame});
  if (use_multi_frame_editing) {
    for (const auto [frame_number, frame] : layer.frames().items()) {
      if (frame_number != current_frame && frame.is_selected()) {
        frame_numbers.append_unchecked(frame_number);
      }
    }
  }
  return frame_numbers.as_span();
}

Array<MutableDrawingInfo> retrieve_editable_drawings(const Scene &scene,
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
    const Layer *layer = layers[layer_i];
    if (!layer->is_editable()) {
      continue;
    }
    const Array<int> frame_numbers = get_frame_numbers_for_layer(
        *layer, current_frame, use_multi_frame_editing);
    for (const int frame_number : frame_numbers) {
      if (Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number)) {
        editable_drawings.append({*drawing, layer_i, frame_number});
      }
    }
  }

  return editable_drawings.as_span();
}

Array<DrawingInfo> retrieve_visible_drawings(const Scene &scene, const GreasePencil &grease_pencil)
{
  using namespace blender::bke::greasepencil;
  const int current_frame = scene.r.cfra;
  const ToolSettings *toolsettings = scene.toolsettings;
  const bool use_multi_frame_editing = (toolsettings->gpencil_flags &
                                        GP_USE_MULTI_FRAME_EDITING) != 0;

  Vector<DrawingInfo> visible_drawings;
  Span<const Layer *> layers = grease_pencil.layers();
  for (const int layer_i : layers.index_range()) {
    const Layer *layer = layers[layer_i];
    if (!layer->is_visible()) {
      continue;
    }
    const Array<int> frame_numbers = get_frame_numbers_for_layer(
        *layer, current_frame, use_multi_frame_editing);
    for (const int frame_number : frame_numbers) {
      if (const Drawing *drawing = grease_pencil.get_drawing_at(layer, frame_number)) {
        visible_drawings.append({*drawing, layer_i, frame_number});
      }
    }
  }

  return visible_drawings.as_span();
}

static VectorSet<int> get_locked_material_indices(Object &object)
{
  BLI_assert(object.type == OB_GREASE_PENCIL);
  VectorSet<int> locked_material_indices;
  for (const int mat_i : IndexRange(object.totcol)) {
    Material *material = BKE_object_material_get(&object, mat_i + 1);
    if (material != nullptr && material->gp_style != nullptr &&
        (material->gp_style->flag & GP_MATERIAL_LOCKED) != 0)
    {
      locked_material_indices.add(mat_i);
    }
  }
  return locked_material_indices;
}

IndexMask retrieve_editable_strokes(Object &object,
                                    const bke::greasepencil::Drawing &drawing,
                                    IndexMaskMemory &memory)
{
  using namespace blender;

  /* Get all the locked material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = drawing.strokes().curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Get all the strokes that have their material unlocked. */
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_CURVE, -1);
  return IndexMask::from_predicate(
      curves_range, GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int material_index = materials[curve_i];
        return !locked_material_indices.contains(material_index);
      });
}

IndexMask retrieve_editable_points(Object &object,
                                   const bke::greasepencil::Drawing &drawing,
                                   IndexMaskMemory &memory)
{
  /* Get all the locked material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points_range = drawing.strokes().points_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Propagate the material index to the points. */
  const VArray<int> materials = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_POINT, -1);
  /* Get all the points that are part of a stroke with an unlocked material. */
  return IndexMask::from_predicate(
      points_range, GrainSize(4096), memory, [&](const int64_t point_i) {
        const int material_index = materials[point_i];
        return !locked_material_indices.contains(material_index);
      });
}

IndexMask retrieve_editable_elements(Object &object,
                                     const bke::greasepencil::Drawing &drawing,
                                     const eAttrDomain selection_domain,
                                     IndexMaskMemory &memory)
{
  if (selection_domain == ATTR_DOMAIN_CURVE) {
    return ed::greasepencil::retrieve_editable_strokes(object, drawing, memory);
  }
  else if (selection_domain == ATTR_DOMAIN_POINT) {
    return ed::greasepencil::retrieve_editable_points(object, drawing, memory);
  }
  return {};
}

IndexMask retrieve_editable_and_selected_strokes(Object &object,
                                                 const bke::greasepencil::Drawing &drawing,
                                                 IndexMaskMemory &memory)
{
  using namespace blender;

  /* Get all the locked material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange curves_range = drawing.strokes().curves_range();

  const IndexMask editable_strokes = retrieve_editable_strokes(object, drawing, memory);
  const IndexMask selected_strokes = ed::curves::retrieve_selected_curves(curves, memory);

  BitVector<> editable_strokes_bits(curves.curves_num(), false);
  editable_strokes.to_bits(editable_strokes_bits);
  BitVector<> selected_strokes_bits(curves.curves_num(), false);
  selected_strokes.to_bits(selected_strokes_bits);

  editable_strokes_bits &= selected_strokes_bits;
  return IndexMask::from_bits(curves_range, editable_strokes_bits, memory);
}

IndexMask retrieve_editable_and_selected_points(Object &object,
                                                const bke::greasepencil::Drawing &drawing,
                                                IndexMaskMemory &memory)
{
  /* Get all the locked material indices */
  VectorSet<int> locked_material_indices = get_locked_material_indices(object);

  const bke::CurvesGeometry &curves = drawing.strokes();
  const IndexRange points_range = drawing.strokes().points_range();

  const IndexMask editable_points = retrieve_editable_points(object, drawing, memory);
  const IndexMask selected_points = ed::curves::retrieve_selected_points(curves, memory);

  BitVector<> editable_points_bits(curves.points_num(), false);
  editable_points.to_bits(editable_points_bits);
  BitVector<> selected_points_bits(curves.points_num(), false);
  selected_points.to_bits(selected_points_bits);

  editable_points_bits &= selected_points_bits;
  return IndexMask::from_bits(points_range, editable_points_bits, memory);
}

IndexMask retrieve_editable_and_selected_elements(Object &object,
                                                  const bke::greasepencil::Drawing &drawing,
                                                  const eAttrDomain selection_domain,
                                                  IndexMaskMemory &memory)
{
  if (selection_domain == ATTR_DOMAIN_CURVE) {
    return ed::greasepencil::retrieve_editable_and_selected_strokes(object, drawing, memory);
  }
  else if (selection_domain == ATTR_DOMAIN_POINT) {
    return ed::greasepencil::retrieve_editable_and_selected_points(object, drawing, memory);
  }
  return {};
}

}  // namespace blender::ed::greasepencil
