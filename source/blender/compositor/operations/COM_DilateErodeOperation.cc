/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DilateErodeOperation.h"

namespace blender::compositor {

DilateErodeThresholdOperation::DilateErodeThresholdOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.can_be_constant = true;
  inset_ = 0.0f;
  switch_ = 0.5f;
  distance_ = 0.0f;
}

void DilateErodeThresholdOperation::init_data()
{
  if (distance_ < 0.0f) {
    scope_ = -distance_ + inset_;
  }
  else {
    if (inset_ * 2 > distance_) {
      scope_ = std::max(inset_ * 2 - distance_, distance_);
    }
    else {
      scope_ = distance_;
    }
  }
  if (scope_ < 3) {
    scope_ = 3;
  }
}

void DilateErodeThresholdOperation::get_area_of_interest(const int input_idx,
                                                         const rcti &output_area,
                                                         rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - scope_;
  r_input_area.xmax = output_area.xmax + scope_;
  r_input_area.ymin = output_area.ymin - scope_;
  r_input_area.ymax = output_area.ymax + scope_;
}

struct DilateErodeThresholdOperation::PixelData {
  int x;
  int y;
  int xmin;
  int xmax;
  int ymin;
  int ymax;
  const float *elem;
  float distance;
  int elem_stride;
  int row_stride;
  /** Switch. */
  float sw;
};

template<template<typename> typename TCompare>
static float get_min_distance(DilateErodeThresholdOperation::PixelData &p)
{
  /* TODO(manzanilla): bad performance, generate a table with relative offsets on operation
   * initialization to loop from less to greater distance and break as soon as #compare is
   * true. */
  const TCompare compare;
  float min_dist = p.distance;
  const float *row = p.elem + (intptr_t(p.ymin) - p.y) * p.row_stride +
                     (intptr_t(p.xmin) - p.x) * p.elem_stride;
  for (int yi = p.ymin; yi < p.ymax; yi++) {
    const float dy = yi - p.y;
    const float dist_y = dy * dy;
    const float *elem = row;
    for (int xi = p.xmin; xi < p.xmax; xi++) {
      if (compare(*elem, p.sw)) {
        const float dx = xi - p.x;
        const float dist = dx * dx + dist_y;
        min_dist = std::min(min_dist, dist);
      }
      elem += p.elem_stride;
    }
    row += p.row_stride;
  }
  return min_dist;
}

void DilateErodeThresholdOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const rcti &input_rect = input->get_rect();
  const float rd = scope_ * scope_;
  const float inset = inset_;

  PixelData p;
  p.sw = switch_;
  p.distance = rd * 2;
  p.elem_stride = input->elem_stride;
  p.row_stride = input->row_stride;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.x = it.x;
    p.y = it.y;
    p.xmin = std::max(p.x - scope_, input_rect.xmin);
    p.ymin = std::max(p.y - scope_, input_rect.ymin);
    p.xmax = std::min(p.x + scope_, input_rect.xmax);
    p.ymax = std::min(p.y + scope_, input_rect.ymax);
    p.elem = it.in(0);

    float pixel_value;
    if (*p.elem > p.sw) {
      pixel_value = -sqrtf(get_min_distance<std::less>(p));
    }
    else {
      pixel_value = sqrtf(get_min_distance<std::greater>(p));
    }

    if (distance_ > 0.0f) {
      const float delta = distance_ - pixel_value;
      if (delta >= 0.0f) {
        *it.out = delta >= inset ? 1.0f : delta / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
    else {
      const float delta = -distance_ + pixel_value;
      if (delta < 0.0f) {
        *it.out = delta < -inset ? 1.0f : (-delta) / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
  }
}

DilateDistanceOperation::DilateDistanceOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  distance_ = 0.0f;
  flags_.can_be_constant = true;
}

void DilateDistanceOperation::init_data()
{
  scope_ = distance_;
  if (scope_ < 3) {
    scope_ = 3;
  }
}

void DilateDistanceOperation::get_area_of_interest(const int input_idx,
                                                   const rcti &output_area,
                                                   rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - scope_;
  r_input_area.xmax = output_area.xmax + scope_;
  r_input_area.ymin = output_area.ymin - scope_;
  r_input_area.ymax = output_area.ymax + scope_;
}

struct DilateDistanceOperation::PixelData {
  int x;
  int y;
  int xmin;
  int xmax;
  int ymin;
  int ymax;
  const float *elem;
  float min_distance;
  int scope;
  int elem_stride;
  int row_stride;
  const rcti &input_rect;

  PixelData(MemoryBuffer *input, const int distance, const int scope)
      : min_distance(distance * distance),
        scope(scope),
        elem_stride(input->elem_stride),
        row_stride(input->row_stride),
        input_rect(input->get_rect())
  {
  }

  void update(BuffersIterator<float> &it)
  {
    x = it.x;
    y = it.y;
    xmin = std::max(x - scope, input_rect.xmin);
    ymin = std::max(y - scope, input_rect.ymin);
    xmax = std::min(x + scope, input_rect.xmax);
    ymax = std::min(y + scope, input_rect.ymax);
    elem = it.in(0);
  }
};

template<template<typename> typename TCompare>
static float get_distance_value(DilateDistanceOperation::PixelData &p, const float start_value)
{
  /* TODO(manzanilla): bad performance, only loop elements within minimum distance removing
   * coordinates and conditional if `dist <= min_dist`. May need to generate a table of offsets. */
  const TCompare compare;
  const float min_dist = p.min_distance;
  float value = start_value;
  const float *row = p.elem + (intptr_t(p.ymin) - p.y) * p.row_stride +
                     (intptr_t(p.xmin) - p.x) * p.elem_stride;
  for (int yi = p.ymin; yi < p.ymax; yi++) {
    const float dy = yi - p.y;
    const float dist_y = dy * dy;
    const float *elem = row;
    for (int xi = p.xmin; xi < p.xmax; xi++) {
      const float dx = xi - p.x;
      const float dist = dx * dx + dist_y;
      if (dist <= min_dist) {
        value = compare(*elem, value) ? *elem : value;
      }
      elem += p.elem_stride;
    }
    row += p.row_stride;
  }

  return value;
}

void DilateDistanceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  PixelData p(inputs[0], distance_, scope_);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::greater>(p, 0.0f);
  }
}

