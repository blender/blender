/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GaussianBlurBaseOperation.h"

namespace blender::compositor {

GaussianBlurBaseOperation::GaussianBlurBaseOperation(eDimension dim)
    : BlurBaseOperation(DataType::Color)
{
  gausstab_ = nullptr;
#if BLI_HAVE_SSE2
  gausstab_sse_ = nullptr;
#endif
  filtersize_ = 0;
  rad_ = 0.0f;
  dimension_ = dim;
}

void GaussianBlurBaseOperation::init_data()
{
  BlurBaseOperation::init_data();
  if (execution_model_ == eExecutionModel::FullFrame) {
    rad_ = max_ff(size_ * this->get_blur_size(dimension_), 0.0f);
    rad_ = min_ff(rad_, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad_), MAX_GAUSSTAB_RADIUS);
  }
}

void GaussianBlurBaseOperation::init_execution()
{
  BlurBaseOperation::init_execution();
  if (execution_model_ == eExecutionModel::FullFrame) {
    gausstab_ = BlurBaseOperation::make_gausstab(rad_, filtersize_);
#if BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianBlurBaseOperation::deinit_execution()
{
  BlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }
#if BLI_HAVE_SSE2
  if (gausstab_sse_) {
    MEM_freeN(gausstab_sse_);
    gausstab_sse_ = nullptr;
  }
#endif
}

void GaussianBlurBaseOperation::get_area_of_interest(const int input_idx,
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

void GaussianBlurBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  const int2 unit_offset = dimension_ == eDimension::X ? int2(1, 0) : int2(0, 1);
  MemoryBuffer *input = inputs[IMAGE_INPUT_INDEX];
  for (BuffersIterator<float> it = output->iterate_with({input}, area); !it.is_end(); ++it) {
    alignas(16) float4 accumulated_color = float4(0.0f);
#if BLI_HAVE_SSE2
    __m128 accumulated_color_sse = _mm_setzero_ps();
    for (int i = -filtersize_; i <= filtersize_; i++) {
      const int2 offset = unit_offset * i;
      __m128 weight = gausstab_sse_[i + filtersize_];
      __m128 color = _mm_load_ps(input->get_elem_clamped(it.x + offset.x, it.y + offset.y));
      __m128 weighted_color = _mm_mul_ps(color, weight);
      accumulated_color_sse = _mm_add_ps(accumulated_color_sse, weighted_color);
    }
    _mm_store_ps(accumulated_color, accumulated_color_sse);
#else
    for (int i = -filtersize_; i <= filtersize_; i++) {
      const int2 offset = unit_offset * i;
      const float weight = gausstab_[i + filtersize_];
      const float4 color = input->get_elem_clamped(it.x + offset.x, it.y + offset.y);
      accumulated_color += color * weight;
    }
#endif
    copy_v4_v4(it.out, accumulated_color);
  }
}

}  // namespace blender::compositor
