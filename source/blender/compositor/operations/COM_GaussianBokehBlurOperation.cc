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

#include "COM_GaussianBokehBlurOperation.h"

#include "RE_pipeline.h"

namespace blender::compositor {

GaussianBokehBlurOperation::GaussianBokehBlurOperation() : BlurBaseOperation(DataType::Color)
{
  gausstab_ = nullptr;
}

void *GaussianBokehBlurOperation::initialize_tile_data(rcti * /*rect*/)
{
  lock_mutex();
  if (!sizeavailable_) {
    update_gauss();
  }
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  unlock_mutex();
  return buffer;
}

void GaussianBokehBlurOperation::init_data()
{
  BlurBaseOperation::init_data();
  const float width = this->get_width();
  const float height = this->get_height();

  if (execution_model_ == eExecutionModel::FullFrame) {
    if (!sizeavailable_) {
      update_size();
    }
  }

  radxf_ = size_ * (float)data_.sizex;
  CLAMP(radxf_, 0.0f, width / 2.0f);

  /* Vertical. */
  radyf_ = size_ * (float)data_.sizey;
  CLAMP(radyf_, 0.0f, height / 2.0f);

  radx_ = ceil(radxf_);
  rady_ = ceil(radyf_);
}

void GaussianBokehBlurOperation::init_execution()
{
  BlurBaseOperation::init_execution();

  init_mutex();

  if (sizeavailable_) {
    update_gauss();
  }
}

void GaussianBokehBlurOperation::update_gauss()
{
  if (gausstab_ == nullptr) {
    int ddwidth = 2 * radx_ + 1;
    int ddheight = 2 * rady_ + 1;
    int n = ddwidth * ddheight;
    /* create a full filter image */
    float *ddgauss = (float *)MEM_mallocN(sizeof(float) * n, __func__);
    float *dgauss = ddgauss;
    float sum = 0.0f;
    float facx = (radxf_ > 0.0f ? 1.0f / radxf_ : 0.0f);
    float facy = (radyf_ > 0.0f ? 1.0f / radyf_ : 0.0f);
    for (int j = -rady_; j <= rady_; j++) {
      for (int i = -radx_; i <= radx_; i++, dgauss++) {
        float fj = (float)j * facy;
        float fi = (float)i * facx;
        float dist = sqrt(fj * fj + fi * fi);
        *dgauss = RE_filter_value(data_.filtertype, dist);

        sum += *dgauss;
      }
    }

    if (sum > 0.0f) {
      /* normalize */
      float norm = 1.0f / sum;
      for (int j = n - 1; j >= 0; j--) {
        ddgauss[j] *= norm;
      }
    }
    else {
      int center = rady_ * ddwidth + radx_;
      ddgauss[center] = 1.0f;
    }

    gausstab_ = ddgauss;
  }
}

void GaussianBokehBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float result[4];
  input_size_->read_sampled(result, 0, 0, PixelSampler::Nearest);
  size_ = result[0];

  const float width = this->get_width();
  const float height = this->get_height();

  radxf_ = size_ * (float)data_.sizex;
  CLAMP(radxf_, 0.0f, width / 2.0f);

  radyf_ = size_ * (float)data_.sizey;
  CLAMP(radyf_, 0.0f, height / 2.0f);

  radx_ = ceil(radxf_);
  rady_ = ceil(radyf_);

  float temp_color[4];
  temp_color[0] = 0;
  temp_color[1] = 0;
  temp_color[2] = 0;
  temp_color[3] = 0;
  float multiplier_accum = 0;
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();
  int bufferwidth = input_buffer->get_width();
  const rcti &input_rect = input_buffer->get_rect();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int ymin = max_ii(y - rady_, input_rect.ymin);
  int ymax = min_ii(y + rady_ + 1, input_rect.ymax);
  int xmin = max_ii(x - radx_, input_rect.xmin);
  int xmax = min_ii(x + radx_ + 1, input_rect.xmax);

