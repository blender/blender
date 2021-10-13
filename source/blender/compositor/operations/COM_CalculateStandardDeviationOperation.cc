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

#include "COM_CalculateStandardDeviationOperation.h"

#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.h"

namespace blender::compositor {

void CalculateStandardDeviationOperation::executePixel(float output[4],
                                                       int /*x*/,
                                                       int /*y*/,
                                                       void * /*data*/)
{
  output[0] = standardDeviation_;
}

void *CalculateStandardDeviationOperation::initializeTileData(rcti *rect)
{
  lockMutex();
  if (!iscalculated_) {
    MemoryBuffer *tile = (MemoryBuffer *)imageReader_->initializeTileData(rect);
    CalculateMeanOperation::calculateMean(tile);
    standardDeviation_ = 0.0f;
    float *buffer = tile->getBuffer();
    int size = tile->getWidth() * tile->getHeight();
    int pixels = 0;
    float sum = 0.0f;
    float mean = result_;
    for (int i = 0, offset = 0; i < size; i++, offset += 4) {
      if (buffer[offset + 3] > 0) {
        pixels++;

        switch (setting_) {
          case 1: /* rgb combined */
          {
            float value = IMB_colormanagement_get_luminance(&buffer[offset]);
            sum += (value - mean) * (value - mean);
            break;
          }
          case 2: /* red */
          {
            float value = buffer[offset];
            sum += (value - mean) * (value - mean);
            break;
          }
          case 3: /* green */
          {
            float value = buffer[offset + 1];
            sum += (value - mean) * (value - mean);
            break;
          }
          case 4: /* blue */
          {
            float value = buffer[offset + 2];
            sum += (value - mean) * (value - mean);
            break;
          }
          case 5: /* luminance */
          {
            float yuv[3];
            rgb_to_yuv(buffer[offset],
                       buffer[offset + 1],
                       buffer[offset + 2],
                       &yuv[0],
                       &yuv[1],
                       &yuv[2],
                       BLI_YUV_ITU_BT709);
            sum += (yuv[0] - mean) * (yuv[0] - mean);
            break;
          }
        }
      }
    }
    standardDeviation_ = sqrt(sum / (float)(pixels - 1));
    iscalculated_ = true;
  }
  unlockMutex();
  return nullptr;
}

void CalculateStandardDeviationOperation::update_memory_buffer_started(
    MemoryBuffer *UNUSED(output), const rcti &UNUSED(area), Span<MemoryBuffer *> inputs)
{
  if (!iscalculated_) {
    const MemoryBuffer *input = inputs[0];
    const float mean = CalculateMeanOperation::calc_mean(input);

    PixelsSum total = {0};
    exec_system_->execute_work<PixelsSum>(
        input->get_rect(),
        [=](const rcti &split) { return calc_area_sum(input, split, mean); },
        total,
        [](PixelsSum &join, const PixelsSum &chunk) {
          join.sum += chunk.sum;
          join.num_pixels += chunk.num_pixels;
        });
    standardDeviation_ = total.num_pixels <= 1 ? 0.0f :
                                                 sqrt(total.sum / (float)(total.num_pixels - 1));
    iscalculated_ = true;
  }
}

void CalculateStandardDeviationOperation::update_memory_buffer_partial(
    MemoryBuffer *output, const rcti &area, Span<MemoryBuffer *> UNUSED(inputs))
{
  output->fill(area, &standardDeviation_);
}

using PixelsSum = CalculateMeanOperation::PixelsSum;
PixelsSum CalculateStandardDeviationOperation::calc_area_sum(const MemoryBuffer *input,
                                                             const rcti &area,
                                                             const float mean)
{
  PixelsSum result = {0};
  for (const float *elem : input->get_buffer_area(area)) {
    if (elem[3] <= 0.0f) {
      continue;
    }
    const float value = setting_func_(elem);
    result.sum += (value - mean) * (value - mean);
    result.num_pixels++;
  }
  return result;
}

}  // namespace blender::compositor
