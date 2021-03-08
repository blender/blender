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

#include "MEM_guardedalloc.h"

#include "COM_DespeckleOperation.h"

#include "BLI_utildefines.h"

DespeckleOperation::DespeckleOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = nullptr;
  this->setComplex(true);
}
void DespeckleOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_inputValueOperation = this->getInputSocketReader(1);
}

void DespeckleOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
  this->m_inputValueOperation = nullptr;
}

BLI_INLINE int color_diff(const float a[3], const float b[3], const float threshold)
{
  return ((fabsf(a[0] - b[0]) > threshold) || (fabsf(a[1] - b[1]) > threshold) ||
          (fabsf(a[2] - b[2]) > threshold));
}

void DespeckleOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  float w = 0.0f;
  float color_org[4];
  float color_mid[4];
  float color_mid_ok[4];
  float in1[4];
  int x1 = x - 1;
  int x2 = x;
  int x3 = x + 1;
  int y1 = y - 1;
  int y2 = y;
  int y3 = y + 1;
  CLAMP(x1, 0, getWidth() - 1);
  CLAMP(x2, 0, getWidth() - 1);
  CLAMP(x3, 0, getWidth() - 1);
  CLAMP(y1, 0, getHeight() - 1);
  CLAMP(y2, 0, getHeight() - 1);
  CLAMP(y3, 0, getHeight() - 1);
  float value[4];
  this->m_inputValueOperation->read(value, x2, y2, nullptr);
  // const float mval = 1.0f - value[0];

  this->m_inputOperation->read(color_org, x2, y2, nullptr);

#define TOT_DIV_ONE 1.0f
#define TOT_DIV_CNR (float)M_SQRT1_2

#define WTOT (TOT_DIV_ONE * 4 + TOT_DIV_CNR * 4)

#define COLOR_ADD(fac) \
  { \
    madd_v4_v4fl(color_mid, in1, fac); \
    if (color_diff(in1, color_org, this->m_threshold)) { \
      w += fac; \
      madd_v4_v4fl(color_mid_ok, in1, fac); \
    } \
  }

  zero_v4(color_mid);
  zero_v4(color_mid_ok);

  this->m_inputOperation->read(in1, x1, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  this->m_inputOperation->read(in1, x2, y1, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  this->m_inputOperation->read(in1, x3, y1, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  this->m_inputOperation->read(in1, x1, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)

#if 0
  this->m_inputOperation->read(in2, x2, y2, NULL);
  madd_v4_v4fl(color_mid, in2, this->m_filter[4]);
#endif

  this->m_inputOperation->read(in1, x3, y2, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  this->m_inputOperation->read(in1, x1, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)
  this->m_inputOperation->read(in1, x2, y3, nullptr);
  COLOR_ADD(TOT_DIV_ONE)
  this->m_inputOperation->read(in1, x3, y3, nullptr);
  COLOR_ADD(TOT_DIV_CNR)

  mul_v4_fl(color_mid, 1.0f / (4.0f + (4.0f * (float)M_SQRT1_2)));
  // mul_v4_fl(color_mid, 1.0f / w);

  if ((w != 0.0f) && ((w / WTOT) > (this->m_threshold_neighbor)) &&
      color_diff(color_mid, color_org, this->m_threshold)) {
    mul_v4_fl(color_mid_ok, 1.0f / w);
    interp_v4_v4v4(output, color_org, color_mid_ok, value[0]);
  }
  else {
    copy_v4_v4(output, color_org);
  }
}

bool DespeckleOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;
  int addx = 2;  //(this->m_filterWidth - 1) / 2 + 1;
  int addy = 2;  //(this->m_filterHeight - 1) / 2 + 1;
  newInput.xmax = input->xmax + addx;
  newInput.xmin = input->xmin - addx;
  newInput.ymax = input->ymax + addy;
  newInput.ymin = input->ymin - addy;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
