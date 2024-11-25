/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Possible morphological operations to apply. */
enum class MorphologicalBlurOperation : uint8_t {
  /* Dilate by taking the maximum from the original input and the blurred input. Which means the
   * whites bleeds into the blacks while the blacks don't bleed into the whites. */
  Dilate,
  /* Erode by taking the minimum from the original input and the blurred input. Which means the
   * blacks bleeds into the whites while the whites don't bleed into the blacks. */
  Erode,
};

/* Applies a morphological blur on input using the given radius and filter type. This essentially
 * applies a standard blur operation, but then takes the maximum or minimum from the original input
 * and blurred input depending on the chosen operation, see the MorphologicalBlurOperation enum for
 * more information. The output is written to the given output result, which will be allocated
 * internally and is thus expected not to be previously allocated. */
void morphological_blur(
    Context &context,
    const Result &input,
    Result &output,
    const float2 &radius,
    const MorphologicalBlurOperation operation = MorphologicalBlurOperation::Erode,
    const int filter_type = R_FILTER_GAUSS);

}  // namespace blender::realtime_compositor
