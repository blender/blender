/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"
#include "BLI_assert.h"
#include "BLI_index_mask.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_rect.h"

#include "BKE_attribute.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "ED_curves.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"
#include <optional>

namespace blender::ed::curves {

IndexMask retrieve_selected_curves(const bke::CurvesGeometry &curves, IndexMaskMemory &memory)
{
  const IndexRange curves_range = curves.curves_range();
  const VArray<int8_t> curve_types = curves.curve_types();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Interpolate from points to curves manually as a performance improvement, since we are only
   * interested in whether any point in each curve is selected. Retrieve meta data since
   * #lookup_or_default from the attribute API doesn't give the domain of the attribute. */
  std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(".selection");
  if (meta_data && meta_data->domain == bke::AttrDomain::Point) {
    /* Avoid the interpolation from interpolating the attribute to the
     * curve domain by retrieving the point domain values directly. */
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    const VArray<bool> selection_left = *attributes.lookup_or_default<bool>(
        ".selection_handle_left", bke::AttrDomain::Point, true);
    const VArray<bool> selection_right = *attributes.lookup_or_default<bool>(
        ".selection_handle_right", bke::AttrDomain::Point, true);

    if (selection.is_single() && curves.is_single_type(CURVE_TYPE_POLY)) {
      return selection.get_internal_single() ? IndexMask(curves_range) : IndexMask();
    }

    const OffsetIndices points_by_curve = curves.points_by_curve();
    return IndexMask::from_predicate(
        curves_range, GrainSize(512), memory, [&](const int64_t curve) {
          const IndexRange points = points_by_curve[curve];
          /* The curve is selected if any of its points are selected. */
          Array<bool, 32> point_selection(points.size());
          selection.materialize_compressed(points, point_selection);
          bool is_selected = point_selection.as_span().contains(true);
          if (curve_types[curve] == CURVE_TYPE_BEZIER) {
            selection_left.materialize_compressed(points, point_selection);
            is_selected |= point_selection.as_span().contains(true);
            selection_right.materialize_compressed(points, point_selection);
            is_selected |= point_selection.as_span().contains(true);
          }
          return is_selected;
        });
  }
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Curve, true);
  return IndexMask::from_bools(curves_range, selection, memory);
}

IndexMask retrieve_selected_curves(const Curves &curves_id, IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  return retrieve_selected_curves(curves, memory);
}

IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves, IndexMaskMemory &memory)
{
  return IndexMask::from_bools(
      *curves.attributes().lookup_or_default<bool>(".selection", bke::AttrDomain::Point, true),
      memory);
}

IndexMask retrieve_all_selected_points(const bke::CurvesGeometry &curves,
                                       const int handle_display,
                                       IndexMaskMemory &memory)
{
  const IndexMask bezier_points = bke::curves::curve_type_point_selection(
      curves, CURVE_TYPE_BEZIER, memory);

  Vector<IndexMask> selection_by_attribute;
  for (const StringRef selection_name : ed::curves::get_curves_selection_attribute_names(curves)) {
    if (selection_name != ".selection" && handle_display == CURVE_HANDLE_NONE) {
      continue;
    }

    selection_by_attribute.append(
        ed::curves::retrieve_selected_points(curves, selection_name, bezier_points, memory));
  }
  return IndexMask::from_union(selection_by_attribute, memory);
}

IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves,
                                   StringRef attribute_name,
                                   const IndexMask &bezier_points,
                                   IndexMaskMemory &memory)
{
  const VArray<bool> selected = *curves.attributes().lookup_or_default<bool>(
      attribute_name, bke::AttrDomain::Point, true);

  if (attribute_name == ".selection") {
    return IndexMask::from_bools(selected, memory);
  }

  return IndexMask::from_bools(bezier_points, selected, memory);
}

IndexMask retrieve_selected_points(const Curves &curves_id, IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  return retrieve_selected_points(curves, memory);
}

Span<StringRef> get_curves_selection_attribute_names(const bke::CurvesGeometry &curves)
{
  static const std::array<StringRef, 1> selection_attribute_names{".selection"};
  return curves.has_curve_with_type(CURVE_TYPE_BEZIER) ?
             get_curves_all_selection_attribute_names() :
             selection_attribute_names;
}

Span<StringRef> get_curves_all_selection_attribute_names()
{
  static const std::array<StringRef, 3> selection_attribute_names{
      ".selection", ".selection_handle_left", ".selection_handle_right"};
  return selection_attribute_names;
}

Span<StringRef> get_curves_bezier_selection_attribute_names(const bke::CurvesGeometry &curves)
{
  static const std::array<StringRef, 2> selection_attribute_names{".selection_handle_left",
                                                                  ".selection_handle_right"};
  const bke::AttributeAccessor attributes = curves.attributes();
  return (attributes.contains("handle_type_left") && attributes.contains("handle_type_right")) ?
             selection_attribute_names :
             Span<StringRef>();
}

void remove_selection_attributes(bke::MutableAttributeAccessor &attributes,
                                 Span<StringRef> selection_attribute_names)
{
  for (const StringRef selection_name : selection_attribute_names) {
    attributes.remove(selection_name);
  }
}

std::optional<Span<float3>> get_selection_attribute_positions(
    const bke::CurvesGeometry &curves,
    const bke::crazyspace::GeometryDeformation &deformation,
    const StringRef attribute_name)
{
  if (attribute_name == ".selection") {
    return deformation.positions;
  }
  if (attribute_name == ".selection_handle_left") {
    return curves.handle_positions_left();
  }
  if (attribute_name == ".selection_handle_right") {
    return curves.handle_positions_right();
  }
  BLI_assert_unreachable();
  return {};
}

