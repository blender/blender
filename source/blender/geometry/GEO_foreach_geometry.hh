/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

namespace blender::geometry {

/**
 * Modify every real geometry separately, including those from instances. The input to the
 * callback never contains instances. Newly generated instances will be merged with the
 * previously existing ones.
 */
void foreach_real_geometry(bke::GeometrySet &geometry,
                           FunctionRef<void(bke::GeometrySet &geometry_set)> fn);

}  // namespace blender::geometry
