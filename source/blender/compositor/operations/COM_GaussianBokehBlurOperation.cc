/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GaussianBokehBlurOperation.h"

#include "RE_pipeline.h"

namespace blender::compositor {

GaussianBokehBlurOperation::GaussianBokehBlurOperation() : BlurBaseOperation(DataType::Color)
{
  gausstab_ = nullptr;
}

void GaussianBokehBlurOperation::init_data()
{
  BlurBaseOperation::init_data();
  const float width = this->get_width();
  const float height = this->get_height();

  if (!sizeavailable_) {
    update_size();
  }

  radxf_ = size_ * float(data_.sizex);
  CLAMP(radxf_, 0.0f, width / 2.0f);

  /* Vertical. */
  radyf_ = size_ * float(data_.sizey);
  CLAMP(radyf_, 0.0f, height / 2.0f);

  radx_ = ceil(radxf_);
  rady_ = ceil(radyf_);
}

void GaussianBokehBlurOperation::init_execution()
{
  BlurBaseOperation::init_execution();

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
        float fj = float(j) * facy;
        float fi = float(i) * facx;
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

void GaussianBokehBlurOperation::deinit_execution()
{
  BlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }
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
        data_.sizex = int(data_.percentx * 0.01f * data_.image_in_width);
        data_.sizey = int(data_.percenty * 0.01f * data_.image_in_height);
        break;
      case CMP_NODE_BLUR_ASPECT_Y:
        data_.sizex = int(data_.percentx * 0.01f * data_.image_in_width);
        data_.sizey = int(data_.percenty * 0.01f * data_.image_in_width);
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        data_.sizex = int(data_.percentx * 0.01f * data_.image_in_height);
        data_.sizey = int(data_.percenty * 0.01f * data_.image_in_height);
        break;
    }
  }

  /* Horizontal. */
  filtersizex_ = float(data_.sizex);
  int imgx = get_width() / 2;
  if (filtersizex_ > imgx) {
    filtersizex_ = imgx;
  }
  else if (filtersizex_ < 1) {
    filtersizex_ = 1;
  }
  radx_ = float(filtersizex_);

  /* Vertical. */
  filtersizey_ = float(data_.sizey);
  int imgy = get_height() / 2;
  if (filtersizey_ > imgy) {
    filtersizey_ = imgy;
  }
  else if (filtersizey_ < 1) {
    filtersizey_ = 1;
  }
  rady_ = float(filtersizey_);
}

void GaussianBlurReferenceOperation::init_execution()
{
  BlurBaseOperation::init_execution();

  update_gauss();
}

void GaussianBlurReferenceOperation::update_gauss()
{
  int i;
  int x = std::max(filtersizex_, filtersizey_);
  maintabs_ = (float **)MEM_mallocN(x * sizeof(float *), "gauss array");
  for (i = 0; i < x; i++) {
    maintabs_[i] = make_gausstab(i + 1, i + 1);
  }
}

void GaussianBlurReferenceOperation::deinit_execution()
{
  int x, i;
  x = std::max(filtersizex_, filtersizey_);
  for (i = 0; i < x; i++) {
    MEM_freeN(maintabs_[i]);
  }
  MEM_freeN(maintabs_);
  BlurBaseOperation::deinit_execution();
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
    int ref_radx = int(ref_size * radx_);
    int ref_rady = int(ref_size * rady_);
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