static Vector<bke::GSpanAttributeWriter> init_selection_writers(bke::CurvesGeometry &curves,
                                                                bke::AttrDomain selection_domain)
{
  const bke::AttrType create_type = bke::AttrType::Bool;
  Span<StringRef> selection_attribute_names = get_curves_selection_attribute_names(curves);
  Vector<bke::GSpanAttributeWriter> writers;
  for (const int i : selection_attribute_names.index_range()) {
    writers.append(ensure_selection_attribute(
        curves, selection_domain, create_type, selection_attribute_names[i]));
  };
  return writers;
}

static void finish_attribute_writers(MutableSpan<bke::GSpanAttributeWriter> attribute_writers)
{
  for (auto &attribute_writer : attribute_writers) {
    attribute_writer.finish();
  }
}

static bke::GSpanAttributeWriter &selection_attribute_writer_by_name(
    MutableSpan<bke::GSpanAttributeWriter> selections, StringRef attribute_name)
{
  Span<StringRef> selection_attribute_names = get_curves_all_selection_attribute_names();

  BLI_assert(selection_attribute_names.contains(attribute_name));

  for (const int index : selections.index_range()) {
    if (attribute_name.size() == selection_attribute_names[index].size()) {
      return selections[index];
    }
  }
  BLI_assert_unreachable();
  return selections[0];
}

void foreach_selection_attribute_writer(
    bke::CurvesGeometry &curves,
    bke::AttrDomain selection_domain,
    blender::FunctionRef<void(bke::GSpanAttributeWriter &selection)> fn)
{
  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(curves,
                                                                               selection_domain);
  for (bke::GSpanAttributeWriter &selection_writer : selection_writers) {
    fn(selection_writer);
  }
  finish_attribute_writers(selection_writers);
}

static void init_selectable_foreach(
    const bke::CurvesGeometry &curves,
    const bke::crazyspace::GeometryDeformation &deformation,
    eHandleDisplay handle_display,
    Span<StringRef> &r_bezier_attribute_names,
    Span<float3> &r_positions,
    std::optional<std::array<Span<float3>, 2>> &r_bezier_handle_positions,
    IndexMaskMemory &r_memory,
    IndexMask &r_bezier_curves)
{
  r_bezier_attribute_names = get_curves_bezier_selection_attribute_names(curves);
  r_positions = deformation.positions;
  if (handle_display != eHandleDisplay::CURVE_HANDLE_NONE && r_bezier_attribute_names.size() > 0) {
    r_bezier_handle_positions = {*curves.handle_positions_left(),
                                 *curves.handle_positions_right()};
    r_bezier_curves = curves.indices_for_curve_type(CURVE_TYPE_BEZIER, r_memory);
  }
  else {
    r_bezier_handle_positions = std::nullopt;
  }
}

void foreach_selectable_point_range(const bke::CurvesGeometry &curves,
                                    const bke::crazyspace::GeometryDeformation &deformation,
                                    eHandleDisplay handle_display,
                                    SelectionRangeFn range_consumer)
{
  Span<StringRef> bezier_attribute_names;
  Span<float3> positions;
  std::optional<std::array<Span<float3>, 2>> bezier_handle_positions;
  IndexMaskMemory memory;
  IndexMask bezier_curves;
  init_selectable_foreach(curves,
                          deformation,
                          handle_display,
                          bezier_attribute_names,
                          positions,
                          bezier_handle_positions,
                          memory,
                          bezier_curves);

  range_consumer(curves.points_range(), positions, ".selection");

  if (handle_display == eHandleDisplay::CURVE_HANDLE_NONE) {
    return;
  }

  OffsetIndices<int> points_by_curve = curves.points_by_curve();
  for (const int attribute_i : bezier_attribute_names.index_range()) {
    bezier_curves.foreach_index(GrainSize(512), [&](const int64_t curve) {
      range_consumer(points_by_curve[curve],
                     (*bezier_handle_positions)[attribute_i],
                     bezier_attribute_names[attribute_i]);
    });
  }
}

void foreach_selectable_curve_range(const bke::CurvesGeometry &curves,
                                    const bke::crazyspace::GeometryDeformation &deformation,
                                    eHandleDisplay handle_display,
                                    SelectionRangeFn range_consumer)
{
  Span<StringRef> bezier_attribute_names;
  Span<float3> positions;
  std::optional<std::array<Span<float3>, 2>> bezier_handle_positions;
  IndexMaskMemory memory;
  IndexMask bezier_curves;
  init_selectable_foreach(curves,
                          deformation,
                          handle_display,
                          bezier_attribute_names,
                          positions,
                          bezier_handle_positions,
                          memory,
                          bezier_curves);

  range_consumer(curves.curves_range(), positions, ".selection");
  if (handle_display == eHandleDisplay::CURVE_HANDLE_NONE) {
    return;
  }

  for (const int attribute_i : bezier_attribute_names.index_range()) {
    bezier_curves.foreach_range([&](const IndexRange curves_range) {
      range_consumer(curves_range,
                     (*bezier_handle_positions)[attribute_i],
                     bezier_attribute_names[attribute_i]);
    });
  }
}

bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     bke::AttrDomain selection_domain,
                                                     bke::AttrType create_type,
                                                     StringRef attribute_name)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (attributes.contains(attribute_name)) {
    bke::GSpanAttributeWriter selection_attr = attributes.lookup_for_write_span(attribute_name);
    /* Check domain type. */
    if (selection_attr.domain == selection_domain) {
      return selection_attr;
    }
    selection_attr.finish();
    attributes.remove(attribute_name);
  }
  const int domain_size = attributes.domain_size(selection_domain);
  switch (create_type) {
    case bke::AttrType::Bool:
      attributes.add(attribute_name,
                     selection_domain,
                     bke::AttrType::Bool,
                     bke::AttributeInitVArray(VArray<bool>::from_single(true, domain_size)));
      break;
    case bke::AttrType::Float:
      attributes.add(attribute_name,
                     selection_domain,
                     bke::AttrType::Float,
                     bke::AttributeInitVArray(VArray<float>::from_single(1.0f, domain_size)));
      break;
    default:
      BLI_assert_unreachable();
  }
  return attributes.lookup_for_write_span(attribute_name);
}

