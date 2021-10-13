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

#include "COM_KeyingClipOperation.h"

namespace blender::compositor {

KeyingClipOperation::KeyingClipOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);

  m_kernelRadius = 3;
  m_kernelTolerance = 0.1f;

  m_clipBlack = 0.0f;
  m_clipWhite = 1.0f;

  m_isEdgeMatte = false;

  this->flags.complex = true;
}

void *KeyingClipOperation::initializeTileData(rcti *rect)
{
  void *buffer = getInputOperation(0)->initializeTileData(rect);

  return buffer;
}

void KeyingClipOperation::executePixel(float output[4], int x, int y, void *data)
{
  const int delta = m_kernelRadius;
  const float tolerance = m_kernelTolerance;

  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();

  int bufferWidth = inputBuffer->getWidth();
  int bufferHeight = inputBuffer->getHeight();

  float value = buffer[(y * bufferWidth + x)];

  bool ok = false;
  int start_x = max_ff(0, x - delta + 1), start_y = max_ff(0, y - delta + 1),
      end_x = min_ff(x + delta - 1, bufferWidth - 1),
      end_y = min_ff(y + delta - 1, bufferHeight - 1);

  int count = 0, totalCount = (end_x - start_x + 1) * (end_y - start_y + 1) - 1;
  int thresholdCount = ceil((float)totalCount * 0.9f);

  if (delta == 0) {
    ok = true;
  }

  for (int cx = start_x; ok == false && cx <= end_x; cx++) {
    for (int cy = start_y; ok == false && cy <= end_y; cy++) {
      if (UNLIKELY(cx == x && cy == y)) {
        continue;
      }

      int bufferIndex = (cy * bufferWidth + cx);
      float currentValue = buffer[bufferIndex];

      if (fabsf(currentValue - value) < tolerance) {
        count++;
        if (count >= thresholdCount) {
          ok = true;
        }
      }
    }
  }

  if (m_isEdgeMatte) {
    if (ok) {
      output[0] = 0.0f;
    }
    else {
      output[0] = 1.0f;
    }
  }
  else {
    output[0] = value;

    if (ok) {
      if (output[0] < m_clipBlack) {
        output[0] = 0.0f;
      }
      else if (output[0] >= m_clipWhite) {
        output[0] = 1.0f;
      }
      else {
        output[0] = (output[0] - m_clipBlack) / (m_clipWhite - m_clipBlack);
      }
    }
  }
}

bool KeyingClipOperation::determineDependingAreaOfInterest(rcti *input,
                                                           ReadBufferOperation *readOperation,
                                                           rcti *output)
{
  rcti newInput;

  newInput.xmin = input->xmin - m_kernelRadius;
  newInput.ymin = input->ymin - m_kernelRadius;
  newInput.xmax = input->xmax + m_kernelRadius;
  newInput.ymax = input->ymax + m_kernelRadius;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void KeyingClipOperation::get_area_of_interest(const int input_idx,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - m_kernelRadius;
  r_input_area.xmax = output_area.xmax + m_kernelRadius;
  r_input_area.ymin = output_area.ymin - m_kernelRadius;
  r_input_area.ymax = output_area.ymax + m_kernelRadius;
}

void KeyingClipOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  BuffersIterator<float> it = output->iterate_with(inputs, area);

  const int delta = m_kernelRadius;
  const float tolerance = m_kernelTolerance;
  const int width = this->getWidth();
  const int height = this->getHeight();
  const int row_stride = input->row_stride;
  const int elem_stride = input->elem_stride;
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    const int start_x = MAX2(0, x - delta + 1);
    const int start_y = MAX2(0, y - delta + 1);
    const int end_x = MIN2(x + delta, width);
    const int end_y = MIN2(y + delta, height);
    const int x_len = end_x - start_x;
    const int y_len = end_y - start_y;

    const int total_count = x_len * y_len - 1;
    const int threshold_count = ceil((float)total_count * 0.9f);
    bool ok = false;
    if (delta == 0) {
      ok = true;
    }

    const float *main_elem = it.in(0);
    const float value = *main_elem;
    const float *row = input->get_elem(start_x, start_y);
    const float *end_row = row + y_len * row_stride;
    int count = 0;
    for (; ok == false && row < end_row; row += row_stride) {
      const float *end_elem = row + x_len * elem_stride;
      for (const float *elem = row; ok == false && elem < end_elem; elem += elem_stride) {
        if (UNLIKELY(elem == main_elem)) {
          continue;
        }

        const float current_value = *elem;
        if (fabsf(current_value - value) < tolerance) {
          count++;
          if (count >= threshold_count) {
            ok = true;
          }
        }
      }
    }

    if (m_isEdgeMatte) {
      *it.out = ok ? 0.0f : 1.0f;
    }
    else {
      if (!ok) {
        *it.out = value;
      }
      else if (value < m_clipBlack) {
        *it.out = 0.0f;
      }
      else if (value >= m_clipWhite) {
        *it.out = 1.0f;
      }
      else {
        *it.out = (value - m_clipBlack) / (m_clipWhite - m_clipBlack);
      }
    }
  }
}

}  // namespace blender::compositor
