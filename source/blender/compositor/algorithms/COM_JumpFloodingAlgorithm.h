/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

/* Exact copies of the functions in gpu_shader_compositor_jump_flooding_lib.glsl and
 * COM_algorithm_jump_flooding.hh but adapted for CPU. See those files for more information. */

#define JUMP_FLOODING_NON_FLOODED_VALUE int2(-1)

namespace blender::compositor {

int2 encode_jump_flooding_value(int2 closest_seed_texel, bool is_flooded);

int2 initialize_jump_flooding_value(int2 texel, bool is_seed);

Array<int2> jump_flooding(Span<int2> input, int2 size);

}  // namespace blender::compositor