void fill_selection_false(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(false);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(0.0f);
  }
}

void fill_selection_true(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(true);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(1.0f);
  }
}

void fill_selection(GMutableSpan selection, bool value)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(value);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(value ? 1.0f : 0.0f);
  }
}

void fill_selection_false(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    index_mask::masked_fill(selection.typed<bool>(), false, mask);
  }
  else if (selection.type().is<float>()) {
    index_mask::masked_fill(selection.typed<float>(), 0.0f, mask);
  }
}

void fill_selection_true(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    index_mask::masked_fill(selection.typed<bool>(), true, mask);
  }
  else if (selection.type().is<float>()) {
    index_mask::masked_fill(selection.typed<float>(), 1.0f, mask);
  }
}

bool has_anything_selected(const VArray<bool> &varray, const IndexRange range_to_check)
{
  return array_utils::contains(varray, range_to_check, true);
}

bool has_anything_selected(const VArray<bool> &varray, const IndexMask &indices_to_check)
{
  return array_utils::contains(varray, indices_to_check, true);
}

bool has_anything_selected(const bke::CurvesGeometry &curves)
{
  const VArray<bool> selection = *curves.attributes().lookup<bool>(".selection");
  return !selection || array_utils::contains(selection, selection.index_range(), true);
}

bool has_anything_selected(const bke::CurvesGeometry &curves, bke::AttrDomain selection_domain)
{
  return has_anything_selected(
      curves, selection_domain, IndexRange(curves.attributes().domain_size(selection_domain)));
}

bool has_anything_selected(const bke::CurvesGeometry &curves,
                           bke::AttrDomain selection_domain,
                           const IndexMask &mask)
{
  for (const StringRef selection_name : get_curves_selection_attribute_names(curves)) {
    const VArray<bool> selection = *curves.attributes().lookup<bool>(selection_name,
                                                                     selection_domain);
    if (!selection || array_utils::contains(selection, mask, true)) {
      return true;
    }
  }
  return false;
}

bool has_anything_selected(const GSpan selection)
{
  if (selection.type().is<bool>()) {
    return selection.typed<bool>().contains(true);
  }
  if (selection.type().is<float>()) {
    for (const float elem : selection.typed<float>()) {
      if (elem > 0.0f) {
        return true;
      }
    }
  }
  return false;
}

static void invert_selection(MutableSpan<float> selection, const IndexMask &mask)
{
  mask.foreach_index_optimized<int64_t>(
      GrainSize(2048), [&](const int64_t i) { selection[i] = 1.0f - selection[i]; });
}

static void invert_selection(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    array_utils::invert_booleans(selection.typed<bool>(), mask);
  }
  else if (selection.type().is<float>()) {
    invert_selection(selection.typed<float>(), mask);
  }
}

static void invert_selection(GMutableSpan selection)
{
  invert_selection(selection, IndexRange(selection.size()));
}

void select_all(bke::CurvesGeometry &curves,
                const IndexMask &mask,
                const bke::AttrDomain selection_domain,
                int action)
{
  if (action == SEL_SELECT) {
    std::optional<IndexRange> range = mask.to_range();
    if (range.has_value() &&
        (*range == IndexRange(curves.attributes().domain_size(selection_domain))))
    {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      /* As an optimization, just remove the selection attributes when everything is selected. */
      remove_selection_attributes(attributes);
      return;
    }
  }
  foreach_selection_attribute_writer(
      curves, selection_domain, [&](bke::GSpanAttributeWriter &selection) {
        if (action == SEL_SELECT) {
          fill_selection_true(selection.span, mask);
        }
        else if (action == SEL_DESELECT) {
          fill_selection_false(selection.span, mask);
        }
        else if (action == SEL_INVERT) {
          invert_selection(selection.span, mask);
        }
      });
}

void select_all(bke::CurvesGeometry &curves, const bke::AttrDomain selection_domain, int action)
{
  const IndexRange selection(curves.attributes().domain_size(selection_domain));
  select_all(curves, selection, selection_domain, action);
}

void select_linked(bke::CurvesGeometry &curves, const IndexMask &curves_mask)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> curve_types = curves.curve_types();
  const IndexRange all_writers = get_curves_all_selection_attribute_names().index_range();
  const IndexRange selection_writer = IndexRange(1);

  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(
      curves, bke::AttrDomain::Point);

  curves_mask.foreach_index(GrainSize(256), [&](const int64_t curve) {
    /* For Bezier curves check all three selection layers  ".selection", ".selection_handle_left",
     * ".selection_handle_right". For other curves only ".selection". */
    const IndexRange curve_writers = curve_types[curve] == CURVE_TYPE_BEZIER ? all_writers :
                                                                               selection_writer;
    const IndexRange points = points_by_curve[curve];

    for (const int i : curve_writers) {
      bke::GSpanAttributeWriter &selection = selection_writers[i];
      GMutableSpan selection_curve = selection.span.slice(points);
      if (has_anything_selected(selection_curve)) {
        fill_selection_true(selection_curve);
        for (const int j : curve_writers) {
          if (j == i) {
            continue;
          }
          fill_selection_true(selection_writers[j].span.slice(points));
        }
        return;
      }
    }
  });
  finish_attribute_writers(selection_writers);
}

void select_linked(bke::CurvesGeometry &curves)
{
  select_linked(curves, curves.curves_range());
}

