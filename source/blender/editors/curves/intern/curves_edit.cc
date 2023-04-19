/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_index_mask_ops.hh"

#include "BKE_curves.hh"

#include "ED_curves.h"

namespace blender::ed::curves {

bool remove_selection(bke::CurvesGeometry &curves, const eAttrDomain selection_domain)
{
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", selection_domain, true);
  const int domain_size_orig = attributes.domain_size(selection_domain);
  Vector<int64_t> indices;
  const IndexMask mask = index_mask_ops::find_indices_from_virtual_array(
      selection.index_range(), selection, 4096, indices);
  switch (selection_domain) {
    case ATTR_DOMAIN_POINT:
      curves.remove_points(mask);
      break;
    case ATTR_DOMAIN_CURVE:
      curves.remove_curves(mask);
      break;
    default:
      BLI_assert_unreachable();
  }

  return attributes.domain_size(selection_domain) != domain_size_orig;
}

}  // namespace blender::ed::curves
