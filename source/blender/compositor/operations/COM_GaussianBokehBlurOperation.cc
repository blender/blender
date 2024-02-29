/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_index_range.hh"
#include "BLI_math_vector.hh"

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
  weights_ = nullptr;
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
  const int2 radius = int2(filtersizex_, filtersizey_);
  const float2 scale = math::safe_divide(float2(1.0f), float2(radius));
  const int2 size = radius + int2(1);

  rcti weights_area;
  BLI_rcti_init(&weights_area, 0, size.x, 0, size.y);
  weights_ = std::make_unique<MemoryBuffer>(DataType::Value, weights_area, false);

  float sum = 0.0f;

  const float center_weight = RE_filter_value(data_.filtertype, 0.0f);
  *weights_->get_elem(0, 0) = center_weight;
  sum += center_weight;

  for (const int x : IndexRange(size.x).drop_front(1)) {
    const float weight = RE_filter_value(data_.filtertype, x * scale.x);
    *weights_->get_elem(x, 0) = weight;
    sum += weight * 2.0f;
  }

  for (const int y : IndexRange(size.y).drop_front(1)) {
    const float weight = RE_filter_value(data_.filtertype, y * scale.y);
    *weights_->get_elem(0, y) = weight;
    sum += weight * 2.0f;
  }

  for (const int y : IndexRange(size.y).drop_front(1)) {
    for (const int x : IndexRange(size.x).drop_front(1)) {
      const float weight = RE_filter_value(data_.filtertype, math::length(float2(x, y) * scale));
      *weights_->get_elem(x, y) = weight;
      sum += weight * 4.0f;
    }
  }

  for (const int y : IndexRange(size.y)) {
    for (const int x : IndexRange(size.x)) {
      *weights_->get_elem(x, y) /= sum;
    }
  }
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
  const MemoryBuffer *size_input = inputs[SIZE_INPUT_INDEX];
  const MemoryBuffer *image_input = inputs[IMAGE_INPUT_INDEX];

  int2 weights_size = int2(weights_->get_width(), weights_->get_height());
  int2 base_radius = weights_size - int2(1);

  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float4 accumulated_color = float4(0.0f);
    float4 accumulated_weight = float4(0.0f);

    int2 radius = int2(math::ceil(float2(base_radius) * *size_input->get_elem(it.x, it.y)));

    float4 center_color = float4(image_input->get_elem_clamped(it.x, it.y));
    float center_weight = *weights_->get_elem(0, 0);
    accumulated_color += center_color * center_weight;
    accumulated_weight += center_weight;

    for (int x = 1; x <= radius.x; x++) {
      float weight_coordinates = (x / float(radius.x)) * base_radius.x;
      float weight;
      weights_->read_elem_bilinear(weight_coordinates, 0.0f, &weight);
      accumulated_color += float4(image_input->get_elem_clamped(it.x + x, it.y)) * weight;
      accumulated_color += float4(image_input->get_elem_clamped(it.x - x, it.y)) * weight;
      accumulated_weight += weight * 2.0f;
    }

    for (int y = 1; y <= radius.y; y++) {
      float weight_coordinates = (y / float(radius.y)) * base_radius.y;
      float weight;
      weights_->read_elem_bilinear(0.0f, weight_coordinates, &weight);
      accumulated_color += float4(image_input->get_elem_clamped(it.x, it.y + y)) * weight;
      accumulated_color += float4(image_input->get_elem_clamped(it.x, it.y - y)) * weight;
      accumulated_weight += weight * 2.0f;
    }

    for (int y = 1; y <= radius.y; y++) {
      for (int x = 1; x <= radius.x; x++) {
        float2 weight_coordinates = (float2(x, y) / float2(radius)) * float2(base_radius);
        float weight;
        weights_->read_elem_bilinear(weight_coordinates.x, weight_coordinates.y, &weight);
        accumulated_color += float4(image_input->get_elem_clamped(it.x + x, it.y + y)) * weight;
        accumulated_color += float4(image_input->get_elem_clamped(it.x - x, it.y + y)) * weight;
        accumulated_color += float4(image_input->get_elem_clamped(it.x + x, it.y - y)) * weight;
        accumulated_color += float4(image_input->get_elem_clamped(it.x - x, it.y - y)) * weight;
        accumulated_weight += weight * 4.0f;
      }
    }

    accumulated_color = math::safe_divide(accumulated_color, accumulated_weight);
    copy_v4_v4(it.out, accumulated_color);
  }
}

}  // namespace blender::compositor