  int index;
  int step = QualityStepHelper::get_step();
  int offsetadd = QualityStepHelper::get_offset_add();
  const int add_const = (xmin - x + radx_);
  const int mul_const = (radx_ * 2 + 1);
  for (int ny = ymin; ny < ymax; ny += step) {
    index = ((ny - y) + rady_) * mul_const + add_const;
    int bufferindex = ((xmin - bufferstartx) * 4) + ((ny - bufferstarty) * 4 * bufferwidth);
    for (int nx = xmin; nx < xmax; nx += step) {
      const float multiplier = gausstab_[index];
      madd_v4_v4fl(temp_color, &buffer[bufferindex], multiplier);
      multiplier_accum += multiplier;
      index += step;
      bufferindex += offsetadd;
    }
  }

  mul_v4_v4fl(output, temp_color, 1.0f / multiplier_accum);
}

void GaussianBokehBlurOperation::deinit_execution()
{
  BlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }

  deinit_mutex();
}

bool GaussianBokehBlurOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  rcti size_input;
  size_input.xmin = 0;
  size_input.ymin = 0;
  size_input.xmax = 5;
  size_input.ymax = 5;
  NodeOperation *operation = this->get_input_operation(1);

  if (operation->determine_depending_area_of_interest(&size_input, read_operation, output)) {
    return true;
  }

  if (sizeavailable_ && gausstab_ != nullptr) {
    new_input.xmin = 0;
    new_input.ymin = 0;
    new_input.xmax = this->get_width();
    new_input.ymax = this->get_height();
  }
  else {
    int addx = radx_;
    int addy = rady_;
    new_input.xmax = input->xmax + addx;
    new_input.xmin = input->xmin - addx;
    new_input.ymax = input->ymax + addy;
    new_input.ymin = input->ymin - addy;
  }
  return BlurBaseOperation::determine_depending_area_of_interest(
      &new_input, read_operation, output);
}

void GaussianBokehBlurOperation::get_area_of_interest(const int input_idx,
                                                      const rcti &output_area,
                                                      rcti &r_input_area)
{
  if (input_idx != IMAGE_INPUT_INDEX) {
    BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
    return;
  }

  r_input_area.xmax = output_area.xmax + radx_;
  r_input_area.xmin = output_area.xmin - radx_;
  r_input_area.ymax = output_area.ymax + rady_;
  r_input_area.ymin = output_area.ymin - rady_;
}

void GaussianBokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[IMAGE_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({}, area);
  const rcti &input_rect = input->get_rect();
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    const int ymin = max_ii(y - rady_, input_rect.ymin);
    const int ymax = min_ii(y + rady_ + 1, input_rect.ymax);
    const int xmin = max_ii(x - radx_, input_rect.xmin);
    const int xmax = min_ii(x + radx_ + 1, input_rect.xmax);

    float temp_color[4] = {0};
    float multiplier_accum = 0;
    const int step = QualityStepHelper::get_step();
    const int elem_step = step * input->elem_stride;
    const int add_const = (xmin - x + radx_);
    const int mul_const = (radx_ * 2 + 1);
    for (int ny = ymin; ny < ymax; ny += step) {
      const float *color = input->get_elem(xmin, ny);
      int gauss_index = ((ny - y) + rady_) * mul_const + add_const;
      const int gauss_end = gauss_index + (xmax - xmin);
      for (; gauss_index < gauss_end; gauss_index += step, color += elem_step) {
        const float multiplier = gausstab_[gauss_index];
        madd_v4_v4fl(temp_color, color, multiplier);
        multiplier_accum += multiplier;
      }
    }

    mul_v4_v4fl(it.out, temp_color, 1.0f / multiplier_accum);
  }
}

// reference image
GaussianBlurReferenceOperation::GaussianBlurReferenceOperation()
    : BlurBaseOperation(DataType::Color)
{
  maintabs_ = nullptr;
  use_variable_size_ = true;
}

