/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareThresholdOperation.h"

#include "IMB_colormanagement.h"

namespace blender::compositor {

GlareThresholdOperation::GlareThresholdOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::FitAny);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
}

void GlareThresholdOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  const int width = BLI_rcti_size_x(&r_area) / (1 << settings_->quality);
  const int height = BLI_rcti_size_y(&r_area) / (1 << settings_->quality);
  r_area.xmax = r_area.xmin + width;
  r_area.ymax = r_area.ymin + height;
}

void GlareThresholdOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void GlareThresholdOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  const float threshold = settings_->threshold;

  input_program_->read_sampled(output, x, y, sampler);
  if (IMB_colormanagement_get_luminance(output) >= threshold) {
    output[0] -= threshold;
    output[1] -= threshold;
    output[2] -= threshold;

    output[0] = MAX2(output[0], 0.0f);
    output[1] = MAX2(output[1], 0.0f);
    output[2] = MAX2(output[2], 0.0f);
  }
  else {
    zero_v3(output);
  }
}

void GlareThresholdOperation::deinit_execution()
{
  input_program_ = nullptr;
}

void GlareThresholdOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const float threshold = settings_->threshold;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    if (IMB_colormanagement_get_luminance(color) >= threshold) {
      it.out[0] = color[0] - threshold;
      it.out[1] = color[1] - threshold;
      it.out[2] = color[2] - threshold;

      CLAMP3_MIN(it.out, 0.0f);
    }
    else {
      zero_v3(it.out);
    }
  }
}

}  // namespace blender::compositor
