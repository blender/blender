/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

namespace blender::ocio {

/* Standard XYZ (D65) to linear Rec.709 transform. */
static const float3x3 XYZ_TO_REC709{{3.2404542f, -0.9692660f, 0.0556434f},
                                    {-1.5371385f, 1.8760108f, -0.2040259f},
                                    {-0.4985314f, 0.0415560f, 1.0572252f}};

/* Standard ACES to XYZ (D65) transform.
 * Matches OpenColorIO builtin transform: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD. */
static const float3x3 ACES_TO_XYZ = {{0.938280f, 0.337369f, 0.001174f},
                                     {-0.004451f, 0.729522f, -0.003711f},
                                     {0.016628f, -0.066890f, 1.091595f}};

}  // namespace blender::ocio
