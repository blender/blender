/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

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

void CurveBaseOperation::set_curve_mapping(CurveMapping *mapping)
{
  /* duplicate the curve to avoid glitches while drawing, see bug T32374. */
  if (curve_mapping_) {
    BKE_curvemapping_free(curve_mapping_);
  }
  curve_mapping_ = BKE_curvemapping_copy(mapping);
}

}  // namespace blender::compositor