void GaussianBlurReferenceOperation::init_data()
{
  /* Setup variables for gausstab and area of interest. */
  data_.image_in_width = this->get_width();
  data_.image_in_height = this->get_height();
  if (data_.relative) {
    switch (data_.aspect) {
      case CMP_NODE_BLUR_ASPECT_NONE:
        data_.sizex = (int)(data_.percentx * 0.01f * data_.image_in_width);
        data_.sizey = (int)(data_.percenty * 0.01f * data_.image_in_height);
        break;
      case CMP_NODE_BLUR_ASPECT_Y:
        data_.sizex = (int)(data_.percentx * 0.01f * data_.image_in_width);
        data_.sizey = (int)(data_.percenty * 0.01f * data_.image_in_width);
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        data_.sizex = (int)(data_.percentx * 0.01f * data_.image_in_height);
        data_.sizey = (int)(data_.percenty * 0.01f * data_.image_in_height);
        break;
    }
  }

  /* Horizontal. */
  filtersizex_ = (float)data_.sizex;
  int imgx = get_width() / 2;
  if (filtersizex_ > imgx) {
    filtersizex_ = imgx;
  }
  else if (filtersizex_ < 1) {
    filtersizex_ = 1;
  }
  radx_ = (float)filtersizex_;

  /* Vertical. */
  filtersizey_ = (float)data_.sizey;
  int imgy = get_height() / 2;
  if (filtersizey_ > imgy) {
    filtersizey_ = imgy;
  }
  else if (filtersizey_ < 1) {
    filtersizey_ = 1;
  }
  rady_ = (float)filtersizey_;
}

void *GaussianBlurReferenceOperation::initialize_tile_data(rcti * /*rect*/)
{
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  return buffer;
}

void GaussianBlurReferenceOperation::init_execution()
{
  BlurBaseOperation::init_execution();

  update_gauss();
}

void GaussianBlurReferenceOperation::update_gauss()
{
  int i;
  int x = MAX2(filtersizex_, filtersizey_);
  maintabs_ = (float **)MEM_mallocN(x * sizeof(float *), "gauss array");
  for (i = 0; i < x; i++) {
    maintabs_[i] = make_gausstab(i + 1, i + 1);
  }
}

void GaussianBlurReferenceOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *memorybuffer = (MemoryBuffer *)data;
  float *buffer = memorybuffer->get_buffer();
  float *gausstabx, *gausstabcenty;
  float *gausstaby, *gausstabcentx;
  int i, j;
  float *src;
  float sum, val;
  float rval, gval, bval, aval;
  int imgx = get_width();
  int imgy = get_height();
  float temp_size[4];
  input_size_->read(temp_size, x, y, data);
  float ref_size = temp_size[0];
  int refradx = (int)(ref_size * radx_);
  int refrady = (int)(ref_size * rady_);
  if (refradx > filtersizex_) {
    refradx = filtersizex_;
  }
  else if (refradx < 1) {
    refradx = 1;
  }
  if (refrady > filtersizey_) {
    refrady = filtersizey_;
  }
  else if (refrady < 1) {
    refrady = 1;
  }

  if (refradx == 1 && refrady == 1) {
    memorybuffer->read_no_check(output, x, y);
  }
  else {
    int minxr = x - refradx < 0 ? -x : -refradx;
    int maxxr = x + refradx > imgx ? imgx - x : refradx;
    int minyr = y - refrady < 0 ? -y : -refrady;
    int maxyr = y + refrady > imgy ? imgy - y : refrady;

    float *srcd = buffer + COM_DATA_TYPE_COLOR_CHANNELS * ((y + minyr) * imgx + x + minxr);

    gausstabx = maintabs_[refradx - 1];
    gausstabcentx = gausstabx + refradx;
    gausstaby = maintabs_[refrady - 1];
    gausstabcenty = gausstaby + refrady;

    sum = gval = rval = bval = aval = 0.0f;
    for (i = minyr; i < maxyr; i++, srcd += COM_DATA_TYPE_COLOR_CHANNELS * imgx) {
      src = srcd;
      for (j = minxr; j < maxxr; j++, src += COM_DATA_TYPE_COLOR_CHANNELS) {

        val = gausstabcenty[i] * gausstabcentx[j];
        sum += val;
        rval += val * src[0];
        gval += val * src[1];
        bval += val * src[2];
        aval += val * src[3];
      }
    }
    sum = 1.0f / sum;
    output[0] = rval * sum;
    output[1] = gval * sum;
    output[2] = bval * sum;
    output[3] = aval * sum;
  }
}

