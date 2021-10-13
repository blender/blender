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
  this->m_gausstab = nullptr;
}

void *GaussianBokehBlurOperation::initializeTileData(rcti * /*rect*/)
{
  lockMutex();
  if (!this->m_sizeavailable) {
    updateGauss();
  }
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  unlockMutex();
  return buffer;
}

void GaussianBokehBlurOperation::init_data()
{
  BlurBaseOperation::init_data();
  const float width = this->getWidth();
  const float height = this->getHeight();

  if (!this->m_sizeavailable) {
    updateSize();
  }

  radxf_ = this->m_size * (float)this->m_data.sizex;
  CLAMP(radxf_, 0.0f, width / 2.0f);

  /* Vertical. */
  radyf_ = this->m_size * (float)this->m_data.sizey;
  CLAMP(radyf_, 0.0f, height / 2.0f);

  this->m_radx = ceil(radxf_);
  this->m_rady = ceil(radyf_);
}

void GaussianBokehBlurOperation::initExecution()
{
  BlurBaseOperation::initExecution();

  initMutex();

  if (this->m_sizeavailable) {
    updateGauss();
  }
}

void GaussianBokehBlurOperation::updateGauss()
{
  if (this->m_gausstab == nullptr) {
    int ddwidth = 2 * this->m_radx + 1;
    int ddheight = 2 * this->m_rady + 1;
    int n = ddwidth * ddheight;
    /* create a full filter image */
    float *ddgauss = (float *)MEM_mallocN(sizeof(float) * n, __func__);
    float *dgauss = ddgauss;
    float sum = 0.0f;
    float facx = (radxf_ > 0.0f ? 1.0f / radxf_ : 0.0f);
    float facy = (radyf_ > 0.0f ? 1.0f / radyf_ : 0.0f);
    for (int j = -this->m_rady; j <= this->m_rady; j++) {
      for (int i = -this->m_radx; i <= this->m_radx; i++, dgauss++) {
        float fj = (float)j * facy;
        float fi = (float)i * facx;
        float dist = sqrt(fj * fj + fi * fi);
        *dgauss = RE_filter_value(this->m_data.filtertype, dist);

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
      int center = m_rady * ddwidth + m_radx;
      ddgauss[center] = 1.0f;
    }

    this->m_gausstab = ddgauss;
  }
}

void GaussianBokehBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  float tempColor[4];
  tempColor[0] = 0;
  tempColor[1] = 0;
  tempColor[2] = 0;
  tempColor[3] = 0;
  float multiplier_accum = 0;
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();
  int bufferwidth = inputBuffer->getWidth();
  const rcti &input_rect = inputBuffer->get_rect();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int ymin = max_ii(y - this->m_rady, input_rect.ymin);
  int ymax = min_ii(y + this->m_rady + 1, input_rect.ymax);
  int xmin = max_ii(x - this->m_radx, input_rect.xmin);
  int xmax = min_ii(x + this->m_radx + 1, input_rect.xmax);

  int index;
  int step = QualityStepHelper::getStep();
  int offsetadd = QualityStepHelper::getOffsetAdd();
  const int addConst = (xmin - x + this->m_radx);
  const int mulConst = (this->m_radx * 2 + 1);
  for (int ny = ymin; ny < ymax; ny += step) {
    index = ((ny - y) + this->m_rady) * mulConst + addConst;
    int bufferindex = ((xmin - bufferstartx) * 4) + ((ny - bufferstarty) * 4 * bufferwidth);
    for (int nx = xmin; nx < xmax; nx += step) {
      const float multiplier = this->m_gausstab[index];
      madd_v4_v4fl(tempColor, &buffer[bufferindex], multiplier);
      multiplier_accum += multiplier;
      index += step;
      bufferindex += offsetadd;
    }
  }

  mul_v4_v4fl(output, tempColor, 1.0f / multiplier_accum);
}

void GaussianBokehBlurOperation::deinitExecution()
{
  BlurBaseOperation::deinitExecution();

  if (this->m_gausstab) {
    MEM_freeN(this->m_gausstab);
    this->m_gausstab = nullptr;
  }

  deinitMutex();
}

