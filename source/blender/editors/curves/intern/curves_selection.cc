/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_index_mask_ops.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "ED_curves.h"
#include "ED_object.h"

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
    return index_mask_ops::find_indices_based_on_predicate(
        curves_range, 512, r_indices, [&](const int64_t curve_i) {
          const IndexRange points = curves.points_for_curve(curve_i);
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

static IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves,
                                          Vector<int64_t> &r_indices)
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

void ensure_selection_attribute(Curves &curves_id, const eCustomDataType create_type)
{
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (attributes.contains(".selection")) {
    return;
  }
  const eAttrDomain domain = eAttrDomain(curves_id.selection_domain);
  const int domain_size = attributes.domain_size(domain);
  switch (create_type) {
    case CD_PROP_BOOL:
      attributes.add(".selection",
                     domain,
                     CD_PROP_BOOL,
                     bke::AttributeInitVArray(VArray<bool>::ForSingle(true, domain_size)));
      break;
    case CD_PROP_FLOAT:
      attributes.add(".selection",
                     domain,
                     CD_PROP_FLOAT,
                     bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, domain_size)));
      break;
    default:
      BLI_assert_unreachable();
  }
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

}  // namespace blender::ed::curves