void select_alternate(bke::CurvesGeometry &curves,
                      const IndexMask &curves_mask,
                      const bool deselect_ends)
{
  if (!has_anything_selected(curves)) {
    return;
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();
  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(
      curves, bke::AttrDomain::Point);

  curves_mask.foreach_index([&](const int64_t curve) {
    const IndexRange points = points_by_curve[curve];

    bool anything_selected = false;

    for (bke::GSpanAttributeWriter &writer : selection_writers) {
      const bool writer_has_anything_selected = has_anything_selected(writer.span.slice(points));
      anything_selected = anything_selected || writer_has_anything_selected;
    }
    if (!anything_selected) {
      return;
    }

    for (bke::GSpanAttributeWriter &writer : selection_writers) {
      MutableSpan<bool> selection_typed = writer.span.typed<bool>();

      const int half_of_size = points.size() / 2;
      const IndexRange selected = points.shift(deselect_ends ? 1 : 0);
      const IndexRange deselected = points.shift(deselect_ends ? 0 : 1);
      for (const int i : IndexRange(half_of_size)) {
        const int index = i * 2;
        selection_typed[selected[index]] = true;
        selection_typed[deselected[index]] = false;
      }

      selection_typed[points.first()] = !deselect_ends;
      const bool end_parity_to_selected = bool(points.size() % 2);
      const bool selected_end = cyclic[curve] || end_parity_to_selected;
      selection_typed[points.last()] = !deselect_ends && selected_end;
      /* Selected last one require to deselect pre-last one point which is not first. */
      const IndexRange curve_body = points.drop_front(1).drop_back(1);
      if (!deselect_ends && cyclic[curve] && !curve_body.is_empty()) {
        selection_typed[curve_body.last()] = false;
      }
    }
  });

  finish_attribute_writers(selection_writers);
}

void select_alternate(bke::CurvesGeometry &curves, const bool deselect_ends)
{
  select_alternate(curves, curves.curves_range(), deselect_ends);
}

void select_adjacent(bke::CurvesGeometry &curves,
                     const IndexMask &curves_mask,
                     const bool deselect)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, bke::AttrDomain::Point, bke::AttrType::Bool);
  const VArray<bool> cyclic = curves.cyclic();

  if (deselect) {
    invert_selection(selection.span);
  }

  if (selection.span.type().is<bool>()) {
    MutableSpan<bool> selection_typed = selection.span.typed<bool>();
    curves_mask.foreach_index([&](const int64_t curve) {
      const IndexRange points = points_by_curve[curve];
      const int first_point = points.first();
      const int last_point = points.last();

      /* Handle all cases in the forward direction. */
      for (int point = first_point; point < last_point; point++) {
        if (!selection_typed[point] && selection_typed[point + 1]) {
          selection_typed[point] = true;
        }
      }

      /* Handle all cases in the backwards direction. */
      for (int point = last_point; point > first_point; point--) {
        if (!selection_typed[point] && selection_typed[point - 1]) {
          selection_typed[point] = true;
        }
      }
      if (deselect) {
        if (!selection_typed[first_point]) {
          selection_typed[first_point] = true;
        }
        if (!selection_typed[last_point]) {
          selection_typed[last_point] = true;
        }
      }
      /* Handle cyclic curve case. */
      if (cyclic[curve]) {
        if (selection_typed[first_point] != selection_typed[last_point]) {
          selection_typed[first_point] = true;
          selection_typed[last_point] = true;
        }
      }
    });
  }
  else if (selection.span.type().is<float>()) {
    MutableSpan<float> selection_typed = selection.span.typed<float>();
    curves_mask.foreach_index([&](const int64_t curve) {
      const IndexRange points = points_by_curve[curve];
      const int first_point = points.first();
      const int last_point = points.last();

      /* Handle all cases in the forward direction. */
      for (int point_i = first_point; point_i < last_point; point_i++) {
        if ((selection_typed[point_i] == 0.0f) && (selection_typed[point_i + 1] > 0.0f)) {
          selection_typed[point_i] = 1.0f;
        }
      }

      /* Handle all cases in the backwards direction. */
      for (int point_i = last_point; point_i > first_point; point_i--) {
        if ((selection_typed[point_i] == 0.0f) && (selection_typed[point_i - 1] > 0.0f)) {
          selection_typed[point_i] = 1.0f;
        }
      }

      /* Handle cyclic curve case. */
      if (cyclic[curve]) {
        if (selection_typed[first_point] != selection_typed[last_point]) {
          selection_typed[first_point] = 1.0f;
          selection_typed[last_point] = 1.0f;
        }
      }
    });
  }

  if (deselect) {
    invert_selection(selection.span);
  }

  selection.finish();
}

void select_adjacent(bke::CurvesGeometry &curves, const bool deselect)
{
  select_adjacent(curves, curves.curves_range(), deselect);
}

void apply_selection_operation_at_index(GMutableSpan selection,
                                        const int index,
                                        const eSelectOp sel_op)
{
  if (selection.type().is<bool>()) {
    MutableSpan<bool> selection_typed = selection.typed<bool>();
    switch (sel_op) {
      case SEL_OP_ADD:
      case SEL_OP_SET:
        selection_typed[index] = true;
        break;
      case SEL_OP_SUB:
        selection_typed[index] = false;
        break;
      case SEL_OP_XOR:
        selection_typed[index] = !selection_typed[index];
        break;
      default:
        break;
    }
  }
  else if (selection.type().is<float>()) {
    MutableSpan<float> selection_typed = selection.typed<float>();
    switch (sel_op) {
      case SEL_OP_ADD:
      case SEL_OP_SET:
        selection_typed[index] = 1.0f;
        break;
      case SEL_OP_SUB:
        selection_typed[index] = 0.0f;
        break;
      case SEL_OP_XOR:
        selection_typed[index] = 1.0f - selection_typed[index];
        break;
      default:
        break;
    }
  }
}

