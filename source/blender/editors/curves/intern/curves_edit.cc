/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BKE_curves.hh"

#include "ED_curves.hh"

namespace blender::ed::curves {

bool remove_selection(bke::CurvesGeometry &curves, const eAttrDomain selection_domain)
{
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", selection_domain, true);
  const int domain_size_orig = attributes.domain_size(selection_domain);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools(selection, memory);
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
