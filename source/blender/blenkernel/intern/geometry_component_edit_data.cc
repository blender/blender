/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"

namespace blender::bke {

GeometryComponentEditData::GeometryComponentEditData() : GeometryComponent(Type::Edit) {}

GeometryComponent *GeometryComponentEditData::copy() const
{
  GeometryComponentEditData *new_component = new GeometryComponentEditData();
  if (curves_edit_hints_) {
    new_component->curves_edit_hints_ = std::make_unique<CurvesEditHints>(*curves_edit_hints_);
  }
  if (grease_pencil_edit_hints_) {
    new_component->grease_pencil_edit_hints_ = std::make_unique<GreasePencilEditHints>(
        *grease_pencil_edit_hints_);
  }
  return new_component;
}

bool GeometryComponentEditData::owns_direct_data() const
{
  return true;
}

void GeometryComponentEditData::ensure_owns_direct_data()
{
  /* Nothing to do. */
}

void GeometryComponentEditData::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  curves_edit_hints_.reset();
  grease_pencil_edit_hints_.reset();
}

static void remember_deformed_curve_positions_if_necessary(
    const Curves *curves_id, GeometryComponentEditData &edit_component)
{
  if (!edit_component.curves_edit_hints_) {
    return;
  }
  if (edit_component.curves_edit_hints_->positions.has_value()) {
    return;
  }
  if (curves_id == nullptr) {
    return;
  }
  const CurvesGeometry &curves = curves_id->geometry.wrap();
  const int points_num = curves.points_num();
  if (points_num != edit_component.curves_edit_hints_->curves_id_orig.geometry.point_num) {
    return;
  }
  edit_component.curves_edit_hints_->positions.emplace(points_num);
  edit_component.curves_edit_hints_->positions->as_mutable_span().copy_from(curves.positions());
}

static void remember_deformed_grease_pencil_if_necessary(const GreasePencil *grease_pencil,
                                                         GeometryComponentEditData &edit_component)
{
  if (!edit_component.grease_pencil_edit_hints_) {
    return;
  }
  if (edit_component.grease_pencil_edit_hints_->drawing_hints.has_value()) {
    return;
  }
  if (grease_pencil == nullptr) {
    return;
  }
  const GreasePencil &orig_grease_pencil =
      edit_component.grease_pencil_edit_hints_->grease_pencil_id_orig;
  const Span<const bke::greasepencil::Layer *> layers = grease_pencil->layers();
  const Span<const bke::greasepencil::Layer *> orig_layers = orig_grease_pencil.layers();
  const int layers_num = layers.size();
  if (layers_num != orig_layers.size()) {
    return;
  }
  edit_component.grease_pencil_edit_hints_->drawing_hints.emplace(layers_num);
  MutableSpan<GreasePencilDrawingEditHints> all_hints =
      *edit_component.grease_pencil_edit_hints_->drawing_hints;
  for (const int layer_index : layers.index_range()) {
    const bke::greasepencil::Drawing *drawing = greasepencil::get_eval_grease_pencil_layer_drawing(
        *grease_pencil, layer_index);
    const bke::greasepencil::Layer &orig_layer = *orig_layers[layer_index];
    const bke::greasepencil::Drawing *orig_drawing = orig_grease_pencil.get_drawing_at(
        &orig_layer, grease_pencil->runtime->eval_frame);
    GreasePencilDrawingEditHints &drawing_hints = all_hints[layer_index];

    if (!drawing || !orig_drawing) {
      continue;
    }
    if (drawing->strokes().points_num() != orig_drawing->strokes().points_num()) {
      continue;
    }
    drawing_hints.positions.emplace(drawing->strokes().positions());
  }
}

void GeometryComponentEditData::remember_deformed_positions_if_necessary(GeometrySet &geometry)
{
  /* This component should be created at the start of object evaluation if it's necessary. */
  if (!geometry.has<GeometryComponentEditData>()) {
    return;
  }
  GeometryComponentEditData &edit_component =
      geometry.get_component_for_write<GeometryComponentEditData>();
  remember_deformed_curve_positions_if_necessary(geometry.get_curves(), edit_component);
  remember_deformed_grease_pencil_if_necessary(geometry.get_grease_pencil(), edit_component);
}

}  // namespace blender::bke
