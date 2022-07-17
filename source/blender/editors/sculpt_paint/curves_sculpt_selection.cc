/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask_ops.hh"

#include "BKE_curves.hh"

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

static VArray<float> get_curves_selection(const CurvesGeometry &curves, const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_CURVE:
      return curves.selection_curve_float();
    case ATTR_DOMAIN_POINT:
      return curves.adapt_domain(
          curves.selection_point_float(), ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE);
    default:
      BLI_assert_unreachable();
      return {};
  }
}

VArray<float> get_curves_selection(const Curves &curves_id)
{
  if (!(curves_id.flag & CV_SCULPT_SELECTION_ENABLED)) {
    return VArray<float>::ForSingle(1.0f, CurvesGeometry::wrap(curves_id.geometry).curves_num());
  }
  return get_curves_selection(CurvesGeometry::wrap(curves_id.geometry),
                              eAttrDomain(curves_id.selection_domain));
}

static VArray<float> get_point_selection(const CurvesGeometry &curves, const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_CURVE:
      return curves.adapt_domain(
          curves.selection_curve_float(), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT);
    case ATTR_DOMAIN_POINT:
      return curves.selection_point_float();
    default:
      BLI_assert_unreachable();
      return {};
  }
}

VArray<float> get_point_selection(const Curves &curves_id)
{
  if (!(curves_id.flag & CV_SCULPT_SELECTION_ENABLED)) {
    return VArray<float>::ForSingle(1.0f, CurvesGeometry::wrap(curves_id.geometry).points_num());
  }
  return get_point_selection(CurvesGeometry::wrap(curves_id.geometry),
                             eAttrDomain(curves_id.selection_domain));
}

static IndexMask retrieve_selected_curves(const CurvesGeometry &curves,
                                          const eAttrDomain domain,
                                          Vector<int64_t> &r_indices)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT: {
      const VArray<float> selection = curves.selection_point_float();
      if (selection.is_single()) {
        return selection.get_internal_single() == 0.0f ? IndexMask(0) :
                                                         IndexMask(curves.curves_num());
      }
      return index_mask_ops::find_indices_based_on_predicate(
          curves.curves_range(), 512, r_indices, [&](const int curve_i) {
            for (const int i : curves.points_for_curve(curve_i)) {
              if (selection[i] > 0.0f) {
                return true;
              }
            }
            return false;
          });
    }
    case ATTR_DOMAIN_CURVE: {
      const VArray<float> selection = curves.selection_curve_float();
      if (selection.is_single()) {
        return selection.get_internal_single() == 0.0f ? IndexMask(0) :
                                                         IndexMask(curves.curves_num());
      }
      return index_mask_ops::find_indices_based_on_predicate(
          curves.curves_range(), 2048, r_indices, [&](const int i) {
            return selection[i] > 0.0f;
          });
    }
    default:
      BLI_assert_unreachable();
      return {};
  }
}

IndexMask retrieve_selected_curves(const Curves &curves_id, Vector<int64_t> &r_indices)
{
  if (!(curves_id.flag & CV_SCULPT_SELECTION_ENABLED)) {
    return CurvesGeometry::wrap(curves_id.geometry).curves_range();
  }
  return retrieve_selected_curves(CurvesGeometry::wrap(curves_id.geometry),
                                  eAttrDomain(curves_id.selection_domain),
                                  r_indices);
}

}  // namespace blender::ed::sculpt_paint
