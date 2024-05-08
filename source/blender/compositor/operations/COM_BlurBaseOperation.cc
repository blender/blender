/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BlurBaseOperation.h"
#include "COM_ConstantOperation.h"

#include "RE_pipeline.h"

namespace blender::compositor {

BlurBaseOperation::BlurBaseOperation(DataType data_type)
{
  /* data_type is almost always DataType::Color except for alpha-blur */
  this->add_input_socket(data_type);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(data_type);
  flags_.can_be_constant = true;
  memset(&data_, 0, sizeof(NodeBlurData));
  size_ = 1.0f;
  sizeavailable_ = false;
  extend_bounds_ = false;
  use_variable_size_ = false;
}

void BlurBaseOperation::init_data()
{
  update_size();

  data_.image_in_width = this->get_width();
  data_.image_in_height = this->get_height();
  if (data_.relative) {
    int sizex, sizey;
    switch (data_.aspect) {
      case CMP_NODE_BLUR_ASPECT_Y:
        sizex = sizey = data_.image_in_width;
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        sizex = sizey = data_.image_in_height;
        break;
      default:
        BLI_assert(data_.aspect == CMP_NODE_BLUR_ASPECT_NONE);
        sizex = data_.image_in_width;
        sizey = data_.image_in_height;
        break;
    }
    data_.sizex = round_fl_to_int(data_.percentx * 0.01f * sizex);
    data_.sizey = round_fl_to_int(data_.percenty * 0.01f * sizey);
  }
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
    val = RE_filter_value(data_.filtertype, float(i) * fac);
    sum += val;
    gausstab[i + size] = val;
  }

  sum = 1.0f / sum;
  for (i = 0; i < n; i++) {
    gausstab[i] *= sum;
  }

  return gausstab;
}

#if BLI_HAVE_SSE2
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

float *BlurBaseOperation::make_dist_fac_inverse(float rad, int size, int falloff)
{
  float *dist_fac_invert, val;
  int i, n;

  n = 2 * size + 1;

  dist_fac_invert = (float *)MEM_mallocN(sizeof(float) * n, __func__);

  float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
  for (i = -size; i <= size; i++) {
    val = 1.0f - fabsf(float(i) * fac);

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

void BlurBaseOperation::set_data(const NodeBlurData *data)
{
  memcpy(&data_, data, sizeof(NodeBlurData));
}

int BlurBaseOperation::get_blur_size(eDimension dim) const
{
  switch (dim) {
    case eDimension::X:
      return data_.sizex;
    case eDimension::Y:
      return data_.sizey;
  }
  return -1;
}

void BlurBaseOperation::update_size()
{
  if (sizeavailable_ || use_variable_size_) {
    return;
  }

  NodeOperation *size_input = get_input_operation(SIZE_INPUT_INDEX);
  if (size_input->get_flags().is_constant_operation) {
    size_ = *static_cast<ConstantOperation *>(size_input)->get_constant_elem();
  } /* Else use default. */
  sizeavailable_ = true;
}

void BlurBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (!extend_bounds_) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  /* Setting a modifier ensures all non main inputs have extended bounds as preferred
   * canvas, avoiding unnecessary canvas conversions that would hide constant
   * operations. */
  set_determined_canvas_modifier([=](rcti &canvas) {
    /* Rounding to even prevents jiggling in backdrop while switching size values. */
    canvas.xmax += round_to_even(2 * size_ * data_.sizex);
    canvas.ymax += round_to_even(2 * size_ * data_.sizey);
  });
  NodeOperation::determine_canvas(preferred_area, r_area);
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
