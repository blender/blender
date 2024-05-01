/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DirectionalBlurOperation.h"

namespace blender::compositor {

DirectionalBlurOperation::DirectionalBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void DirectionalBlurOperation::init_execution()
{
  QualityStepHelper::init_execution(COM_QH_INCREASE);
  const float angle = data_->angle;
  const float zoom = data_->zoom;
  const float spin = data_->spin;
  const float iterations = data_->iter;
  const float distance = data_->distance;
  const float center_x = data_->center_x;
  const float center_y = data_->center_y;
  const float width = get_width();
  const float height = get_height();

  const float a = angle;
  const float itsc = 1.0f / powf(2.0f, float(iterations));
  float D;

  D = distance * sqrtf(width * width + height * height);
  center_x_pix_ = center_x * width;
  center_y_pix_ = center_y * height;

  tx_ = itsc * D * cosf(a);
  ty_ = -itsc * D * sinf(a);
  sc_ = itsc * zoom;
  rot_ = itsc * spin;
}

void DirectionalBlurOperation::get_area_of_interest(const int input_idx,
                                                    const rcti & /*output_area*/,
                                                    rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area = this->get_canvas();
}

void DirectionalBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const int iterations = pow(2.0f, data_->iter);
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    float color_accum[4];
    input->read_elem_bilinear(x, y, color_accum);

    /* Blur pixel. */
    /* TODO(manzanilla): Many values used on iterations can be calculated beforehand. Create a
     * table on operation initialization. */
    float ltx = tx_;
    float lty = ty_;
    float lsc = sc_;
    float lrot = rot_;
    for (int i = 0; i < iterations; i++) {
      const float cs = cosf(lrot), ss = sinf(lrot);
      const float isc = 1.0f / (1.0f + lsc);

      const float v = isc * (y + 0.5f - center_y_pix_) + lty;
      const float u = isc * (x + 0.5f - center_x_pix_) + ltx;

      float color[4];
      input->read_elem_bilinear(
          cs * u + ss * v + center_x_pix_ - 0.5f, cs * v - ss * u + center_y_pix_ - 0.5f, color);
      add_v4_v4(color_accum, color);

      /* Double transformations. */
      ltx += tx_;
      lty += ty_;
      lrot += rot_;
      lsc += sc_;
    }

    mul_v4_v4fl(it.out, color_accum, 1.0f / (iterations + 1));
  }
}

}  // namespace blender::compositor
