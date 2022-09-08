/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Curves;

void ED_operatortypes_sculpt_curves(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_index_mask.hh"
#  include "BLI_vector.hh"

namespace blender::ed::sculpt_paint {

/**
 * Find curves that have any point selected (a selection factor greater than zero),
 * or curves that have their own selection factor greater than zero.
 */
IndexMask retrieve_selected_curves(const Curves &curves_id, Vector<int64_t> &r_indices);

/**
 * Find points that are selected (a selection factor greater than zero),
 * or points in curves with a selection factor greater than zero).
 */
IndexMask retrieve_selected_points(const Curves &curves_id, Vector<int64_t> &r_indices);

}  // namespace blender::ed::sculpt_paint

#endif
