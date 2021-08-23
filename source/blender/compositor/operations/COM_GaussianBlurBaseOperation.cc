/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

#include "COM_GaussianBlurBaseOperation.h"

namespace blender::compositor {

GaussianBlurBaseOperation::GaussianBlurBaseOperation(eDimension dim)
    : BlurBaseOperation(DataType::Color)
{
  m_gausstab = nullptr;
#ifdef BLI_HAVE_SSE2
  m_gausstab_sse = nullptr;
#endif
  m_filtersize = 0;
  rad_ = 0.0f;
  dimension_ = dim;
}

void GaussianBlurBaseOperation::init_data()
{
  BlurBaseOperation::init_data();
  if (execution_model_ == eExecutionModel::FullFrame) {
    rad_ = max_ff(m_size * this->get_blur_size(dimension_), 0.0f);
    rad_ = min_ff(rad_, MAX_GAUSSTAB_RADIUS);
    m_filtersize = min_ii(ceil(rad_), MAX_GAUSSTAB_RADIUS);
  }
}

void GaussianBlurBaseOperation::initExecution()
{
  BlurBaseOperation::initExecution();
  if (execution_model_ == eExecutionModel::FullFrame) {
    m_gausstab = BlurBaseOperation::make_gausstab(rad_, m_filtersize);
#ifdef BLI_HAVE_SSE2
    m_gausstab_sse = BlurBaseOperation::convert_gausstab_sse(m_gausstab, m_filtersize);
#endif
  }
}

void GaussianBlurBaseOperation::deinitExecution()
{
  BlurBaseOperation::deinitExecution();

  if (m_gausstab) {
    MEM_freeN(m_gausstab);
    m_gausstab = nullptr;
  }
#ifdef BLI_HAVE_SSE2
  if (m_gausstab_sse) {
    MEM_freeN(m_gausstab_sse);
    m_gausstab_sse = nullptr;
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
      r_input_area.xmin = output_area.xmin - m_filtersize - 1;
      r_input_area.xmax = output_area.xmax + m_filtersize + 1;
      break;
    case eDimension::Y:
      r_input_area.ymin = output_area.ymin - m_filtersize - 1;
      r_input_area.ymax = output_area.ymax + m_filtersize + 1;
      break;
  }
}

void GaussianBlurBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
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
      elem_stride = input->elem_stride;
      get_current_coord = [&] { return it.x; };
      break;
    case eDimension::Y:
      min_input_coord = input_rect.ymin;
      max_input_coord = input_rect.ymax;
      elem_stride = input->row_stride;
      get_current_coord = [&] { return it.y; };
      break;
  }

  for (; !it.is_end(); ++it) {
    const int coord = get_current_coord();
    const int coord_min = max_ii(coord - m_filtersize, min_input_coord);
    const int coord_max = min_ii(coord + m_filtersize + 1, max_input_coord);

    float ATTR_ALIGN(16) color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float multiplier_accum = 0.0f;

    const int step = QualityStepHelper::getStep();
    const float *in = it.in(0) + ((intptr_t)coord_min - coord) * elem_stride;
    const int in_stride = elem_stride * step;
    int gauss_idx = (coord_min - coord) + m_filtersize;
    const int gauss_end = gauss_idx + (coord_max - coord_min);
#ifdef BLI_HAVE_SSE2
    __m128 accum_r = _mm_load_ps(color_accum);
    for (; gauss_idx < gauss_end; in += in_stride, gauss_idx += step) {
      __m128 reg_a = _mm_load_ps(in);
      reg_a = _mm_mul_ps(reg_a, m_gausstab_sse[gauss_idx]);
      accum_r = _mm_add_ps(accum_r, reg_a);
      multiplier_accum += m_gausstab[gauss_idx];
    }
    _mm_store_ps(color_accum, accum_r);
#else
    for (; gauss_idx < gauss_end; in += in_stride, gauss_idx += step) {
      const float multiplier = m_gausstab[gauss_idx];
      madd_v4_v4fl(color_accum, in, multiplier);
      multiplier_accum += multiplier;
    }
#endif
    mul_v4_v4fl(it.out, color_accum, 1.0f / multiplier_accum);
  }
}

}  // namespace blender::compositor
