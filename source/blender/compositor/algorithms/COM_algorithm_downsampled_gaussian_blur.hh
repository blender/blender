/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender::compositor {

class Context;
class Result;

void downsampled_gaussian_blur(Context &context,
                               const Result &input,
                               Result &output,
                               const float2 &radius,
                               bool extend_bounds = false);

}  // namespace blender::compositor
