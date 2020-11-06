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

#include "COM_ColorCurveOperation.h"

#include "BKE_colortools.h"

#include "MEM_guardedalloc.h"

ColorCurveOperation::ColorCurveOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);

  this->m_inputFacProgram = nullptr;
  this->m_inputImageProgram = nullptr;
  this->m_inputBlackProgram = nullptr;
  this->m_inputWhiteProgram = nullptr;

  this->setResolutionInputSocketIndex(1);
}
void ColorCurveOperation::initExecution()
{
  CurveBaseOperation::initExecution();
  this->m_inputFacProgram = this->getInputSocketReader(0);
  this->m_inputImageProgram = this->getInputSocketReader(1);
  this->m_inputBlackProgram = this->getInputSocketReader(2);
  this->m_inputWhiteProgram = this->getInputSocketReader(3);

  BKE_curvemapping_premultiply(this->m_curveMapping, 0);
}

void ColorCurveOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  CurveMapping *cumap = this->m_curveMapping;

  float fac[4];
  float image[4];

  /* local versions of cumap->black, cumap->white, cumap->bwmul */
  float black[4];
  float white[4];
  float bwmul[3];

  this->m_inputBlackProgram->readSampled(black, x, y, sampler);
  this->m_inputWhiteProgram->readSampled(white, x, y, sampler);

  /* get our own local bwmul value,
   * since we can't be threadsafe and use cumap->bwmul & friends */
  BKE_curvemapping_set_black_white_ex(black, white, bwmul);

  this->m_inputFacProgram->readSampled(fac, x, y, sampler);
  this->m_inputImageProgram->readSampled(image, x, y, sampler);

  if (*fac >= 1.0f) {
    BKE_curvemapping_evaluate_premulRGBF_ex(cumap, output, image, black, bwmul);
  }
  else if (*fac <= 0.0f) {
    copy_v3_v3(output, image);
  }
  else {
    float col[4];
    BKE_curvemapping_evaluate_premulRGBF_ex(cumap, col, image, black, bwmul);
    interp_v3_v3v3(output, image, col, *fac);
  }
  output[3] = image[3];
}

void ColorCurveOperation::deinitExecution()
{
  CurveBaseOperation::deinitExecution();
  this->m_inputFacProgram = nullptr;
  this->m_inputImageProgram = nullptr;
  this->m_inputBlackProgram = nullptr;
  this->m_inputWhiteProgram = nullptr;
}

// Constant level curve mapping

ConstantLevelColorCurveOperation::ConstantLevelColorCurveOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);

  this->m_inputFacProgram = nullptr;
  this->m_inputImageProgram = nullptr;

  this->setResolutionInputSocketIndex(1);
}
void ConstantLevelColorCurveOperation::initExecution()
{
  CurveBaseOperation::initExecution();
  this->m_inputFacProgram = this->getInputSocketReader(0);
  this->m_inputImageProgram = this->getInputSocketReader(1);

  BKE_curvemapping_premultiply(this->m_curveMapping, 0);

  BKE_curvemapping_set_black_white(this->m_curveMapping, this->m_black, this->m_white);
}

void ConstantLevelColorCurveOperation::executePixelSampled(float output[4],
                                                           float x,
                                                           float y,
                                                           PixelSampler sampler)
{
  float fac[4];
  float image[4];

  this->m_inputFacProgram->readSampled(fac, x, y, sampler);
  this->m_inputImageProgram->readSampled(image, x, y, sampler);

  if (*fac >= 1.0f) {
    BKE_curvemapping_evaluate_premulRGBF(this->m_curveMapping, output, image);
  }
  else if (*fac <= 0.0f) {
    copy_v3_v3(output, image);
  }
  else {
    float col[4];
    BKE_curvemapping_evaluate_premulRGBF(this->m_curveMapping, col, image);
    interp_v3_v3v3(output, image, col, *fac);
  }
  output[3] = image[3];
}

void ConstantLevelColorCurveOperation::deinitExecution()
{
  CurveBaseOperation::deinitExecution();
  this->m_inputFacProgram = nullptr;
  this->m_inputImageProgram = nullptr;
}
