/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MathBaseOperation.h"

#include "BLI_math_rotation.h"

namespace blender::compositor {

MathBaseOperation::MathBaseOperation()
{
  /* TODO(manzanilla): after removing tiled implementation, template this class to only add needed
   * number of inputs. */
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  use_clamp_ = false;
  flags_.can_be_constant = true;
}

void MathBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperationInput *socket;
  rcti temp_area = COM_AREA_NONE;
  socket = this->get_input_socket(0);
  const bool determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
  if (determined) {
    this->set_canvas_input_index(0);
  }
  else {
    this->set_canvas_input_index(1);
  }
  NodeOperation::determine_canvas(preferred_area, r_area);
}

void MathBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  BuffersIterator<float> it = output->iterate_with(inputs, area);
  update_memory_buffer_partial(it);
}

void MathDivideOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float divisor = *it.in(1);
    *it.out = clamp_when_enabled((divisor == 0) ? 0 : *it.in(0) / divisor);
  }
}

void MathSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sin(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cos(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sinh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cosh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tanh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? asin(value1) : 0.0f);
  }
}

void MathArcCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? acos(value1) : 0.0f);
  }
}

void MathArcTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathPowerOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 >= 0) {
      *it.out = pow(value1, value2);
    }
    else {
      const float y_mod_1 = fmod(value2, 1);
      /* If input value is not nearly an integer, fall back to zero, nicer than straight rounding.
       */
      if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
        *it.out = pow(value1, floorf(value2 + 0.5f));
      }
      else {
        *it.out = 0.0f;
      }
    }
    clamp_when_enabled(it.out);
  }
}

void MathLogarithmOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 > 0 && value2 > 0) {
      *it.out = log(value1) / log(value2);
    }
    else {
      *it.out = 0.0;
    }
    clamp_when_enabled(it.out);
  }
}

void MathMinimumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = std::min(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathMaximumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = std::max(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathRoundOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = round(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathModuloOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value2 = *it.in(1);
    *it.out = (value2 == 0) ? 0 : fmod(*it.in(0), value2);
    clamp_when_enabled(it.out);
  }
}

void MathFlooredModuloOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value2 = *it.in(1);
    *it.out = (value2 == 0) ? 0 : *it.in(0) - floorf(*it.in(0) / value2) * value2;
    clamp_when_enabled(it.out);
  }
}

void MathAbsoluteOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = fabs(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathRadiansOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = DEG2RADF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathDegreesOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = RAD2DEGF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcTan2Operation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathFloorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = floor(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCeilOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = ceil(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathFractOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value - floor(value));
  }
}

void MathSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? sqrt(value) : 0.0f);
  }
}

void MathInverseSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? 1.0f / sqrt(value) : 0.0f);
  }
}

void MathSignOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = compatible_signf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathExponentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = expf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTruncOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = (value >= 0.0f) ? floor(value) : ceil(value);
    clamp_when_enabled(it.out);
  }
}

void MathSnapOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 == 0 || value2 == 0) { /* Avoid dividing by zero. */
      *it.out = 0.0f;
    }
    else {
      *it.out = floorf(value1 / value2) * value2;
    }
    clamp_when_enabled(it.out);
  }
}

void MathWrapOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = wrapf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathPingpongOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = pingpongf(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathCompareOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = (fabsf(*it.in(0) - *it.in(1)) <= std::max(*it.in(2), 1e-5f)) ? 1.0f : 0.0f;
    clamp_when_enabled(it.out);
  }
}

void MathMultiplyAddOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = it.in(0)[0] * it.in(1)[0] + it.in(2)[0];
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMinOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = smoothminf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMaxOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = -smoothminf(-it.in(0)[0], -it.in(1)[0], it.in(2)[0]);
    clamp_when_enabled(it.out);
  }
}

}  // namespace blender::compositor