ErodeDistanceOperation::ErodeDistanceOperation() : DilateDistanceOperation()
{
  /* pass */
}

void ErodeDistanceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  PixelData p(inputs[0], distance_, scope_);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::less>(p, 1.0f);
  }
}

DilateStepOperation::DilateStepOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
}

void DilateStepOperation::get_area_of_interest(const int input_idx,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - iterations_;
  r_input_area.xmax = output_area.xmax + iterations_;
  r_input_area.ymin = output_area.ymin - iterations_;
  r_input_area.ymax = output_area.ymax + iterations_;
}

template<typename TCompareSelector>
static void step_update_memory_buffer(MemoryBuffer *output,
                                      const MemoryBuffer *input,
                                      const rcti &area,
                                      const int num_iterations,
                                      const float compare_min_value)
{
  TCompareSelector selector;

  const int width = output->get_width();
  const int height = output->get_height();

  const int half_window = num_iterations;
  const int window = half_window * 2 + 1;

  const int xmin = std::max(0, area.xmin - half_window);
  const int ymin = std::max(0, area.ymin - half_window);
  const int xmax = std::min(width, area.xmax + half_window);
  const int ymax = std::min(height, area.ymax + half_window);

  const int bwidth = area.xmax - area.xmin;
  const int bheight = area.ymax - area.ymin;

  /* NOTE: #result has area width, but new height.
   * We have to calculate the additional rows in the first pass,
   * to have valid data available for the second pass. */
  rcti result_area;
  BLI_rcti_init(&result_area, area.xmin, area.xmax, ymin, ymax);
  MemoryBuffer result(DataType::Value, result_area);

  /* #temp holds maxima for every step in the algorithm, #buf holds a
   * single row or column of input values, padded with #limit values to
   * simplify the logic. */
  float *temp = (float *)MEM_mallocN(sizeof(float) * (2 * window - 1), "dilate erode temp");
  float *buf = (float *)MEM_mallocN(sizeof(float) * (std::max(bwidth, bheight) + 5 * half_window),
                                    "dilate erode buf");

  /* The following is based on the van Herk/Gil-Werman algorithm for morphology operations. */
  /* First pass, horizontal dilate/erode. */
  for (int y = ymin; y < ymax; y++) {
    for (int x = 0; x < bwidth + 5 * half_window; x++) {
      buf[x] = compare_min_value;
    }
    for (int x = xmin; x < xmax; x++) {
      buf[x - area.xmin + window - 1] = input->get_value(x, y, 0);
    }

    for (int i = 0; i < (bwidth + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (int x = 1; x < window; x++) {
        temp[window - 1 - x] = selector(temp[window - x], buf[start - x]);
        temp[window - 1 + x] = selector(temp[window + x - 2], buf[start + x]);
      }

      start = half_window + (i - 1) * window + 1;
      for (int x = -std::min(0, start); x < window - std::max(0, start + window - bwidth); x++) {
        result.get_value(start + x + area.xmin, y, 0) = selector(temp[x], temp[x + window - 1]);
      }
    }
  }

  /* Second pass, vertical dilate/erode. */
  for (int x = 0; x < bwidth; x++) {
    for (int y = 0; y < bheight + 5 * half_window; y++) {
      buf[y] = compare_min_value;
    }
    for (int y = ymin; y < ymax; y++) {
      buf[y - area.ymin + window - 1] = result.get_value(x + area.xmin, y, 0);
    }

    for (int i = 0; i < (bheight + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (int y = 1; y < window; y++) {
        temp[window - 1 - y] = selector(temp[window - y], buf[start - y]);
        temp[window - 1 + y] = selector(temp[window + y - 2], buf[start + y]);
      }

      start = half_window + (i - 1) * window + 1;
      for (int y = -std::min(0, start); y < window - std::max(0, start + window - bheight); y++) {
        result.get_value(x + area.xmin, y + start + area.ymin, 0) = selector(temp[y],
                                                                             temp[y + window - 1]);
      }
    }
  }

  MEM_freeN(temp);
  MEM_freeN(buf);

  output->copy_from(&result, area);
}

struct Max2Selector {
  float operator()(float f1, float f2) const
  {
    return std::max(f1, f2);
  }
};

void DilateStepOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  step_update_memory_buffer<Max2Selector>(output, inputs[0], area, iterations_, -FLT_MAX);
}

ErodeStepOperation::ErodeStepOperation() : DilateStepOperation()
{
  /* pass */
}

struct Min2Selector {
  float operator()(float f1, float f2) const
  {
    return std::min(f1, f2);
  }
};

void ErodeStepOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  step_update_memory_buffer<Min2Selector>(output, inputs[0], area, iterations_, FLT_MAX);
}

}  // namespace blender::compositor
