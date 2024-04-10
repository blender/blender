/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Transforms the given result based on the given transformation and realization options, writing
 * the transformed result to the given output.
 *
 * The rotation and scale components of the transformation are realized and the size of the result
 * is increased/reduced to adapt to the new transformation. For instance, if the transformation is
 * a rotation, the input will be rotated and expanded in size to account for the bounding box of
 * the input after rotation. The size of the returned result is bound and clipped by the maximum
 * possible GPU texture size to avoid allocations that surpass hardware limits, which is typically
 * 16k.
 *
 * The translation component of the transformation is delayed and only stored in the domain of the
 * result to be realized later when needed, except if the realization options has wrapping enabled,
 * in which case, the result will be translated such that it is clipped on the one side and wrapped
 * on the opposite side.
 *
 * The empty areas around the image after rotation will either be transparent or repetitions of the
 * image based on the realization options. */
void transform(Context &context,
               Result &input,
               Result &output,
               const float3x3 &transformation,
               RealizationOptions realization_options);

}  // namespace blender::realtime_compositor
