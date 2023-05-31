/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "COM_InpaintOperation.h"

namespace blender::compositor {

#define ASSERT_XY_RANGE(x, y) \
  BLI_assert(x >= 0 && x < this->get_width() && y >= 0 && y < this->get_height())

InpaintSimpleOperation::InpaintSimpleOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  input_image_program_ = nullptr;
  pixelorder_ = nullptr;
  manhattan_distance_ = nullptr;
  cached_buffer_ = nullptr;
  cached_buffer_ready_ = false;
  flags_.is_fullframe_operation = true;
}
void InpaintSimpleOperation::init_execution()
{
  input_image_program_ = this->get_input_socket_reader(0);

  pixelorder_ = nullptr;
  manhattan_distance_ = nullptr;
  cached_buffer_ = nullptr;
  cached_buffer_ready_ = false;

  this->init_mutex();
}

void InpaintSimpleOperation::clamp_xy(int &x, int &y)
{
  int width = this->get_width();
  int height = this->get_height();

  if (x < 0) {
    x = 0;
  }
  else if (x >= width) {
    x = width - 1;
  }

  if (y < 0) {
    y = 0;
  }
  else if (y >= height) {
    y = height - 1;
  }
}

float *InpaintSimpleOperation::get_pixel(int x, int y)
{
  int width = this->get_width();

  ASSERT_XY_RANGE(x, y);

  return &cached_buffer_[y * width * COM_DATA_TYPE_COLOR_CHANNELS +
                         x * COM_DATA_TYPE_COLOR_CHANNELS];
}

int InpaintSimpleOperation::mdist(int x, int y)
{
  int width = this->get_width();

  ASSERT_XY_RANGE(x, y);

  return manhattan_distance_[y * width + x];
}

bool InpaintSimpleOperation::next_pixel(int &x, int &y, int &curr, int iters)
{
  int width = this->get_width();

  if (curr >= area_size_) {
    return false;
  }

  int r = pixelorder_[curr++];

  x = r % width;
  y = r / width;

  if (this->mdist(x, y) > iters) {
    return false;
  }

  return true;
}

void InpaintSimpleOperation::calc_manhattan_distance()
{
  int width = this->get_width();
  int height = this->get_height();
  short *m = manhattan_distance_ = (short *)MEM_mallocN(sizeof(short) * width * height, __func__);
  int *offsets;

  offsets = (int *)MEM_callocN(sizeof(int) * (width + height + 1),
                               "InpaintSimpleOperation offsets");

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int r = 0;
      /* no need to clamp here */
      if (this->get_pixel(i, j)[3] < 1.0f) {
        r = width + height;
        if (i > 0) {
          r = min_ii(r, m[j * width + i - 1] + 1);
        }
        if (j > 0) {
          r = min_ii(r, m[(j - 1) * width + i] + 1);
        }
      }
      m[j * width + i] = r;
    }
  }

  for (int j = height - 1; j >= 0; j--) {
    for (int i = width - 1; i >= 0; i--) {
      int r = m[j * width + i];

      if (i + 1 < width) {
        r = min_ii(r, m[j * width + i + 1] + 1);
      }
      if (j + 1 < height) {
        r = min_ii(r, m[(j + 1) * width + i] + 1);
      }

      m[j * width + i] = r;

      offsets[r]++;
    }
  }

  offsets[0] = 0;

  for (int i = 1; i < width + height + 1; i++) {
    offsets[i] += offsets[i - 1];
  }

  area_size_ = offsets[width + height];
  pixelorder_ = (int *)MEM_mallocN(sizeof(int) * area_size_, __func__);

  for (int i = 0; i < width * height; i++) {
    if (m[i] > 0) {
      pixelorder_[offsets[m[i] - 1]++] = i;
    }
  }

  MEM_freeN(offsets);
}

