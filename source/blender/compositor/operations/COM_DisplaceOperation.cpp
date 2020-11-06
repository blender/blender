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

#include "COM_DisplaceOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

DisplaceOperation::DisplaceOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VECTOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setComplex(true);

  this->m_inputColorProgram = nullptr;
  this->m_inputVectorProgram = nullptr;
  this->m_inputScaleXProgram = nullptr;
  this->m_inputScaleYProgram = nullptr;
}

void DisplaceOperation::initExecution()
{
  this->m_inputColorProgram = this->getInputSocketReader(0);
  this->m_inputVectorProgram = this->getInputSocketReader(1);
  this->m_inputScaleXProgram = this->getInputSocketReader(2);
  this->m_inputScaleYProgram = this->getInputSocketReader(3);

  this->m_width_x4 = this->getWidth() * 4;
  this->m_height_x4 = this->getHeight() * 4;
}

void DisplaceOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler /*sampler*/)
{
  float xy[2] = {x, y};
  float uv[2], deriv[2][2];

  pixelTransform(xy, uv, deriv);
  if (is_zero_v2(deriv[0]) && is_zero_v2(deriv[1])) {
    this->m_inputColorProgram->readSampled(output, uv[0], uv[1], COM_PS_BILINEAR);
  }
  else {
    /* EWA filtering (without nearest it gets blurry with NO distortion) */
    this->m_inputColorProgram->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1]);
  }
}

bool DisplaceOperation::read_displacement(
    float x, float y, float xscale, float yscale, const float origin[2], float &r_u, float &r_v)
{
  float width = m_inputVectorProgram->getWidth();
  float height = m_inputVectorProgram->getHeight();
  if (x < 0.0f || x >= width || y < 0.0f || y >= height) {
    r_u = 0.0f;
    r_v = 0.0f;
    return false;
  }

  float col[4];
  m_inputVectorProgram->readSampled(col, x, y, COM_PS_BILINEAR);
  r_u = origin[0] - col[0] * xscale;
  r_v = origin[1] - col[1] * yscale;
  return true;
}

void DisplaceOperation::pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2])
{
  float col[4];
  float uv[2]; /* temporary variables for derivative estimation */
  int num;

  m_inputScaleXProgram->readSampled(col, xy[0], xy[1], COM_PS_NEAREST);
  float xs = col[0];
  m_inputScaleYProgram->readSampled(col, xy[0], xy[1], COM_PS_NEAREST);
  float ys = col[0];
  /* clamp x and y displacement to triple image resolution -
   * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
  CLAMP(xs, -m_width_x4, m_width_x4);
  CLAMP(ys, -m_height_x4, m_height_x4);

  /* displaced pixel in uv coords, for image sampling */
  read_displacement(xy[0], xy[1], xs, ys, xy, r_uv[0], r_uv[1]);

  /* Estimate partial derivatives using 1-pixel offsets */
  const float epsilon[2] = {1.0f, 1.0f};

  zero_v2(r_deriv[0]);
  zero_v2(r_deriv[1]);

  num = 0;
  if (read_displacement(xy[0] + epsilon[0], xy[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][0] += uv[0] - r_uv[0];
    r_deriv[1][0] += uv[1] - r_uv[1];
    num++;
  }
  if (read_displacement(xy[0] - epsilon[0], xy[1], xs, ys, xy, uv[0], uv[1])) {
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
  if (read_displacement(xy[0], xy[1] + epsilon[1], xs, ys, xy, uv[0], uv[1])) {
    r_deriv[0][1] += uv[0] - r_uv[0];
    r_deriv[1][1] += uv[1] - r_uv[1];
    num++;
  }
  if (read_displacement(xy[0], xy[1] - epsilon[1], xs, ys, xy, uv[0], uv[1])) {
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

void DisplaceOperation::deinitExecution()
{
  this->m_inputColorProgram = nullptr;
  this->m_inputVectorProgram = nullptr;
  this->m_inputScaleXProgram = nullptr;
  this->m_inputScaleYProgram = nullptr;
}

bool DisplaceOperation::determineDependingAreaOfInterest(rcti *input,
                                                         ReadBufferOperation *readOperation,
                                                         rcti *output)
{
  rcti colorInput;
  rcti vectorInput;
  NodeOperation *operation = nullptr;

  /* the vector buffer only needs a 2x2 buffer. The image needs whole buffer */
  /* image */
  operation = getInputOperation(0);
  colorInput.xmax = operation->getWidth();
  colorInput.xmin = 0;
  colorInput.ymax = operation->getHeight();
  colorInput.ymin = 0;
  if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
    return true;
  }

  /* vector */
  operation = getInputOperation(1);
  vectorInput.xmax = input->xmax + 1;
  vectorInput.xmin = input->xmin - 1;
  vectorInput.ymax = input->ymax + 1;
  vectorInput.ymin = input->ymin - 1;
  if (operation->determineDependingAreaOfInterest(&vectorInput, readOperation, output)) {
    return true;
  }

  /* scale x */
  operation = getInputOperation(2);
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
    return true;
  }

  /* scale y */
  operation = getInputOperation(3);
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
    return true;
  }

  return false;
}
