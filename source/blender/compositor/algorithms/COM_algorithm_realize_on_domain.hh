/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Projects the input on a target domain, copies the area of the input that intersects the target
 * domain, and fill the rest with zeros or repetitions of the input depending on the realization
 * options. The transformation and realization options of the input are ignored and the given
 * input_transformation and realization_options are used instead to allow the caller to change them
 * without mutating the input result directly. See the discussion in COM_domain.hh for more
 * information on what realization on domain means. */
void realize_on_domain(Context &context,
                       Result &input,
                       Result &output,
                       const Domain &domain,
                       const float3x3 &input_transformation,
                       const RealizationOptions &realization_options);

}  // namespace blender::realtime_compositor