void InpaintSimpleOperation::pix_step(int x, int y)
{
  const int d = this->mdist(x, y);
  float pix[3] = {0.0f, 0.0f, 0.0f};
  float pix_divider = 0.0f;

  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      /* changing to both != 0 gives dithering artifacts */
      if (dx != 0 || dy != 0) {
        int x_ofs = x + dx;
        int y_ofs = y + dy;

        this->clamp_xy(x_ofs, y_ofs);

        if (this->mdist(x_ofs, y_ofs) < d) {

          float weight;

          if (dx == 0 || dy == 0) {
            weight = 1.0f;
          }
          else {
            weight = M_SQRT1_2; /* 1.0f / sqrt(2) */
          }

          madd_v3_v3fl(pix, this->get_pixel(x_ofs, y_ofs), weight);
          pix_divider += weight;
        }
      }
    }
  }

  float *output = this->get_pixel(x, y);
  if (pix_divider != 0.0f) {
    mul_v3_fl(pix, 1.0f / pix_divider);
    /* use existing pixels alpha to blend into */
    interp_v3_v3v3(output, pix, output, output[3]);
    output[3] = 1.0f;
  }
}

void *InpaintSimpleOperation::initialize_tile_data(rcti *rect)
{
  if (cached_buffer_ready_) {
    return cached_buffer_;
  }
  lock_mutex();
  if (!cached_buffer_ready_) {
    MemoryBuffer *buf = (MemoryBuffer *)input_image_program_->initialize_tile_data(rect);
    cached_buffer_ = (float *)MEM_dupallocN(buf->get_buffer());

    this->calc_manhattan_distance();

    int curr = 0;
    int x, y;

    while (this->next_pixel(x, y, curr, iterations_)) {
      this->pix_step(x, y);
    }
    cached_buffer_ready_ = true;
  }

  unlock_mutex();
  return cached_buffer_;
}

void InpaintSimpleOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  this->clamp_xy(x, y);
  copy_v4_v4(output, this->get_pixel(x, y));
}

void InpaintSimpleOperation::deinit_execution()
{
  input_image_program_ = nullptr;
  this->deinit_mutex();
  if (cached_buffer_) {
    MEM_freeN(cached_buffer_);
    cached_buffer_ = nullptr;
  }

  if (pixelorder_) {
    MEM_freeN(pixelorder_);
    pixelorder_ = nullptr;
  }

  if (manhattan_distance_) {
    MEM_freeN(manhattan_distance_);
    manhattan_distance_ = nullptr;
  }
  cached_buffer_ready_ = false;
}

bool InpaintSimpleOperation::determine_depending_area_of_interest(
    rcti * /*input*/, ReadBufferOperation *read_operation, rcti *output)
{
  if (cached_buffer_ready_) {
    return false;
  }

  rcti new_input;

  new_input.xmax = get_width();
  new_input.xmin = 0;
  new_input.ymax = get_height();
  new_input.ymin = 0;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void InpaintSimpleOperation::get_area_of_interest(const int input_idx,
                                                  const rcti & /*output_area*/,
                                                  rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area = this->get_canvas();
}

void InpaintSimpleOperation::update_memory_buffer(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> inputs)
{
  /* TODO(manzanilla): once tiled implementation is removed, run multi-threaded where possible. */
  MemoryBuffer *input = inputs[0];
  if (!cached_buffer_ready_) {
    if (input->is_a_single_elem()) {
      MemoryBuffer *tmp = input->inflate();
      cached_buffer_ = tmp->release_ownership_buffer();
      delete tmp;
    }
    else {
      cached_buffer_ = (float *)MEM_dupallocN(input->get_buffer());
    }

    this->calc_manhattan_distance();

    int curr = 0;
    int x, y;
    while (this->next_pixel(x, y, curr, iterations_)) {
      this->pix_step(x, y);
    }
    cached_buffer_ready_ = true;
  }

  const int num_channels = COM_data_type_num_channels(get_output_socket()->get_data_type());
  MemoryBuffer buf(cached_buffer_, num_channels, input->get_width(), input->get_height());
  output->copy_from(&buf, area);
}

}  // namespace blender::compositor