static FindClosestData closer_elem(const FindClosestData &a, const FindClosestData &b)
{
  if (a.distance_sq < b.distance_sq) {
    return a;
  }
  return b;
}

static std::optional<FindClosestData> find_closest_point_to_screen_co(
    const ARegion *region,
    const Span<float3> positions,
    const float4x4 &projection,
    const IndexMask &points_mask,
    const float2 mouse_pos,
    const float radius,
    const FindClosestData &initial_closest)
{
  const float radius_sq = pow2f(radius);
  const FindClosestData new_closest_data = threading::parallel_reduce(
      points_mask.index_range(),
      1024,
      initial_closest,
      [&](const IndexRange range, const FindClosestData &init) {
        FindClosestData best_match = init;
        points_mask.slice(range).foreach_index([&](const int point) {
          const float3 &pos = positions[point];
          const float2 pos_proj = ED_view3d_project_float_v2_m4(region, pos, projection);

          const float distance_proj_sq = math::distance_squared(pos_proj, mouse_pos);
          if (distance_proj_sq > radius_sq || distance_proj_sq > best_match.distance_sq) {
            return;
          }

          best_match = {point, distance_proj_sq};
        });
        return best_match;
      },
      closer_elem);

  if (new_closest_data.distance_sq < initial_closest.distance_sq) {
    return new_closest_data;
  }

  return {};
}

static std::optional<FindClosestData> find_closest_curve_to_screen_co(
    const ARegion *region,
    const OffsetIndices<int> points_by_curve,
    const Span<float3> positions,
    const VArray<bool> &cyclic,
    const float4x4 &projection,
    const IndexMask &curves_mask,
    const float2 mouse_pos,
    float radius,
    const FindClosestData &initial_closest)
{
  const float radius_sq = pow2f(radius);

  const FindClosestData new_closest_data = threading::parallel_reduce(
      curves_mask.index_range(),
      256,
      initial_closest,
      [&](const IndexRange range, const FindClosestData &init) {
        FindClosestData best_match = init;
        curves_mask.slice(range).foreach_index([&](const int curve) {
          const IndexRange points = points_by_curve[curve];

          if (points.size() == 1) {
            const float3 &pos = positions[points.first()];
            const float2 pos_proj = ED_view3d_project_float_v2_m4(region, pos, projection);

            const float distance_proj_sq = math::distance_squared(pos_proj, mouse_pos);
            if (distance_proj_sq > radius_sq || distance_proj_sq > best_match.distance_sq) {
              return;
            }

            best_match = {curve, distance_proj_sq};
            return;
          }

          auto process_segment = [&](const int segment_i, const int next_i) {
            const float3 &pos1 = positions[segment_i];
            const float3 &pos2 = positions[next_i];
            const float2 pos1_proj = ED_view3d_project_float_v2_m4(region, pos1, projection);
            const float2 pos2_proj = ED_view3d_project_float_v2_m4(region, pos2, projection);

            const float distance_proj_sq = dist_squared_to_line_segment_v2(
                mouse_pos, pos1_proj, pos2_proj);
            if (distance_proj_sq > radius_sq || distance_proj_sq > best_match.distance_sq) {
              return;
            }

            best_match = {curve, distance_proj_sq};
          };
          for (const int segment_i : points.drop_back(1)) {
            process_segment(segment_i, segment_i + 1);
          }
          if (cyclic[curve]) {
            process_segment(points.last(), points.first());
          }
        });
        return best_match;
      },
      closer_elem);

  if (new_closest_data.distance_sq < initial_closest.distance_sq) {
    return new_closest_data;
  }

  return {};
}

std::optional<FindClosestData> closest_elem_find_screen_space(
    const ViewContext &vc,
    const OffsetIndices<int> points_by_curve,
    const Span<float3> positions,
    const VArray<bool> &cyclic,
    const float4x4 &projection,
    const IndexMask &mask,
    const bke::AttrDomain domain,
    const int2 coord,
    const FindClosestData &initial_closest)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return find_closest_point_to_screen_co(vc.region,
                                             positions,
                                             projection,
                                             mask,
                                             float2(coord),
                                             ED_view3d_select_dist_px(),
                                             initial_closest);
    case bke::AttrDomain::Curve:
      return find_closest_curve_to_screen_co(vc.region,
                                             points_by_curve,
                                             positions,
                                             cyclic,
                                             projection,
                                             mask,
                                             float2(coord),
                                             ED_view3d_select_dist_px(),
                                             initial_closest);
    default:
      BLI_assert_unreachable();
      return {};
  }
}

