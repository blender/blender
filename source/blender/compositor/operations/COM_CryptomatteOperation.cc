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
 * Copyright 2018, Blender Foundation.
 */

#include "COM_CryptomatteOperation.h"

namespace blender::compositor {

CryptomatteOperation::CryptomatteOperation(size_t num_inputs)
{
  inputs.resize(num_inputs);
  for (size_t i = 0; i < num_inputs; i++) {
    this->addInputSocket(DataType::Color);
  }
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;
}

void CryptomatteOperation::initExecution()
{
  for (size_t i = 0; i < inputs.size(); i++) {
    inputs[i] = this->getInputSocketReader(i);
  }
}

void CryptomatteOperation::addObjectIndex(float objectIndex)
{
  if (objectIndex != 0.0f) {
    m_objectIndex.append(objectIndex);
  }
}

void CryptomatteOperation::executePixel(float output[4], int x, int y, void *data)
{
  float input[4];
  output[0] = output[1] = output[2] = output[3] = 0.0f;
  for (size_t i = 0; i < inputs.size(); i++) {
    inputs[i]->read(input, x, y, data);
    if (i == 0) {
      /* Write the front-most object as false color for picking. */
      output[0] = input[0];
      uint32_t m3hash;
      ::memcpy(&m3hash, &input[0], sizeof(uint32_t));
      /* Since the red channel is likely to be out of display range,
       * setting green and blue gives more meaningful images. */
      output[1] = ((float)((m3hash << 8)) / (float)UINT32_MAX);
      output[2] = ((float)((m3hash << 16)) / (float)UINT32_MAX);
    }
    for (float hash : m_objectIndex) {
      if (input[0] == hash) {
        output[3] += input[1];
      }
      if (input[2] == hash) {
        output[3] += input[3];
      }
    }
  }
}

}  // namespace blender::compositor
