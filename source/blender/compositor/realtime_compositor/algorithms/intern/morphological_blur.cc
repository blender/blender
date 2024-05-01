/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_morphological_blur.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"

namespace blender::realtime_compositor {

static const char *get_shader(MorphologicalBlurOperation operation)
{
  switch (operation) {
    case MorphologicalBlurOperation::Dilate:
      return "compositor_morphological_blur_dilate";
    case MorphologicalBlurOperation::Erode:
      return "compositor_morphological_blur_erode";
    default:
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

/* Apply the morphological operator (minimum or maximum) on the input and the blurred input. The
 * output is written to the blurred_input in-place. */
static void apply_morphological_operator(Context &context,
                                         Result &input,
                                         Result &blurred_input,
                                         MorphologicalBlurOperation operation)
{
  GPUShader *shader = context.get_shader(get_shader(operation));
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  blurred_input.bind_as_image(shader, "blurred_input_img", true);

  Domain domain = input.domain();
  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  blurred_input.unbind_as_image();
}

void morphological_blur(Context &context,
                        Result &input,
                        Result &output,
                        float2 radius,
                        MorphologicalBlurOperation operation,
                        int filter_type)
{
  BLI_assert(input.type() == ResultType::Float);

  symmetric_separable_blur(context, input, output, radius, filter_type);
  apply_morphological_operator(context, input, output, operation);
}

}  // namespace blender::realtime_compositor
