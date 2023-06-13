/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GaussianAlphaXBlurOperation.h"

namespace blender::compositor {

GaussianAlphaXBlurOperation::GaussianAlphaXBlurOperation()
    : GaussianAlphaBlurBaseOperation(eDimension::X)
{
}

void *GaussianAlphaXBlurOperation::initialize_tile_data(rcti * /*rect*/)
{
  lock_mutex();
  if (!sizeavailable_) {
    update_gauss();
  }
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  unlock_mutex();
  return buffer;
}

void GaussianAlphaXBlurOperation::init_execution()
{
  GaussianAlphaBlurBaseOperation::init_execution();

  init_mutex();

  if (sizeavailable_ && execution_model_ == eExecutionModel::Tiled) {
    float rad = max_ff(size_ * data_.sizex, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
    distbuf_inv_ = BlurBaseOperation::make_dist_fac_inverse(rad, filtersize_, falloff_);
  }
}

void GaussianAlphaXBlurOperation::update_gauss()
{
  if (gausstab_ == nullptr) {
    update_size();
    float rad = max_ff(size_ * data_.sizex, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
  }

  if (distbuf_inv_ == nullptr) {
    update_size();
    float rad = max_ff(size_ * data_.sizex, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    distbuf_inv_ = BlurBaseOperation::make_dist_fac_inverse(rad, filtersize_, falloff_);
  }
}

void GaussianAlphaXBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const bool do_invert = do_subtract_;
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();
  int bufferwidth = input_buffer->get_width();
  const rcti &input_rect = input_buffer->get_rect();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  const rcti &rect = input_buffer->get_rect();
  int xmin = max_ii(x - filtersize_, rect.xmin);
  int xmax = min_ii(x + filtersize_ + 1, rect.xmax);
  int ymin = max_ii(y, rect.ymin);

  /* *** this is the main part which is different to 'GaussianXBlurOperation'  *** */
  int step = get_step();
  int bufferindex = (xmin - bufferstartx) + ((ymin - bufferstarty) * bufferwidth);

  /* gauss */
  float alpha_accum = 0.0f;
  float multiplier_accum = 0.0f;

  /* dilate */
  float value_max = finv_test(
      buffer[(x) + (y * bufferwidth)],
      do_invert);              /* init with the current color to avoid unneeded lookups */
  float distfacinv_max = 1.0f; /* 0 to 1 */

  for (int nx = xmin; nx < xmax; nx += step) {
    const int index = (nx - x) + filtersize_;
    float value = finv_test(buffer[bufferindex], do_invert);
    float multiplier;

    /* gauss */
    {
      multiplier = gausstab_[index];
      alpha_accum += value * multiplier;
      multiplier_accum += multiplier;
    }

    /* dilate - find most extreme color */
    if (value > value_max) {
      multiplier = distbuf_inv_[index];
      value *= multiplier;
      if (value > value_max) {
        value_max = value;
        distfacinv_max = multiplier;
      }
    }
    bufferindex += step;
  }

  /* blend between the max value and gauss blue - gives nice feather */
  const float value_blur = alpha_accum / multiplier_accum;
  const float value_final = (value_max * distfacinv_max) + (value_blur * (1.0f - distfacinv_max));
  output[0] = finv_test(value_final, do_invert);
}

void GaussianAlphaXBlurOperation::deinit_execution()
{
  GaussianAlphaBlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }

  if (distbuf_inv_) {
    MEM_freeN(distbuf_inv_);
    distbuf_inv_ = nullptr;
  }

  deinit_mutex();
}

bool GaussianAlphaXBlurOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
#if 0 /* until we add size input */
  rcti size_input;
  size_input.xmin = 0;
  size_input.ymin = 0;
  size_input.xmax = 5;
  size_input.ymax = 5;

  NodeOperation *operation = this->get_input_operation(1);
  if (operation->determine_depending_area_of_interest(&size_input, read_operation, output)) {
    return true;
  }
  else
#endif
  {
    if (sizeavailable_ && gausstab_ != nullptr) {
      new_input.xmax = input->xmax + filtersize_ + 1;
      new_input.xmin = input->xmin - filtersize_ - 1;
      new_input.ymax = input->ymax;
      new_input.ymin = input->ymin;
    }
    else {
      new_input.xmax = this->get_width();
      new_input.xmin = 0;
      new_input.ymax = this->get_height();
      new_input.ymin = 0;
    }
    return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
  }
}

}  // namespace blender::compositor
