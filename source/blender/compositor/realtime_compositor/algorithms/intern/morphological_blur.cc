/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
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
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void apply_morphological_operator_gpu(Context &context,
                                             const Result &input,
                                             Result &blurred_input,
                                             const MorphologicalBlurOperation operation)
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

static void apply_morphological_operator_cpu(const Result &input,
                                             Result &blurred_input,
                                             const MorphologicalBlurOperation operation)
{
  Domain domain = input.domain();
  switch (operation) {
    case MorphologicalBlurOperation::Dilate:
      parallel_for(domain.size, [&](const int2 texel) {
        float input_value = input.load_pixel(texel).x;
        float blurred_value = blurred_input.load_pixel(texel).x;
        blurred_input.store_pixel(texel, float4(math::max(input_value, blurred_value)));
      });
      break;
    case MorphologicalBlurOperation::Erode:
      parallel_for(domain.size, [&](const int2 texel) {
        float input_value = input.load_pixel(texel).x;
        float blurred_value = blurred_input.load_pixel(texel).x;
        blurred_input.store_pixel(texel, float4(math::min(input_value, blurred_value)));
      });
      break;
  }
}

/* Apply the morphological operator (minimum or maximum) on the input and the blurred input. The
 * output is written to the blurred_input in-place. */
static void apply_morphological_operator(Context &context,
                                         const Result &input,
                                         Result &blurred_input,
                                         const MorphologicalBlurOperation operation)
{
  if (context.use_gpu()) {
    apply_morphological_operator_gpu(context, input, blurred_input, operation);
  }
  else {
    apply_morphological_operator_cpu(input, blurred_input, operation);
  }
}

void morphological_blur(Context &context,
                        const Result &input,
                        Result &output,
                        const float2 &radius,
                        const MorphologicalBlurOperation operation,
                        const int filter_type)
{
  BLI_assert(input.type() == ResultType::Float);

  symmetric_separable_blur(context, input, output, radius, filter_type);
  apply_morphological_operator(context, input, output, operation);
}

}  // namespace blender::realtime_compositor