bool select_box(const ViewContext &vc,
                bke::CurvesGeometry &curves,
                const bke::crazyspace::GeometryDeformation &deformation,
                const float4x4 &projection,
                const IndexMask &selection_mask,
                const IndexMask &bezier_mask,
                const bke::AttrDomain selection_domain,
                const rcti &rect,
                const eSelectOp sel_op)
{
  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(curves,
                                                                               selection_domain);

  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    for (bke::GSpanAttributeWriter &selection : selection_writers) {
      fill_selection_false(selection.span, selection_mask);
    };
    changed = true;
  }

  if (selection_domain == bke::AttrDomain::Point) {
    foreach_selectable_point_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](IndexRange range, Span<float3> positions, StringRef selection_attribute_name) {
          const IndexMask &mask = (selection_attribute_name == ".selection") ? selection_mask :
                                                                               bezier_mask;
          mask.slice_content(range).foreach_index(GrainSize(1024), [&](const int point) {
            const float2 pos_proj = ED_view3d_project_float_v2_m4(
                vc.region, positions[point], projection);
            if (BLI_rcti_isect_pt_v(&rect, int2(pos_proj))) {
              apply_selection_operation_at_index(
                  selection_attribute_writer_by_name(selection_writers, selection_attribute_name)
                      .span,
                  point,
                  sel_op);
              changed = true;
            }
          });
        });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    foreach_selectable_curve_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](const IndexRange range,
            const Span<float3> positions,
            StringRef /* selection_attribute_name */) {
          const IndexMask &mask = selection_mask;
          mask.slice_content(range).foreach_index(GrainSize(512), [&](const int curve) {
            const IndexRange points = points_by_curve[curve];
            if (points.size() == 1) {
              const float2 pos_proj = ED_view3d_project_float_v2_m4(
                  vc.region, positions[points.first()], projection);
              if (BLI_rcti_isect_pt_v(&rect, int2(pos_proj))) {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                };
                changed = true;
              }
              return;
            }
            auto process_segment = [&](const int segment_i, const int next_i) {
              const float3 &pos1 = positions[segment_i];
              const float3 &pos2 = positions[next_i];
              const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection);
              const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection);

              if (BLI_rcti_isect_segment(&rect, int2(pos1_proj), int2(pos2_proj))) {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                };
                changed = true;
                return true;
              }
              return false;
            };
            bool segment_selected = false;
            for (const int segment_i : points.drop_back(1)) {
              if (process_segment(segment_i, segment_i + 1)) {
                segment_selected = true;
                break;
              }
            }
            if (!segment_selected && cyclic[curve]) {
              process_segment(points.last(), points.first());
            }
          });
        });
  }
  finish_attribute_writers(selection_writers);
  return changed;
}

bool select_lasso(const ViewContext &vc,
                  bke::CurvesGeometry &curves,
                  const bke::crazyspace::GeometryDeformation &deformation,
                  const float4x4 &projection,
                  const IndexMask &selection_mask,
                  const IndexMask &bezier_mask,
                  const bke::AttrDomain selection_domain,
                  const Span<int2> lasso_coords,
                  const eSelectOp sel_op)
{
  rcti bbox;
  BLI_lasso_boundbox(&bbox, lasso_coords);
  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(curves,
                                                                               selection_domain);
  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    for (bke::GSpanAttributeWriter &selection : selection_writers) {
      fill_selection_false(selection.span, selection_mask);
    };
    changed = true;
  }

  if (selection_domain == bke::AttrDomain::Point) {
    foreach_selectable_point_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](IndexRange range, Span<float3> positions, StringRef selection_attribute_name) {
          const IndexMask &mask = (selection_attribute_name == ".selection") ? selection_mask :
                                                                               bezier_mask;
          mask.slice_content(range).foreach_index(GrainSize(1024), [&](const int point) {
            const float2 pos_proj = ED_view3d_project_float_v2_m4(
                vc.region, positions[point], projection);
            /* Check the lasso bounding box first as an optimization. */
            if (BLI_rcti_isect_pt_v(&bbox, int2(pos_proj)) &&
                BLI_lasso_is_point_inside(
                    lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED))
            {
              apply_selection_operation_at_index(
                  selection_attribute_writer_by_name(selection_writers, selection_attribute_name)
                      .span,
                  point,
                  sel_op);
              changed = true;
            }
          });
        });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    foreach_selectable_curve_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](const IndexRange range,
            const Span<float3> positions,
            StringRef /* selection_attribute_name */) {
          const IndexMask &mask = selection_mask;
          mask.slice_content(range).foreach_index(GrainSize(512), [&](const int curve) {
            const IndexRange points = points_by_curve[curve];
            if (points.size() == 1) {
              const float2 pos_proj = ED_view3d_project_float_v2_m4(
                  vc.region, positions[points.first()], projection);
              /* Check the lasso bounding box first as an optimization. */
              if (BLI_rcti_isect_pt_v(&bbox, int2(pos_proj)) &&
                  BLI_lasso_is_point_inside(
                      lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED))
              {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                }
                changed = true;
              }
              return;
            }
            auto process_segment = [&](const int segment_i, const int next_i) {
              const float3 &pos1 = positions[segment_i];
              const float3 &pos2 = positions[next_i];
              const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection);
              const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection);

              /* Check the lasso bounding box first as an optimization. */
              if (BLI_rcti_isect_segment(&bbox, int2(pos1_proj), int2(pos2_proj)) &&
                  BLI_lasso_is_edge_inside(lasso_coords,
                                           int(pos1_proj.x),
                                           int(pos1_proj.y),
                                           int(pos2_proj.x),
                                           int(pos2_proj.y),
                                           IS_CLIPPED))
              {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                }
                changed = true;
                return true;
              }
              return false;
            };
            bool segment_selected = false;
            for (const int segment_i : points.drop_back(cyclic[curve] ? 0 : 1)) {
              if (process_segment(segment_i, segment_i + 1)) {
                segment_selected = true;
                break;
              }
            }
            if (!segment_selected && cyclic[curve]) {
              process_segment(points.last(), points.first());
            }
          });
        });
  }
  finish_attribute_writers(selection_writers);
  return changed;
}

