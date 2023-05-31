/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DifferenceMatteOperation.h"

namespace blender::compositor {

DifferenceMatteOperation::DifferenceMatteOperation()
{
  add_input_socket(DataType::Color);
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  input_image1_program_ = nullptr;
  input_image2_program_ = nullptr;
  flags_.can_be_constant = true;
}

void DifferenceMatteOperation::init_execution()
{
  input_image1_program_ = this->get_input_socket_reader(0);
  input_image2_program_ = this->get_input_socket_reader(1);
}
void DifferenceMatteOperation::deinit_execution()
{
  input_image1_program_ = nullptr;
  input_image2_program_ = nullptr;
}

void DifferenceMatteOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float in_color1[4];
  float in_color2[4];

  const float tolerance = settings_->t1;
  const float falloff = settings_->t2;
  float difference;
  float alpha;

  input_image1_program_->read_sampled(in_color1, x, y, sampler);
  input_image2_program_->read_sampled(in_color2, x, y, sampler);

  difference = (fabsf(in_color2[0] - in_color1[0]) + fabsf(in_color2[1] - in_color1[1]) +
                fabsf(in_color2[2] - in_color1[2]));

  /* average together the distances */
  difference = difference / 3.0f;

  /* make 100% transparent */
  if (difference <= tolerance) {
    output[0] = 0.0f;
  }
  /* In the falloff region, make partially transparent. */
  else if (difference <= falloff + tolerance) {
    difference = difference - tolerance;
    alpha = difference / falloff;
    /* Only change if more transparent than before. */
    if (alpha < in_color1[3]) {
      output[0] = alpha;
    }
    else { /* leave as before */
      output[0] = in_color1[3];
    }
  }
  else {
    /* foreground object */
    output[0] = in_color1[3];
  }
}

void DifferenceMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color1 = it.in(0);
    const float *color2 = it.in(1);

    float difference = (fabsf(color2[0] - color1[0]) + fabsf(color2[1] - color1[1]) +
                        fabsf(color2[2] - color1[2]));

    /* Average together the distances. */
    difference = difference / 3.0f;

    const float tolerance = settings_->t1;
    const float falloff = settings_->t2;

    /* Make 100% transparent. */
    if (difference <= tolerance) {
      it.out[0] = 0.0f;
    }
    /* In the falloff region, make partially transparent. */
    else if (difference <= falloff + tolerance) {
      difference = difference - tolerance;
      const float alpha = difference / falloff;
      /* Only change if more transparent than before. */
      if (alpha < color1[3]) {
        it.out[0] = alpha;
      }
      else { /* Leave as before. */
        it.out[0] = color1[3];
      }
    }
    else {
      /* Foreground object. */
      it.out[0] = color1[3];
    }
  }
}

}  // namespace blender::compositor
