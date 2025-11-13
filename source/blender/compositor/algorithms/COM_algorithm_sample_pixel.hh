/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Samples a pixel from a color result. */
Color sample_pixel(Context &context,
                   const Result &input,
                   const Interpolation &interpolation,
                   const ExtensionMode &extension_mode_x,
                   const ExtensionMode &extension_mode_y,
                   const float2 coordinates);

}  // namespace blender::compositor
