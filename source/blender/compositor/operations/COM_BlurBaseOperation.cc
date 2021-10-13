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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_BlurBaseOperation.h"
#include "COM_ConstantOperation.h"

#include "RE_pipeline.h"

namespace blender::compositor {

BlurBaseOperation::BlurBaseOperation(DataType data_type)
{
  /* data_type is almost always DataType::Color except for alpha-blur */
  this->addInputSocket(data_type);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(data_type);
  this->flags.complex = true;
  m_inputProgram = nullptr;
  memset(&m_data, 0, sizeof(NodeBlurData));
  m_size = 1.0f;
  m_sizeavailable = false;
  m_extend_bounds = false;
  use_variable_size_ = false;
}

void BlurBaseOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    updateSize();
  }

  m_data.image_in_width = this->getWidth();
  m_data.image_in_height = this->getHeight();
  if (m_data.relative) {
    int sizex, sizey;
    switch (m_data.aspect) {
      case CMP_NODE_BLUR_ASPECT_Y:
        sizex = sizey = m_data.image_in_width;
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        sizex = sizey = m_data.image_in_height;
        break;
      default:
        BLI_assert(m_data.aspect == CMP_NODE_BLUR_ASPECT_NONE);
        sizex = m_data.image_in_width;
        sizey = m_data.image_in_height;
        break;
    }
    m_data.sizex = round_fl_to_int(m_data.percentx * 0.01f * sizex);
    m_data.sizey = round_fl_to_int(m_data.percenty * 0.01f * sizey);
  }
}

void BlurBaseOperation::initExecution()
{
  m_inputProgram = this->getInputSocketReader(0);
  m_inputSize = this->getInputSocketReader(1);

  QualityStepHelper::initExecution(COM_QH_MULTIPLY);
}

float *BlurBaseOperation::make_gausstab(float rad, int size)
{
  float *gausstab, sum, val;
  int i, n;

  n = 2 * size + 1;

  gausstab = (float *)MEM_mallocN(sizeof(float) * n, __func__);

  sum = 0.0f;
  float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
  for (i = -size; i <= size; i++) {
    val = RE_filter_value(m_data.filtertype, (float)i * fac);
    sum += val;
    gausstab[i + size] = val;
  }

  sum = 1.0f / sum;
  for (i = 0; i < n; i++) {
    gausstab[i] *= sum;
  }

  return gausstab;
}

#ifdef BLI_HAVE_SSE2
__m128 *BlurBaseOperation::convert_gausstab_sse(const float *gausstab, int size)
{
  int n = 2 * size + 1;
  __m128 *gausstab_sse = (__m128 *)MEM_mallocN_aligned(sizeof(__m128) * n, 16, "gausstab sse");
  for (int i = 0; i < n; i++) {
    gausstab_sse[i] = _mm_set1_ps(gausstab[i]);
  }
  return gausstab_sse;
}
#endif

/* normalized distance from the current (inverted so 1.0 is close and 0.0 is far)
 * 'ease' is applied after, looks nicer */
float *BlurBaseOperation::make_dist_fac_inverse(float rad, int size, int falloff)
{
  float *dist_fac_invert, val;
  int i, n;

  n = 2 * size + 1;

  dist_fac_invert = (float *)MEM_mallocN(sizeof(float) * n, __func__);

  float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
  for (i = -size; i <= size; i++) {
    val = 1.0f - fabsf((float)i * fac);

    /* keep in sync with rna_enum_proportional_falloff_curve_only_items */
    switch (falloff) {
      case PROP_SMOOTH:
        /* ease - gives less hard lines for dilate/erode feather */
        val = (3.0f * val * val - 2.0f * val * val * val);
        break;
      case PROP_SPHERE:
        val = sqrtf(2.0f * val - val * val);
        break;
      case PROP_ROOT:
        val = sqrtf(val);
        break;
      case PROP_SHARP:
        val = val * val;
        break;
      case PROP_INVSQUARE:
        val = val * (2.0f - val);
        break;
      case PROP_LIN:
        /* nothing to do */
        break;
#ifndef NDEBUG
      case -1:
        /* uninitialized! */
        BLI_assert(0);
        break;
#endif
      default:
        /* nothing */
        break;
    }
    dist_fac_invert[i + size] = val;
  }

  return dist_fac_invert;
}

void BlurBaseOperation::deinitExecution()
{
  m_inputProgram = nullptr;
  m_inputSize = nullptr;
}

void BlurBaseOperation::setData(const NodeBlurData *data)
{
  memcpy(&m_data, data, sizeof(NodeBlurData));
}

int BlurBaseOperation::get_blur_size(eDimension dim) const
{
  switch (dim) {
    case eDimension::X:
      return m_data.sizex;
    case eDimension::Y:
      return m_data.sizey;
  }
  return -1;
}

void BlurBaseOperation::updateSize()
{
  if (m_sizeavailable || use_variable_size_) {
    return;
  }

  switch (execution_model_) {
    case eExecutionModel::Tiled: {
      float result[4];
      this->getInputSocketReader(1)->readSampled(result, 0, 0, PixelSampler::Nearest);
      m_size = result[0];
      break;
    }
    case eExecutionModel::FullFrame: {
      NodeOperation *size_input = get_input_operation(SIZE_INPUT_INDEX);
      if (size_input->get_flags().is_constant_operation) {
        m_size = *static_cast<ConstantOperation *>(size_input)->get_constant_elem();
      } /* Else use default. */
      break;
    }
  }
  m_sizeavailable = true;
}

void BlurBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (!m_extend_bounds) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  switch (execution_model_) {
    case eExecutionModel::Tiled: {
      NodeOperation::determine_canvas(preferred_area, r_area);
      r_area.xmax += 2 * m_size * m_data.sizex;
      r_area.ymax += 2 * m_size * m_data.sizey;
      break;
    }
    case eExecutionModel::FullFrame: {
      /* Setting a modifier ensures all non main inputs have extended bounds as preferred
       * canvas, avoiding unnecessary canvas conversions that would hide constant
       * operations. */
      set_determined_canvas_modifier([=](rcti &canvas) {
        /* Rounding to even prevents jiggling in backdrop while switching size values. */
        canvas.xmax += round_to_even(2 * m_size * m_data.sizex);
        canvas.ymax += round_to_even(2 * m_size * m_data.sizey);
      });
      NodeOperation::determine_canvas(preferred_area, r_area);
      break;
    }
  }
}

void BlurBaseOperation::get_area_of_interest(const int input_idx,
                                             const rcti &output_area,
                                             rcti &r_input_area)
{
  switch (input_idx) {
    case 0:
      r_input_area = output_area;
      break;
    case 1:
      r_input_area = use_variable_size_ ? output_area : COM_CONSTANT_INPUT_AREA_OF_INTEREST;
      break;
  }
}

}  // namespace blender::compositor
