/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Extracts the alpha channel from the given input and write it to the given output. The output
 * will be allocated internally and is thus expected not to be previously allocated. */
void extract_alpha(Context &context, Result &input, Result &output);

}  // namespace blender::realtime_compositor
