/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GaussianAlphaYBlurOperation.h"

namespace blender::compositor {

GaussianAlphaYBlurOperation::GaussianAlphaYBlurOperation()
    : GaussianAlphaBlurBaseOperation(eDimension::Y)
{
}

void *GaussianAlphaYBlurOperation::initialize_tile_data(rcti * /*rect*/)
{
  lock_mutex();
  if (!sizeavailable_) {
    update_gauss();
  }
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  unlock_mutex();
  return buffer;
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void GaussianAlphaYBlurOperation::init_execution()
{
  GaussianAlphaBlurBaseOperation::init_execution();

  init_mutex();

  if (sizeavailable_ && execution_model_ == eExecutionModel::Tiled) {
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
    distbuf_inv_ = BlurBaseOperation::make_dist_fac_inverse(rad, filtersize_, falloff_);
  }
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void GaussianAlphaYBlurOperation::update_gauss()
{
  if (gausstab_ == nullptr) {
    update_size();
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
  }

  if (distbuf_inv_ == nullptr) {
    update_size();
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    distbuf_inv_ = BlurBaseOperation::make_dist_fac_inverse(rad, filtersize_, falloff_);
  }
}

void GaussianAlphaYBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const bool do_invert = do_subtract_;
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const rcti &input_rect = input_buffer->get_rect();
  float *buffer = input_buffer->get_buffer();
  int bufferwidth = input_buffer->get_width();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int xmin = max_ii(x, input_rect.xmin);
  int ymin = max_ii(y - filtersize_, input_rect.ymin);
  int ymax = min_ii(y + filtersize_ + 1, input_rect.ymax);

  /* *** this is the main part which is different to 'GaussianYBlurOperation'  *** */
  int step = get_step();

  /* gauss */
  float alpha_accum = 0.0f;
  float multiplier_accum = 0.0f;

  /* dilate */
  float value_max = finv_test(
      buffer[(x) + (y * bufferwidth)],
      do_invert);              /* init with the current color to avoid unneeded lookups */
  float distfacinv_max = 1.0f; /* 0 to 1 */

  for (int ny = ymin; ny < ymax; ny += step) {
    int bufferindex = (xmin - bufferstartx) + ((ny - bufferstarty) * bufferwidth);

    const int index = (ny - y) + filtersize_;
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
  }

  /* blend between the max value and gauss blue - gives nice feather */
  const float value_blur = alpha_accum / multiplier_accum;
  const float value_final = (value_max * distfacinv_max) + (value_blur * (1.0f - distfacinv_max));
  output[0] = finv_test(value_final, do_invert);
}

void GaussianAlphaYBlurOperation::deinit_execution()
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

bool GaussianAlphaYBlurOperation::determine_depending_area_of_interest(
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
      new_input.xmax = input->xmax;
      new_input.xmin = input->xmin;
      new_input.ymax = input->ymax + filtersize_ + 1;
      new_input.ymin = input->ymin - filtersize_ - 1;
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
