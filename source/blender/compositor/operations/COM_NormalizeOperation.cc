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
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  m_imageReader = nullptr;
  m_cachedInstance = nullptr;
  this->flags.complex = true;
  flags.can_be_constant = true;
}
void NormalizeOperation::initExecution()
{
  m_imageReader = this->getInputSocketReader(0);
  NodeOperation::initMutex();
}

void NormalizeOperation::executePixel(float output[4], int x, int y, void *data)
{
  /* using generic two floats struct to store `x: min`, `y: multiply` */
  NodeTwoFloats *minmult = (NodeTwoFloats *)data;

  m_imageReader->read(output, x, y, nullptr);

  output[0] = (output[0] - minmult->x) * minmult->y;

  /* clamp infinities */
  if (output[0] > 1.0f) {
    output[0] = 1.0f;
  }
  else if (output[0] < 0.0f) {
    output[0] = 0.0f;
  }
}

void NormalizeOperation::deinitExecution()
{
  m_imageReader = nullptr;
  delete m_cachedInstance;
  m_cachedInstance = nullptr;
  NodeOperation::deinitMutex();
}

bool NormalizeOperation::determineDependingAreaOfInterest(rcti * /*input*/,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti imageInput;
  if (m_cachedInstance) {
    return false;
  }

  NodeOperation *operation = getInputOperation(0);
  imageInput.xmax = operation->getWidth();
  imageInput.xmin = 0;
  imageInput.ymax = operation->getHeight();
  imageInput.ymin = 0;

  if (operation->determineDependingAreaOfInterest(&imageInput, readOperation, output)) {
    return true;
  }
  return false;
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel
 */
#define BLENDER_ZMAX 10000.0f

void *NormalizeOperation::initializeTileData(rcti *rect)
{
  lockMutex();
  if (m_cachedInstance == nullptr) {
    MemoryBuffer *tile = (MemoryBuffer *)m_imageReader->initializeTileData(rect);
    /* using generic two floats struct to store `x: min`, `y: multiply`. */
    NodeTwoFloats *minmult = new NodeTwoFloats();

    float *buffer = tile->getBuffer();
    int p = tile->getWidth() * tile->getHeight();
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

    m_cachedInstance = minmult;
  }

  unlockMutex();
  return m_cachedInstance;
}

void NormalizeOperation::deinitializeTileData(rcti * /*rect*/, void * /*data*/)
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
  if (m_cachedInstance == nullptr) {
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

    m_cachedInstance = minmult;
  }
}

void NormalizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  NodeTwoFloats *minmult = m_cachedInstance;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float input_value = *it.in(0);

    *it.out = (input_value - minmult->x) * minmult->y;

    /* Clamp infinities. */
    CLAMP(*it.out, 0.0f, 1.0f);
  }
}

}  // namespace blender::compositor