bool GaussianBokehBlurOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
  rcti sizeInput;
  sizeInput.xmin = 0;
  sizeInput.ymin = 0;
  sizeInput.xmax = 5;
  sizeInput.ymax = 5;
  NodeOperation *operation = this->getInputOperation(1);

  if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
    return true;
  }

  if (this->m_sizeavailable && this->m_gausstab != nullptr) {
    newInput.xmin = 0;
    newInput.ymin = 0;
    newInput.xmax = this->getWidth();
    newInput.ymax = this->getHeight();
  }
  else {
    int addx = this->m_radx;
    int addy = this->m_rady;
    newInput.xmax = input->xmax + addx;
    newInput.xmin = input->xmin - addx;
    newInput.ymax = input->ymax + addy;
    newInput.ymin = input->ymin - addy;
  }
  return BlurBaseOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void GaussianBokehBlurOperation::get_area_of_interest(const int input_idx,
                                                      const rcti &output_area,
                                                      rcti &r_input_area)
{
  if (input_idx != IMAGE_INPUT_INDEX) {
    BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
    return;
  }

  r_input_area.xmax = output_area.xmax + m_radx;
  r_input_area.xmin = output_area.xmin - m_radx;
  r_input_area.ymax = output_area.ymax + m_rady;
  r_input_area.ymin = output_area.ymin - m_rady;
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

    const int ymin = max_ii(y - this->m_rady, input_rect.ymin);
    const int ymax = min_ii(y + this->m_rady + 1, input_rect.ymax);
    const int xmin = max_ii(x - this->m_radx, input_rect.xmin);
    const int xmax = min_ii(x + this->m_radx + 1, input_rect.xmax);

    float tempColor[4] = {0};
    float multiplier_accum = 0;
    const int step = QualityStepHelper::getStep();
    const int elem_step = step * input->elem_stride;
    const int add_const = (xmin - x + this->m_radx);
    const int mul_const = (this->m_radx * 2 + 1);
    for (int ny = ymin; ny < ymax; ny += step) {
      const float *color = input->get_elem(xmin, ny);
      int gauss_index = ((ny - y) + this->m_rady) * mul_const + add_const;
      const int gauss_end = gauss_index + (xmax - xmin);
      for (; gauss_index < gauss_end; gauss_index += step, color += elem_step) {
        const float multiplier = this->m_gausstab[gauss_index];
        madd_v4_v4fl(tempColor, color, multiplier);
        multiplier_accum += multiplier;
      }
    }

    mul_v4_v4fl(it.out, tempColor, 1.0f / multiplier_accum);
  }
}

// reference image
GaussianBlurReferenceOperation::GaussianBlurReferenceOperation()
    : BlurBaseOperation(DataType::Color)
{
  this->m_maintabs = nullptr;
  use_variable_size_ = true;
}

void GaussianBlurReferenceOperation::init_data()
{
  /* Setup variables for gausstab and area of interest. */
  this->m_data.image_in_width = this->getWidth();
  this->m_data.image_in_height = this->getHeight();
  if (this->m_data.relative) {
    switch (this->m_data.aspect) {
      case CMP_NODE_BLUR_ASPECT_NONE:
        this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_width);
        this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_height);
        break;
      case CMP_NODE_BLUR_ASPECT_Y:
        this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_width);
        this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_width);
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_height);
        this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_height);
        break;
    }
  }

  /* Horizontal. */
  m_filtersizex = (float)this->m_data.sizex;
  int imgx = getWidth() / 2;
  if (m_filtersizex > imgx) {
    m_filtersizex = imgx;
  }
  else if (m_filtersizex < 1) {
    m_filtersizex = 1;
  }
  m_radx = (float)m_filtersizex;

  /* Vertical. */
  m_filtersizey = (float)this->m_data.sizey;
  int imgy = getHeight() / 2;
  if (m_filtersizey > imgy) {
    m_filtersizey = imgy;
  }
  else if (m_filtersizey < 1) {
    m_filtersizey = 1;
  }
  m_rady = (float)m_filtersizey;
}

void *GaussianBlurReferenceOperation::initializeTileData(rcti * /*rect*/)
{
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  return buffer;
}

