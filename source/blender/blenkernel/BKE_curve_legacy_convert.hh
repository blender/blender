/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_curves_types.h"

struct Curve;
struct ListBase;

namespace blender::bke {

/**
 * Convert the old curve type to the new data type. Caller owns the returned pointer.
 */
Curves *curve_legacy_to_curves(const Curve &curve_legacy);
/**
 * Convert the old curve type to the new data type using a specific list of #Nurb for the actual
 * geometry data. Caller owns the returned pointer.
 */
Curves *curve_legacy_to_curves(const Curve &curve_legacy, const ListBase &nurbs_list);

/**
 * Determine Curves knot mode from legacy flag.
 */
KnotsMode knots_mode_from_legacy(short flag);

}  // namespace blender::bke
