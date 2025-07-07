/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

namespace blender::ocio {

/* Standard XYZ (D65) to linear Rec.709 transform. */
/* Use four-digit constants instead of higher precisions to match sRGB and Rec.2020 standards.
 * See PR #141027 for details. */
static const float3x3 XYZ_TO_REC709{{3.2409699f, -0.9692436f, 0.0556301f},
                                    {-1.5373832f, 1.8759675f, -0.2039770f},
                                    {-0.4986108f, 0.0415551f, 1.0569715f}};

/* Standard ACES to XYZ (D65) transform.
 * Matches OpenColorIO builtin transform: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD. */
static const float3x3 ACES_TO_XYZ = {{0.938280f, 0.337369f, 0.001174f},
                                     {-0.004451f, 0.729522f, -0.003711f},
                                     {0.016628f, -0.066890f, 1.091595f}};

}  // namespace blender::ocio
