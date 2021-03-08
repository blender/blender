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

#include "COM_MapUVOperation.h"
#include "BLI_math.h"

MapUVOperation::MapUVOperation()
{
  this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
  this->addInputSocket(COM_DT_VECTOR);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_alpha = 0.0f;
  this->setComplex(true);
  setResolutionInputSocketIndex(1);

  this->m_inputUVProgram = nullptr;
  this->m_inputColorProgram = nullptr;
}

void MapUVOperation::initExecution()
{
  this->m_inputColorProgram = this->getInputSocketReader(0);
  this->m_inputUVProgram = this->getInputSocketReader(1);
}

void MapUVOperation::executePixelSampled(float output[4],
                                         float x,
                                         float y,
                                         PixelSampler /*sampler*/)
{
  float xy[2] = {x, y};
  float uv[2], deriv[2][2], alpha;

  pixelTransform(xy, uv, deriv, alpha);
  if (alpha == 0.0f) {
    zero_v4(output);
    return;
  }

  /* EWA filtering */
  this->m_inputColorProgram->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1]);

  /* UV to alpha threshold */
  const float threshold = this->m_alpha * 0.05f;
  /* XXX alpha threshold is used to fade out pixels on boundaries with invalid derivatives.
   * this calculation is not very well defined, should be looked into if it becomes a problem ...
   */
  float du = len_v2(deriv[0]);
  float dv = len_v2(deriv[1]);
  float factor = 1.0f - threshold * (du / m_inputColorProgram->getWidth() +
                                     dv / m_inputColorProgram->getHeight());
  if (factor < 0.0f) {
    alpha = 0.0f;
  }
  else {
    alpha *= factor;
  }

  /* "premul" */
  if (alpha < 1.0f) {
    mul_v4_fl(output, alpha);
  }
}

bool MapUVOperation::read_uv(float x, float y, float &r_u, float &r_v, float &r_alpha)
{
  float width = m_inputUVProgram->getWidth();
  float height = m_inputUVProgram->getHeight();
  if (x < 0.0f || x >= width || y < 0.0f || y >= height) {
    r_u = 0.0f;
    r_v = 0.0f;
    r_alpha = 0.0f;
    return false;
  }

  float vector[3];
  m_inputUVProgram->readSampled(vector, x, y, COM_PS_BILINEAR);
  r_u = vector[0] * m_inputColorProgram->getWidth();
  r_v = vector[1] * m_inputColorProgram->getHeight();
  r_alpha = vector[2];
  return true;
}

void MapUVOperation::pixelTransform(const float xy[2],
                                    float r_uv[2],
                                    float r_deriv[2][2],
                                    float &r_alpha)
{
  float uv[2], alpha; /* temporary variables for derivative estimation */
  int num;

  read_uv(xy[0], xy[1], r_uv[0], r_uv[1], r_alpha);

  /* Estimate partial derivatives using 1-pixel offsets */
  const float epsilon[2] = {1.0f, 1.0f};

  zero_v2(r_deriv[0]);
  zero_v2(r_deriv[1]);

  num = 0;
  if (read_uv(xy[0] + epsilon[0], xy[1], uv[0], uv[1], alpha)) {
    r_deriv[0][0] += uv[0] - r_uv[0];
    r_deriv[1][0] += uv[1] - r_uv[1];
    num++;
  }
  if (read_uv(xy[0] - epsilon[0], xy[1], uv[0], uv[1], alpha)) {
    r_deriv[0][0] += r_uv[0] - uv[0];
    r_deriv[1][0] += r_uv[1] - uv[1];
    num++;
  }
  if (num > 0) {
    float numinv = 1.0f / (float)num;
    r_deriv[0][0] *= numinv;
    r_deriv[1][0] *= numinv;
  }

  num = 0;
  if (read_uv(xy[0], xy[1] + epsilon[1], uv[0], uv[1], alpha)) {
    r_deriv[0][1] += uv[0] - r_uv[0];
    r_deriv[1][1] += uv[1] - r_uv[1];
    num++;
  }
  if (read_uv(xy[0], xy[1] - epsilon[1], uv[0], uv[1], alpha)) {
    r_deriv[0][1] += r_uv[0] - uv[0];
    r_deriv[1][1] += r_uv[1] - uv[1];
    num++;
  }
  if (num > 0) {
    float numinv = 1.0f / (float)num;
    r_deriv[0][1] *= numinv;
    r_deriv[1][1] *= numinv;
  }
}

void MapUVOperation::deinitExecution()
{
  this->m_inputUVProgram = nullptr;
  this->m_inputColorProgram = nullptr;
}

bool MapUVOperation::determineDependingAreaOfInterest(rcti *input,
                                                      ReadBufferOperation *readOperation,
                                                      rcti *output)
{
  rcti colorInput;
  rcti uvInput;
  NodeOperation *operation = nullptr;

  /* the uv buffer only needs a 3x3 buffer. The image needs whole buffer */

  operation = getInputOperation(0);
  colorInput.xmax = operation->getWidth();
  colorInput.xmin = 0;
  colorInput.ymax = operation->getHeight();
  colorInput.ymin = 0;
  if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
    return true;
  }

  operation = getInputOperation(1);
  uvInput.xmax = input->xmax + 1;
  uvInput.xmin = input->xmin - 1;
  uvInput.ymax = input->ymax + 1;
  uvInput.ymin = input->ymin - 1;
  if (operation->determineDependingAreaOfInterest(&uvInput, readOperation, output)) {
    return true;
  }

  return false;
}
