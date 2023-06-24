/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DistanceRGBMatteOperation.h"

namespace blender::compositor {

DistanceRGBMatteOperation::DistanceRGBMatteOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);

  input_image_program_ = nullptr;
  input_key_program_ = nullptr;
  flags_.can_be_constant = true;
}

void DistanceRGBMatteOperation::init_execution()
{
  input_image_program_ = this->get_input_socket_reader(0);
  input_key_program_ = this->get_input_socket_reader(1);
}

void DistanceRGBMatteOperation::deinit_execution()
{
  input_image_program_ = nullptr;
  input_key_program_ = nullptr;
}

float DistanceRGBMatteOperation::calculate_distance(const float key[4], const float image[4])
{
  return len_v3v3(key, image);
}

void DistanceRGBMatteOperation::execute_pixel_sampled(float output[4],
                                                      float x,
                                                      float y,
                                                      PixelSampler sampler)
{
  float in_key[4];
  float in_image[4];

  const float tolerance = settings_->t1;
  const float falloff = settings_->t2;

  float distance;
  float alpha;

  input_key_program_->read_sampled(in_key, x, y, sampler);
  input_image_program_->read_sampled(in_image, x, y, sampler);

  distance = this->calculate_distance(in_key, in_image);

  /* Store matte(alpha) value in [0] to go with
   * COM_SetAlphaMultiplyOperation and the Value output.
   */

  /* Make 100% transparent. */
  if (distance < tolerance) {
    output[0] = 0.0f;
  }
  /* In the falloff region, make partially transparent. */
  else if (distance < falloff + tolerance) {
    distance = distance - tolerance;
    alpha = distance / falloff;
    /* Only change if more transparent than before. */
    if (alpha < in_image[3]) {
      output[0] = alpha;
    }
    else { /* leave as before */
      output[0] = in_image[3];
    }
  }
  else {
    /* leave as before */
    output[0] = in_image[3];
  }
}

void DistanceRGBMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_image = it.in(0);
    const float *in_key = it.in(1);

    float distance = this->calculate_distance(in_key, in_image);
    const float tolerance = settings_->t1;
    const float falloff = settings_->t2;

    /* Store matte(alpha) value in [0] to go with
     * COM_SetAlphaMultiplyOperation and the Value output.
     */

    /* Make 100% transparent. */
    if (distance < tolerance) {
      it.out[0] = 0.0f;
    }
    /* In the falloff region, make partially transparent. */
    else if (distance < falloff + tolerance) {
      distance = distance - tolerance;
      const float alpha = distance / falloff;
      /* Only change if more transparent than before. */
      if (alpha < in_image[3]) {
        it.out[0] = alpha;
      }
      else { /* Leave as before. */
        it.out[0] = in_image[3];
      }
    }
    else {
      /* Leave as before. */
      it.out[0] = in_image[3];
    }
  }
}

}  // namespace blender::compositor
