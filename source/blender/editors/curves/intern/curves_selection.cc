/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_rand.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "ED_curves.h"
#include "ED_object.h"
#include "ED_select_utils.h"

namespace blender::ed::curves {

static IndexMask retrieve_selected_curves(const bke::CurvesGeometry &curves,
                                          Vector<int64_t> &r_indices)
{
  const IndexRange curves_range = curves.curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Interpolate from points to curves manually as a performance improvement, since we are only
   * interested in whether any point in each curve is selected. Retrieve meta data since
   * #lookup_or_default from the attribute API doesn't give the domain of the attribute. */
  std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(".selection");
  if (meta_data && meta_data->domain == ATTR_DOMAIN_POINT) {
    /* Avoid the interpolation from interpolating the attribute to the
     * curve domain by retrieving the point domain values directly. */
    const VArray<bool> selection = attributes.lookup_or_default<bool>(
        ".selection", ATTR_DOMAIN_POINT, true);
    if (selection.is_single()) {
      return selection.get_internal_single() ? IndexMask(curves_range) : IndexMask();
    }
    const OffsetIndices points_by_curve = curves.points_by_curve();
    return index_mask_ops::find_indices_based_on_predicate(
        curves_range, 512, r_indices, [&](const int64_t curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          /* The curve is selected if any of its points are selected. */
          Array<bool, 32> point_selection(points.size());
          selection.materialize_compressed(points, point_selection);
          return point_selection.as_span().contains(true);
        });
  }
  const VArray<bool> selection = attributes.lookup_or_default<bool>(
      ".selection", ATTR_DOMAIN_CURVE, true);
  return index_mask_ops::find_indices_from_virtual_array(curves_range, selection, 2048, r_indices);
}

IndexMask retrieve_selected_curves(const Curves &curves_id, Vector<int64_t> &r_indices)
{
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
  return retrieve_selected_curves(curves, r_indices);
}

IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves, Vector<int64_t> &r_indices)
{
  return index_mask_ops::find_indices_from_virtual_array(
      curves.points_range(),
      curves.attributes().lookup_or_default<bool>(".selection", ATTR_DOMAIN_POINT, true),
      2048,
      r_indices);
}

IndexMask retrieve_selected_points(const Curves &curves_id, Vector<int64_t> &r_indices)
{
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
  return retrieve_selected_points(curves, r_indices);
}

bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     const eAttrDomain selection_domain,
                                                     const eCustomDataType create_type)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (attributes.contains(".selection")) {
    return attributes.lookup_for_write_span(".selection");
  }
  const int domain_size = attributes.domain_size(selection_domain);
  switch (create_type) {
    case CD_PROP_BOOL:
      attributes.add(".selection",
                     selection_domain,
                     CD_PROP_BOOL,
                     bke::AttributeInitVArray(VArray<bool>::ForSingle(true, domain_size)));
      break;
    case CD_PROP_FLOAT:
      attributes.add(".selection",
                     selection_domain,
                     CD_PROP_FLOAT,
                     bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, domain_size)));
      break;
    default:
      BLI_assert_unreachable();
  }
  return attributes.lookup_for_write_span(".selection");
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

static bool contains(const VArray<bool> &varray, const bool value)
{
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) == value;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        span.index_range(),
        4096,
        false,
        [&](const IndexRange range, const bool init) {
          return init || span.slice(range).contains(value);
        },
        [&](const bool a, const bool b) { return a || b; });
  }
  return threading::parallel_reduce(
      varray.index_range(),
      2048,
      false,
      [&](const IndexRange range, const bool init) {
        if (init) {
          return init;
        }
        /* Alternatively, this could use #materialize to retrieve many values at once. */
        for (const int64_t i : range) {
          if (varray[i] == value) {
            return true;
          }
        }
        return false;
      },
      [&](const bool a, const bool b) { return a || b; });
}

bool has_anything_selected(const bke::CurvesGeometry &curves)
{
  const VArray<bool> selection = curves.attributes().lookup<bool>(".selection");
  return !selection || contains(selection, true);
}

static void invert_selection(MutableSpan<float> selection)
{
  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      selection[i] = 1.0f - selection[i];
    }
  });
}

static void invert_selection(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    array_utils::invert_booleans(selection.typed<bool>());
  }
  else if (selection.type().is<float>()) {
    invert_selection(selection.typed<float>());
  }
}

void select_all(bke::CurvesGeometry &curves, const eAttrDomain selection_domain, int action)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (action == SEL_SELECT) {
    /* As an optimization, just remove the selection attributes when everything is selected. */
    attributes.remove(".selection");
  }
  else {
    bke::GSpanAttributeWriter selection = ensure_selection_attribute(
        curves, selection_domain, CD_PROP_BOOL);
    if (action == SEL_DESELECT) {
      fill_selection_false(selection.span);
    }
    else if (action == SEL_INVERT) {
      invert_selection(selection.span);
    }
    selection.finish();
  }
}

void select_ends(bke::CurvesGeometry &curves,
                 const eAttrDomain selection_domain,
                 int amount,
                 bool end_points)
{
  const bool was_anything_selected = has_anything_selected(curves);
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);
  if (!was_anything_selected) {
    fill_selection_true(selection.span);
  }
  selection.span.type().to_static_type_tag<bool, float>([&](auto type_tag) {
    using T = typename decltype(type_tag)::type;
    if constexpr (std::is_void_v<T>) {
      BLI_assert_unreachable();
    }
    else {
      MutableSpan<T> selection_typed = selection.span.typed<T>();
      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
        for (const int curve_i : range) {
          const OffsetIndices points_by_curve = curves.points_by_curve();
          if (end_points) {
            selection_typed.slice(points_by_curve[curve_i].drop_back(amount)).fill(T(0));
          }
          else {
            selection_typed.slice(points_by_curve[curve_i].drop_front(amount)).fill(T(0));
          }
        }
      });
    }
  });
  selection.finish();
}

void select_random(bke::CurvesGeometry &curves,
                   const eAttrDomain selection_domain,
                   uint32_t random_seed,
                   float probability)
{
  RandomNumberGenerator rng{random_seed};
  const auto next_bool_random_value = [&]() { return rng.get_float() <= probability; };

  const bool was_anything_selected = has_anything_selected(curves);
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);
  if (!was_anything_selected) {
    curves::fill_selection_true(selection.span);
  }
  selection.span.type().to_static_type_tag<bool, float>([&](auto type_tag) {
    using T = typename decltype(type_tag)::type;
    if constexpr (std::is_void_v<T>) {
      BLI_assert_unreachable();
    }
    else {
      MutableSpan<T> selection_typed = selection.span.typed<T>();
      switch (selection_domain) {
        case ATTR_DOMAIN_POINT: {
          for (const int point_i : selection_typed.index_range()) {
            const bool random_value = next_bool_random_value();
            if (!random_value) {
              selection_typed[point_i] = T(0);
            }
          }

          break;
        }
        case ATTR_DOMAIN_CURVE: {
          for (const int curve_i : curves.curves_range()) {
            const bool random_value = next_bool_random_value();
            if (!random_value) {
              selection_typed[curve_i] = T(0);
            }
          }
          break;
        }
        default:
          BLI_assert_unreachable();
      }
    }
  });
  selection.finish();
}

}  // namespace blender::ed::curves
