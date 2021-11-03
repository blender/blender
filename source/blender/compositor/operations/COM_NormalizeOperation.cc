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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_NormalizeOperation.h"

namespace blender::compositor {

NormalizeOperation::NormalizeOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  image_reader_ = nullptr;
  cached_instance_ = nullptr;
  flags_.complex = true;
  flags_.can_be_constant = true;
}
void NormalizeOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
  NodeOperation::init_mutex();
}

void NormalizeOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  /* using generic two floats struct to store `x: min`, `y: multiply` */
  NodeTwoFloats *minmult = (NodeTwoFloats *)data;

  image_reader_->read(output, x, y, nullptr);

  output[0] = (output[0] - minmult->x) * minmult->y;

  /* clamp infinities */
  if (output[0] > 1.0f) {
    output[0] = 1.0f;
  }
  else if (output[0] < 0.0f) {
    output[0] = 0.0f;
  }
}

void NormalizeOperation::deinit_execution()
{
  image_reader_ = nullptr;
  delete cached_instance_;
  cached_instance_ = nullptr;
  NodeOperation::deinit_mutex();
}

bool NormalizeOperation::determine_depending_area_of_interest(rcti * /*input*/,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti image_input;
  if (cached_instance_) {
    return false;
  }

  NodeOperation *operation = get_input_operation(0);
  image_input.xmax = operation->get_width();
  image_input.xmin = 0;
  image_input.ymax = operation->get_height();
  image_input.ymin = 0;

  if (operation->determine_depending_area_of_interest(&image_input, read_operation, output)) {
    return true;
  }
  return false;
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel
 */
#define BLENDER_ZMAX 10000.0f

void *NormalizeOperation::initialize_tile_data(rcti *rect)
{
  lock_mutex();
  if (cached_instance_ == nullptr) {
    MemoryBuffer *tile = (MemoryBuffer *)image_reader_->initialize_tile_data(rect);
    /* using generic two floats struct to store `x: min`, `y: multiply`. */
    NodeTwoFloats *minmult = new NodeTwoFloats();

    float *buffer = tile->get_buffer();
    int p = tile->get_width() * tile->get_height();
    float *bc = buffer;

    float minv = 1.0f + BLENDER_ZMAX;
    float maxv = -1.0f - BLENDER_ZMAX;

    float value;
    while (p--) {
      value = bc[0];
      if ((value > maxv) && (value <= BLENDER_ZMAX)) {
        maxv = value;
      }
      if ((value < minv) && (value >= -BLENDER_ZMAX)) {
        minv = value;
      }
      bc++;
    }

    minmult->x = minv;
    /* The rare case of flat buffer  would cause a divide by 0 */
    minmult->y = ((maxv != minv) ? 1.0f / (maxv - minv) : 0.0f);

    cached_instance_ = minmult;
  }

  unlock_mutex();
  return cached_instance_;
}

void NormalizeOperation::deinitialize_tile_data(rcti * /*rect*/, void * /*data*/)
{
  /* pass */
}

void NormalizeOperation::get_area_of_interest(const int UNUSED(input_idx),
                                              const rcti &UNUSED(output_area),
                                              rcti &r_input_area)
{
  r_input_area = get_input_operation(0)->get_canvas();
}

void NormalizeOperation::update_memory_buffer_started(MemoryBuffer *UNUSED(output),
                                                      const rcti &UNUSED(area),
                                                      Span<MemoryBuffer *> inputs)
{
  if (cached_instance_ == nullptr) {
    MemoryBuffer *input = inputs[0];

    /* Using generic two floats struct to store `x: min`, `y: multiply`. */
    NodeTwoFloats *minmult = new NodeTwoFloats();

    float minv = 1.0f + BLENDER_ZMAX;
    float maxv = -1.0f - BLENDER_ZMAX;
    for (const float *elem : input->as_range()) {
      const float value = *elem;
      if ((value > maxv) && (value <= BLENDER_ZMAX)) {
        maxv = value;
      }
      if ((value < minv) && (value >= -BLENDER_ZMAX)) {
        minv = value;
      }
    }

    minmult->x = minv;
    /* The case of a flat buffer would cause a divide by 0. */
    minmult->y = ((maxv != minv) ? 1.0f / (maxv - minv) : 0.0f);

    cached_instance_ = minmult;
  }
}

void NormalizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  NodeTwoFloats *minmult = cached_instance_;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float input_value = *it.in(0);

    *it.out = (input_value - minmult->x) * minmult->y;

    /* Clamp infinities. */
    CLAMP(*it.out, 0.0f, 1.0f);
  }
}

}  // namespace blender::compositor
