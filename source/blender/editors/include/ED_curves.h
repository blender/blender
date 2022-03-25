/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ED_operatortypes_curves(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BKE_curves.hh"

namespace blender::ed::curves {

bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);

}
#endif
