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

#include "COM_ConvolutionEdgeFilterOperation.h"
#include "BLI_math.h"

ConvolutionEdgeFilterOperation::ConvolutionEdgeFilterOperation()
{
  /* pass */
}

void ConvolutionEdgeFilterOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  float in1[4], in2[4], res1[4] = {0.0}, res2[4] = {0.0};

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
  float mval = 1.0f - value[0];

  this->m_inputOperation->read(in1, x1, y1, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[0]);
  madd_v3_v3fl(res2, in1, this->m_filter[0]);

  this->m_inputOperation->read(in1, x2, y1, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[1]);
  madd_v3_v3fl(res2, in1, this->m_filter[3]);

  this->m_inputOperation->read(in1, x3, y1, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[2]);
  madd_v3_v3fl(res2, in1, this->m_filter[6]);

  this->m_inputOperation->read(in1, x1, y2, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[3]);
  madd_v3_v3fl(res2, in1, this->m_filter[1]);

  this->m_inputOperation->read(in2, x2, y2, nullptr);
  madd_v3_v3fl(res1, in2, this->m_filter[4]);
  madd_v3_v3fl(res2, in2, this->m_filter[4]);

  this->m_inputOperation->read(in1, x3, y2, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[5]);
  madd_v3_v3fl(res2, in1, this->m_filter[7]);

  this->m_inputOperation->read(in1, x1, y3, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[6]);
  madd_v3_v3fl(res2, in1, this->m_filter[2]);

  this->m_inputOperation->read(in1, x2, y3, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[7]);
  madd_v3_v3fl(res2, in1, this->m_filter[5]);

  this->m_inputOperation->read(in1, x3, y3, nullptr);
  madd_v3_v3fl(res1, in1, this->m_filter[8]);
  madd_v3_v3fl(res2, in1, this->m_filter[8]);

  output[0] = sqrt(res1[0] * res1[0] + res2[0] * res2[0]);
  output[1] = sqrt(res1[1] * res1[1] + res2[1] * res2[1]);
  output[2] = sqrt(res1[2] * res1[2] + res2[2] * res2[2]);

  output[0] = output[0] * value[0] + in2[0] * mval;
  output[1] = output[1] * value[0] + in2[1] * mval;
  output[2] = output[2] * value[0] + in2[2] * mval;

  output[3] = in2[3];

  /* Make sure we don't return negative color. */
  output[0] = MAX2(output[0], 0.0f);
  output[1] = MAX2(output[1], 0.0f);
  output[2] = MAX2(output[2], 0.0f);
  output[3] = MAX2(output[3], 0.0f);
}
