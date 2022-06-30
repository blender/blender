/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

void ED_operatortypes_curves(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BKE_curves.hh"
#  include "BLI_vector_set.hh"

namespace blender::ed::curves {

bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);
bool selection_operator_poll(bContext *C);
bool has_anything_selected(const Curves &curves_id);
VectorSet<Curves *> get_unique_editable_curves(const bContext &C);

}  // namespace blender::ed::curves
#endif