void GaussianBlurReferenceOperation::initExecution()
{
  BlurBaseOperation::initExecution();

  updateGauss();
}

void GaussianBlurReferenceOperation::updateGauss()
{
  int i;
  int x = MAX2(m_filtersizex, m_filtersizey);
  m_maintabs = (float **)MEM_mallocN(x * sizeof(float *), "gauss array");
  for (i = 0; i < x; i++) {
    m_maintabs[i] = make_gausstab(i + 1, i + 1);
  }
}

void GaussianBlurReferenceOperation::executePixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *memorybuffer = (MemoryBuffer *)data;
  float *buffer = memorybuffer->getBuffer();
  float *gausstabx, *gausstabcenty;
  float *gausstaby, *gausstabcentx;
  int i, j;
  float *src;
  float sum, val;
  float rval, gval, bval, aval;
  int imgx = getWidth();
  int imgy = getHeight();
  float tempSize[4];
  this->m_inputSize->read(tempSize, x, y, data);
  float refSize = tempSize[0];
  int refradx = (int)(refSize * m_radx);
  int refrady = (int)(refSize * m_rady);
  if (refradx > m_filtersizex) {
    refradx = m_filtersizex;
  }
  else if (refradx < 1) {
    refradx = 1;
  }
  if (refrady > m_filtersizey) {
    refrady = m_filtersizey;
  }
  else if (refrady < 1) {
    refrady = 1;
  }

  if (refradx == 1 && refrady == 1) {
    memorybuffer->readNoCheck(output, x, y);
  }
  else {
    int minxr = x - refradx < 0 ? -x : -refradx;
    int maxxr = x + refradx > imgx ? imgx - x : refradx;
    int minyr = y - refrady < 0 ? -y : -refrady;
    int maxyr = y + refrady > imgy ? imgy - y : refrady;

    float *srcd = buffer + COM_DATA_TYPE_COLOR_CHANNELS * ((y + minyr) * imgx + x + minxr);

    gausstabx = m_maintabs[refradx - 1];
    gausstabcentx = gausstabx + refradx;
    gausstaby = m_maintabs[refrady - 1];
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

void GaussianBlurReferenceOperation::deinitExecution()
{
  int x, i;
  x = MAX2(this->m_filtersizex, this->m_filtersizey);
  for (i = 0; i < x; i++) {
    MEM_freeN(this->m_maintabs[i]);
  }
  MEM_freeN(this->m_maintabs);
  BlurBaseOperation::deinitExecution();
}

bool GaussianBlurReferenceOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
  NodeOperation *operation = this->getInputOperation(1);

  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
    return true;
  }

  int addx = this->m_data.sizex + 2;
  int addy = this->m_data.sizey + 2;
  newInput.xmax = input->xmax + addx;
  newInput.xmin = input->xmin - addx;
  newInput.ymax = input->ymax + addy;
  newInput.ymin = input->ymin - addy;
  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void GaussianBlurReferenceOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  if (input_idx != IMAGE_INPUT_INDEX) {
    BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
    return;
  }

  const int add_x = this->m_data.sizex + 2;
  const int add_y = this->m_data.sizey + 2;
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
    int ref_radx = (int)(ref_size * m_radx);
    int ref_rady = (int)(ref_size * m_rady);
    if (ref_radx > m_filtersizex) {
      ref_radx = m_filtersizex;
    }
    else if (ref_radx < 1) {
      ref_radx = 1;
    }
    if (ref_rady > m_filtersizey) {
      ref_rady = m_filtersizey;
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

    const int w = getWidth();
    const int height = getHeight();
    const int minxr = x - ref_radx < 0 ? -x : -ref_radx;
    const int maxxr = x + ref_radx > w ? w - x : ref_radx;
    const int minyr = y - ref_rady < 0 ? -y : -ref_rady;
    const int maxyr = y + ref_rady > height ? height - y : ref_rady;

    const float *gausstabx = m_maintabs[ref_radx - 1];
    const float *gausstabcentx = gausstabx + ref_radx;
    const float *gausstaby = m_maintabs[ref_rady - 1];
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
