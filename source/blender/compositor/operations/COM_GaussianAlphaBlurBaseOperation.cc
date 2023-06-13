/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GaussianAlphaBlurBaseOperation.h"

namespace blender::compositor {

GaussianAlphaBlurBaseOperation::GaussianAlphaBlurBaseOperation(eDimension dim)
    : BlurBaseOperation(DataType::Value)
{
  gausstab_ = nullptr;
  filtersize_ = 0;
  falloff_ = -1; /* Intentionally invalid, so we can detect uninitialized values. */
  dimension_ = dim;
}

void GaussianAlphaBlurBaseOperation::init_data()
{
  BlurBaseOperation::init_data();
  if (execution_model_ == eExecutionModel::FullFrame) {
    rad_ = max_ff(size_ * this->get_blur_size(dimension_), 0.0f);
    rad_ = min_ff(rad_, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad_), MAX_GAUSSTAB_RADIUS);
  }
}

void GaussianAlphaBlurBaseOperation::init_execution()
{
  BlurBaseOperation::init_execution();
  if (execution_model_ == eExecutionModel::FullFrame) {
    gausstab_ = BlurBaseOperation::make_gausstab(rad_, filtersize_);
    distbuf_inv_ = BlurBaseOperation::make_dist_fac_inverse(rad_, filtersize_, falloff_);
  }
}

void GaussianAlphaBlurBaseOperation::deinit_execution()
{
  BlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }

  if (distbuf_inv_) {
    MEM_freeN(distbuf_inv_);
    distbuf_inv_ = nullptr;
  }
}

void GaussianAlphaBlurBaseOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  if (input_idx != IMAGE_INPUT_INDEX) {
    BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
    return;
  }

  r_input_area = output_area;
  switch (dimension_) {
    case eDimension::X:
      r_input_area.xmin = output_area.xmin - filtersize_ - 1;
      r_input_area.xmax = output_area.xmax + filtersize_ + 1;
      break;
    case eDimension::Y:
      r_input_area.ymin = output_area.ymin - filtersize_ - 1;
      r_input_area.ymax = output_area.ymax + filtersize_ + 1;
      break;
  }
}

void GaussianAlphaBlurBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[IMAGE_INPUT_INDEX];
  const rcti &input_rect = input->get_rect();
  BuffersIterator<float> it = output->iterate_with({input}, area);

  int min_input_coord = -1;
  int max_input_coord = -1;
  int elem_stride = -1;
  std::function<int()> get_current_coord;
  switch (dimension_) {
    case eDimension::X:
      min_input_coord = input_rect.xmin;
      max_input_coord = input_rect.xmax;
      get_current_coord = [&] { return it.x; };
      elem_stride = input->elem_stride;
      break;
    case eDimension::Y:
      min_input_coord = input_rect.ymin;
      max_input_coord = input_rect.ymax;
      get_current_coord = [&] { return it.y; };
      elem_stride = input->row_stride;
      break;
  }

  for (; !it.is_end(); ++it) {
    const int coord = get_current_coord();
    const int coord_min = max_ii(coord - filtersize_, min_input_coord);
    const int coord_max = min_ii(coord + filtersize_ + 1, max_input_coord);

    /* *** This is the main part which is different to #GaussianBlurBaseOperation. *** */
    /* Gauss. */
    float alpha_accum = 0.0f;
    float multiplier_accum = 0.0f;

    /* Dilate. */
    const bool do_invert = do_subtract_;
    /* Init with the current color to avoid unneeded lookups. */
    float value_max = finv_test(*it.in(0), do_invert);
    float distfacinv_max = 1.0f; /* 0 to 1 */

    const int step = QualityStepHelper::get_step();
    const float *in = it.in(0) + (intptr_t(coord_min) - coord) * elem_stride;
    const int in_stride = elem_stride * step;
    int index = (coord_min - coord) + filtersize_;
    const int index_end = index + (coord_max - coord_min);
    for (; index < index_end; in += in_stride, index += step) {
      float value = finv_test(*in, do_invert);

      /* Gauss. */
      float multiplier = gausstab_[index];
      alpha_accum += value * multiplier;
      multiplier_accum += multiplier;

      /* Dilate - find most extreme color. */
      if (value > value_max) {
        multiplier = distbuf_inv_[index];
        value *= multiplier;
        if (value > value_max) {
          value_max = value;
          distfacinv_max = multiplier;
        }
      }
    }

    /* Blend between the max value and gauss blue - gives nice feather. */
    const float value_blur = alpha_accum / multiplier_accum;
    const float value_final = (value_max * distfacinv_max) +
                              (value_blur * (1.0f - distfacinv_max));
    *it.out = finv_test(value_final, do_invert);
  }
}

}  // namespace blender::compositor