bool select_circle(const ViewContext &vc,
                   bke::CurvesGeometry &curves,
                   const bke::crazyspace::GeometryDeformation &deformation,
                   const float4x4 &projection,
                   const IndexMask &selection_mask,
                   const IndexMask &bezier_mask,
                   const bke::AttrDomain selection_domain,
                   const int2 coord,
                   const float radius,
                   const eSelectOp sel_op)
{
  const float radius_sq = pow2f(radius);
  Vector<bke::GSpanAttributeWriter> selection_writers = init_selection_writers(curves,
                                                                               selection_domain);
  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    for (bke::GSpanAttributeWriter &selection : selection_writers) {
      fill_selection_false(selection.span, selection_mask);
    };
    changed = true;
  }

  if (selection_domain == bke::AttrDomain::Point) {
    foreach_selectable_point_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](IndexRange range, Span<float3> positions, StringRef selection_attribute_name) {
          const IndexMask &mask = (selection_attribute_name == ".selection") ? selection_mask :
                                                                               bezier_mask;
          mask.slice_content(range).foreach_index(GrainSize(1024), [&](const int point) {
            const float2 pos_proj = ED_view3d_project_float_v2_m4(
                vc.region, positions[point], projection);
            if (math::distance_squared(pos_proj, float2(coord)) <= radius_sq) {
              apply_selection_operation_at_index(
                  selection_attribute_writer_by_name(selection_writers, selection_attribute_name)
                      .span,
                  point,
                  sel_op);
              changed = true;
            }
          });
        });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    foreach_selectable_curve_range(
        curves,
        deformation,
        eHandleDisplay(vc.v3d->overlay.handle_display),
        [&](const IndexRange range,
            const Span<float3> positions,
            StringRef /* selection_attribute_name */) {
          const IndexMask &mask = selection_mask;
          mask.slice_content(range).foreach_index(GrainSize(512), [&](const int curve) {
            const IndexRange points = points_by_curve[curve];
            if (points.size() == 1) {
              const float2 pos_proj = ED_view3d_project_float_v2_m4(
                  vc.region, positions[points.first()], projection);
              if (math::distance_squared(pos_proj, float2(coord)) <= radius_sq) {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                }
                changed = true;
              }
              return;
            }
            auto process_segments = [&](const int segment_i, const int next_i) {
              const float3 &pos1 = positions[segment_i];
              const float3 &pos2 = positions[next_i];
              const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection);
              const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection);

              const float distance_proj_sq = dist_squared_to_line_segment_v2(
                  float2(coord), pos1_proj, pos2_proj);
              if (distance_proj_sq <= radius_sq) {
                for (bke::GSpanAttributeWriter &selection : selection_writers) {
                  apply_selection_operation_at_index(selection.span, curve, sel_op);
                }
                changed = true;
                return true;
              }
              return false;
            };
            bool segment_selected = false;
            for (const int segment_i : points.drop_back(1)) {
              if (process_segments(segment_i, segment_i + 1)) {
                segment_selected = true;
                break;
              }
            }
            if (!segment_selected && cyclic[curve]) {
              process_segments(points.last(), points.first());
            }
          });
        });
  }
  finish_attribute_writers(selection_writers);
  return changed;
}

template<typename PointSelectFn, typename LineSelectFn>
IndexMask select_mask_from_predicates(const bke::CurvesGeometry &curves,
                                      const IndexMask &mask,
                                      const bke::AttrDomain selection_domain,
                                      IndexMaskMemory &memory,
                                      PointSelectFn &&point_predicate,
                                      LineSelectFn &&line_predicate)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArraySpan<bool> cyclic = curves.cyclic();

  if (selection_domain == bke::AttrDomain::Point) {
    return IndexMask::from_predicate(
        mask.slice_content(curves.points_range()), GrainSize(1024), memory, point_predicate);
  }
  if (selection_domain == bke::AttrDomain::Curve) {
    return IndexMask::from_predicate(mask.slice_content(curves.curves_range()),
                                     GrainSize(512),
                                     memory,
                                     [&](const int curve) -> bool {
                                       const IndexRange points = points_by_curve[curve];
                                       const bool is_cyclic = cyclic[curve];

                                       /* Single-point curve can still be selected in curve mode.
                                        */
                                       if (points.size() == 1) {
                                         return point_predicate(points.first());
                                       }

                                       for (const int point : points.drop_back(1)) {
                                         if (line_predicate(curve, point, point + 1)) {
                                           return true;
                                         }
                                       }
                                       if (is_cyclic) {
                                         if (line_predicate(curve, points.last(), points.first()))
                                         {
                                           return true;
                                         }
                                       }
                                       return false;
                                     });
  }
  return {};
}

IndexMask select_adjacent_mask(const bke::CurvesGeometry &curves,
                               const IndexMask &curves_mask,
                               const StringRef attribute_name,
                               const bool deselect,
                               IndexMaskMemory &memory)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  VArraySpan<bool> selection = *curves.attributes().lookup_or_default<bool>(
      attribute_name, bke::AttrDomain::Point, true);

  /* Mask of points that are not selected yet but adjacent. */
  Array<bool> changed_points(curves.points_num());

  auto is_point_changed1 = [&](const int point, const int neighbor) {
    return deselect ? (selection[point] && !selection[neighbor]) :
                      (!selection[point] && selection[neighbor]);
  };
  auto is_point_changed2 = [&](const int point, const int neighbor1, const int neighbor2) {
    return deselect ? (selection[point] && (!selection[neighbor1] || !selection[neighbor2])) :
                      (!selection[point] && (selection[neighbor1] || selection[neighbor2]));
  };

  curves_mask.foreach_index([&](const int64_t curve) {
    const IndexRange points = points_by_curve[curve];
    if (points.size() == 1) {
      /* Single point curve does not add anything to the mask. */
      return;
    }

    if (cyclic[curve]) {
      changed_points[points.first()] = is_point_changed2(
          points.first(), points.last(), points.first() + 1);
      for (const int point : points.drop_front(1).drop_back(1)) {
        changed_points[point] = is_point_changed2(point, point - 1, point + 1);
      }
      changed_points[points.last()] = is_point_changed2(
          points.last(), points.last() - 1, points.first());
    }
    else {
      changed_points[points.first()] = is_point_changed1(points.first(), points.first() + 1);
      for (const int point : points.drop_front(1).drop_back(1)) {
        changed_points[point] = is_point_changed2(point, point - 1, point + 1);
      }
      changed_points[points.last()] = is_point_changed1(points.last(), points.last() - 1);
    }
  });

  return IndexMask::from_bools(changed_points, memory);
}

