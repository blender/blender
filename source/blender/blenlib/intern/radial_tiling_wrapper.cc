/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_radial_tiling.hh" /* Own include (forward declare). */

namespace blender {

/* Define macro flags for code adaption. */
#define ADAPT_TO_GEOMETRY_NODES

/* The rounded polygon calculation functions are defined in radial_tiling_shared.hh. */
#include "radial_tiling_shared.hh"

/* Undefine macro flags used for code adaption. */
#undef ADAPT_TO_GEOMETRY_NODES

}  // namespace blender
