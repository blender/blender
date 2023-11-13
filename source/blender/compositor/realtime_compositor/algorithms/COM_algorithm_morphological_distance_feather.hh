/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_scene_types.h"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Dilate or erode the given input using a morphological inverse distance operation evaluated at
 * the given falloff. The radius of the structuring element is equivalent to the absolute value of
 * the given distance parameter. A positive distance corresponds to a dilate operator, while a
 * negative distance corresponds to an erode operator. See the implementation and shader for more
 * information. */
void morphological_distance_feather(
    Context &context, Result &input, Result &output, int distance, int falloff_type = PROP_SMOOTH);

}  // namespace blender::realtime_compositor