IndexMask select_adjacent_mask(const bke::CurvesGeometry &curves,
                               const StringRef attribute_name,
                               const bool deselect,
                               IndexMaskMemory &memory)
{
  return select_adjacent_mask(curves, curves.curves_range(), attribute_name, deselect, memory);
}

IndexMask select_box_mask(const ViewContext &vc,
                          const bke::CurvesGeometry &curves,
                          const bke::crazyspace::GeometryDeformation &deformation,
                          const float4x4 &projection,
                          const IndexMask &selection_mask,
                          const IndexMask &bezier_mask,
                          const bke::AttrDomain selection_domain,
                          const StringRef attribute_name,
                          const rcti &rect,
                          IndexMaskMemory &memory)
{
  const std::optional<Span<float3>> positions_opt = get_selection_attribute_positions(
      curves, deformation, attribute_name);
  if (!positions_opt) {
    return {};
  }
  const Span<float3> positions = *positions_opt;

  auto point_predicate = [&](const int point) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    /* Check the lasso bounding box first as an optimization. */
    return BLI_rcti_isect_pt_v(&rect, int2(pos_proj));
  };
  auto line_predicate = [&](const int /*curve*/, const int point, const int next_point_i) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    const float2 next_pos_proj = ED_view3d_project_float_v2_m4(
        vc.region, positions[next_point_i], projection);
    return BLI_rcti_isect_segment(&rect, int2(pos_proj), int2(next_pos_proj));
  };

  const IndexMask &mask = (selection_domain != bke::AttrDomain::Point ||
                           attribute_name == ".selection") ?
                              selection_mask :
                              bezier_mask;
  return select_mask_from_predicates(
      curves, mask, selection_domain, memory, point_predicate, line_predicate);
}

IndexMask select_lasso_mask(const ViewContext &vc,
                            const bke::CurvesGeometry &curves,
                            const bke::crazyspace::GeometryDeformation &deformation,
                            const float4x4 &projection,
                            const IndexMask &selection_mask,
                            const IndexMask &bezier_mask,
                            const bke::AttrDomain selection_domain,
                            const StringRef attribute_name,
                            const Span<int2> lasso_coords,
                            IndexMaskMemory &memory)
{
  rcti bbox;
  BLI_lasso_boundbox(&bbox, lasso_coords);
  const std::optional<Span<float3>> positions_opt = get_selection_attribute_positions(
      curves, deformation, attribute_name);
  if (!positions_opt) {
    return {};
  }
  const Span<float3> positions = *positions_opt;

  auto point_predicate = [&](const int point) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    /* Check the lasso bounding box first as an optimization. */
    return BLI_rcti_isect_pt_v(&bbox, int2(pos_proj)) &&
           BLI_lasso_is_point_inside(lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED);
  };
  auto line_predicate = [&](const int /*curve*/, const int point, const int next_point_i) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    const float2 next_pos_proj = ED_view3d_project_float_v2_m4(
        vc.region, positions[next_point_i], projection);
    return BLI_rcti_isect_segment(&bbox, int2(pos_proj), int2(next_pos_proj)) &&
           BLI_lasso_is_edge_inside(lasso_coords,
                                    int(pos_proj.x),
                                    int(pos_proj.y),
                                    int(next_pos_proj.x),
                                    int(next_pos_proj.y),
                                    IS_CLIPPED);
  };

  const IndexMask &mask = (selection_domain != bke::AttrDomain::Point ||
                           attribute_name == ".selection") ?
                              selection_mask :
                              bezier_mask;
  return select_mask_from_predicates(
      curves, mask, selection_domain, memory, point_predicate, line_predicate);
}

IndexMask select_circle_mask(const ViewContext &vc,
                             const bke::CurvesGeometry &curves,
                             const bke::crazyspace::GeometryDeformation &deformation,
                             const float4x4 &projection,
                             const IndexMask &selection_mask,
                             const IndexMask &bezier_mask,
                             const bke::AttrDomain selection_domain,
                             const StringRef attribute_name,
                             const int2 coord,
                             const float radius,
                             IndexMaskMemory &memory)
{
  const float radius_sq = pow2f(radius);
  const std::optional<Span<float3>> positions_opt = get_selection_attribute_positions(
      curves, deformation, attribute_name);
  if (!positions_opt) {
    return {};
  }
  const Span<float3> positions = *positions_opt;

  auto point_predicate = [&](const int point) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    const float distance_proj_sq = math::distance_squared(pos_proj, float2(coord));
    return distance_proj_sq <= radius_sq;
  };
  auto line_predicate = [&](const int /*curve*/, const int point, const int next_point_i) {
    const float2 pos_proj = ED_view3d_project_float_v2_m4(vc.region, positions[point], projection);
    const float2 next_pos_proj = ED_view3d_project_float_v2_m4(
        vc.region, positions[next_point_i], projection);
    const float distance_proj_sq = dist_squared_to_line_segment_v2(
        float2(coord), pos_proj, next_pos_proj);
    return distance_proj_sq <= radius_sq;
  };

  const IndexMask &mask = (selection_domain != bke::AttrDomain::Point ||
                           attribute_name == ".selection") ?
                              selection_mask :
                              bezier_mask;
  return select_mask_from_predicates(
      curves, mask, selection_domain, memory, point_predicate, line_predicate);
}

}  // namespace blender::ed::curves
