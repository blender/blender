/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Dilate or erode the given input using a morphological operator with a circular structuring
 * element of radius equivalent to the absolute value of the given distance parameter. A positive
 * distance corresponds to dilate operator, while a negative distance corresponds to an erode
 * operator. */
void morphological_distance(Context &context, Result &input, Result &output, int distance);

}  // namespace blender::realtime_compositor
