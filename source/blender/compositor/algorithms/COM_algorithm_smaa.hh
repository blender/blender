/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Anti-alias the given input using the SMAA algorithm and write the result into the given output.
 * See the SMAA_THRESHOLD, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR, and SMAA_CORNER_ROUNDING defines
 * in the implementation for information on the parameters. */
void smaa(Context &context,
          Result &input,
          Result &output,
          const float threshold = 0.1f,
          const float local_contrast_adaptation_factor = 2.0f,
          const int corner_rounding = 25);

}  // namespace blender::compositor
