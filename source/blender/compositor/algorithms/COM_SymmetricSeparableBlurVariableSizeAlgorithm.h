/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_scene_types.h"

#include "COM_MemoryBuffer.h"

namespace blender::compositor {

/* Identical to the same function in COM_algorithm_symmetric_separable_blur_variable_size.hh, see
 * the function and its implementation for more details. */
void symmetric_separable_blur_variable_size(const MemoryBuffer &input,
                                            MemoryBuffer &output,
                                            const MemoryBuffer &radius,
                                            int filter_type = R_FILTER_GAUSS,
                                            int weights_resolution = 128);

}  // namespace blender::compositor