void GaussianBlurReferenceOperation::deinit_execution()
{
  int x, i;
  x = MAX2(filtersizex_, filtersizey_);
  for (i = 0; i < x; i++) {
    MEM_freeN(maintabs_[i]);
  }
  MEM_freeN(maintabs_);
  BlurBaseOperation::deinit_execution();
}

bool GaussianBlurReferenceOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  NodeOperation *operation = this->get_input_operation(1);

  if (operation->determine_depending_area_of_interest(input, read_operation, output)) {
    return true;
  }

  int addx = data_.sizex + 2;
  int addy = data_.sizey + 2;
  new_input.xmax = input->xmax + addx;
  new_input.xmin = input->xmin - addx;
  new_input.ymax = input->ymax + addy;
  new_input.ymin = input->ymin - addy;
  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void GaussianBlurReferenceOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  if (input_idx != IMAGE_INPUT_INDEX) {
    BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
    return;
  }

  const int add_x = data_.sizex + 2;
  const int add_y = data_.sizey + 2;
  r_input_area.xmax = output_area.xmax + add_x;
  r_input_area.xmin = output_area.xmin - add_x;
  r_input_area.ymax = output_area.ymax + add_y;
  r_input_area.ymin = output_area.ymin - add_y;
}

void GaussianBlurReferenceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *image_input = inputs[IMAGE_INPUT_INDEX];
  MemoryBuffer *size_input = inputs[SIZE_INPUT_INDEX];
  for (BuffersIterator<float> it = output->iterate_with({size_input}, area); !it.is_end(); ++it) {
    const float ref_size = *it.in(0);
    int ref_radx = (int)(ref_size * radx_);
    int ref_rady = (int)(ref_size * rady_);
    if (ref_radx > filtersizex_) {
      ref_radx = filtersizex_;
    }
    else if (ref_radx < 1) {
      ref_radx = 1;
    }
    if (ref_rady > filtersizey_) {
      ref_rady = filtersizey_;
    }
    else if (ref_rady < 1) {
      ref_rady = 1;
    }

    const int x = it.x;
    const int y = it.y;
    if (ref_radx == 1 && ref_rady == 1) {
      image_input->read_elem(x, y, it.out);
      continue;
    }

    const int w = get_width();
    const int height = get_height();
    const int minxr = x - ref_radx < 0 ? -x : -ref_radx;
    const int maxxr = x + ref_radx > w ? w - x : ref_radx;
    const int minyr = y - ref_rady < 0 ? -y : -ref_rady;
    const int maxyr = y + ref_rady > height ? height - y : ref_rady;

    const float *gausstabx = maintabs_[ref_radx - 1];
    const float *gausstabcentx = gausstabx + ref_radx;
    const float *gausstaby = maintabs_[ref_rady - 1];
    const float *gausstabcenty = gausstaby + ref_rady;

    float gauss_sum = 0.0f;
    float color_sum[4] = {0};
    const float *row_color = image_input->get_elem(x + minxr, y + minyr);
    for (int i = minyr; i < maxyr; i++, row_color += image_input->row_stride) {
      const float *color = row_color;
      for (int j = minxr; j < maxxr; j++, color += image_input->elem_stride) {
        const float val = gausstabcenty[i] * gausstabcentx[j];
        gauss_sum += val;
        madd_v4_v4fl(color_sum, color, val);
      }
    }
    mul_v4_v4fl(it.out, color_sum, 1.0f / gauss_sum);
  }
}

}  // namespace blender::compositor
