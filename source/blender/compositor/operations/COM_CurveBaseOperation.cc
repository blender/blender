/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CurveBaseOperation.h"

#include "BKE_colortools.h"

namespace blender::compositor {

CurveBaseOperation::CurveBaseOperation()
{
  curve_mapping_ = nullptr;
  flags_.can_be_constant = true;
}

CurveBaseOperation::~CurveBaseOperation()
{
  if (curve_mapping_) {
    BKE_curvemapping_free(curve_mapping_);
    curve_mapping_ = nullptr;
  }
}

void CurveBaseOperation::init_execution()
{
  BKE_curvemapping_init(curve_mapping_);
}
void CurveBaseOperation::deinit_execution()
{
  if (curve_mapping_) {
    BKE_curvemapping_free(curve_mapping_);
    curve_mapping_ = nullptr;
  }
}

void CurveBaseOperation::set_curve_mapping(const CurveMapping *mapping)
{
  /* duplicate the curve to avoid glitches while drawing, see bug #32374. */
  if (curve_mapping_) {
    BKE_curvemapping_free(curve_mapping_);
  }
  curve_mapping_ = BKE_curvemapping_copy(mapping);
}

}  // namespace blender::compositor
