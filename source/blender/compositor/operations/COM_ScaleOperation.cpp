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

#include "COM_ScaleOperation.h"

#define USE_FORCE_BILINEAR
/* XXX - ignore input and use default from old compositor,
 * could become an option like the transform node - campbell
 *
 * note: use bilinear because bicubic makes fuzzy even when not scaling at all (1:1)
 */

BaseScaleOperation::BaseScaleOperation()
{
#ifdef USE_FORCE_BILINEAR
  m_sampler = (int)COM_PS_BILINEAR;
#else
  m_sampler = -1;
#endif
  m_variable_size = false;
}

ScaleOperation::ScaleOperation() : BaseScaleOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = NULL;
  this->m_inputXOperation = NULL;
  this->m_inputYOperation = NULL;
}
void ScaleOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_inputXOperation = this->getInputSocketReader(1);
  this->m_inputYOperation = this->getInputSocketReader(2);
  this->m_centerX = this->getWidth() / 2.0;
  this->m_centerY = this->getHeight() / 2.0;
}

void ScaleOperation::deinitExecution()
{
  this->m_inputOperation = NULL;
  this->m_inputXOperation = NULL;
  this->m_inputYOperation = NULL;
}

void ScaleOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  PixelSampler effective_sampler = getEffectiveSampler(sampler);

  float scaleX[4];
  float scaleY[4];

  this->m_inputXOperation->readSampled(scaleX, x, y, effective_sampler);
  this->m_inputYOperation->readSampled(scaleY, x, y, effective_sampler);

  const float scx = scaleX[0];
  const float scy = scaleY[0];

  float nx = this->m_centerX + (x - this->m_centerX) / scx;
  float ny = this->m_centerY + (y - this->m_centerY) / scy;
  this->m_inputOperation->readSampled(output, nx, ny, effective_sampler);
}

bool ScaleOperation::determineDependingAreaOfInterest(rcti *input,
                                                      ReadBufferOperation *readOperation,
                                                      rcti *output)
{
  rcti newInput;
  if (!m_variable_size) {
    float scaleX[4];
    float scaleY[4];

    this->m_inputXOperation->readSampled(scaleX, 0, 0, COM_PS_NEAREST);
    this->m_inputYOperation->readSampled(scaleY, 0, 0, COM_PS_NEAREST);

    const float scx = scaleX[0];
    const float scy = scaleY[0];

    newInput.xmax = this->m_centerX + (input->xmax - this->m_centerX) / scx;
    newInput.xmin = this->m_centerX + (input->xmin - this->m_centerX) / scx;
    newInput.ymax = this->m_centerY + (input->ymax - this->m_centerY) / scy;
    newInput.ymin = this->m_centerY + (input->ymin - this->m_centerY) / scy;
  }
  else {
    newInput.xmax = this->getWidth();
    newInput.xmin = 0;
    newInput.ymax = this->getHeight();
    newInput.ymin = 0;
  }
  return BaseScaleOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

// SCALE ABSOLUTE
ScaleAbsoluteOperation::ScaleAbsoluteOperation() : BaseScaleOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = NULL;
  this->m_inputXOperation = NULL;
  this->m_inputYOperation = NULL;
}
void ScaleAbsoluteOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_inputXOperation = this->getInputSocketReader(1);
  this->m_inputYOperation = this->getInputSocketReader(2);
  this->m_centerX = this->getWidth() / 2.0;
  this->m_centerY = this->getHeight() / 2.0;
}

void ScaleAbsoluteOperation::deinitExecution()
{
  this->m_inputOperation = NULL;
  this->m_inputXOperation = NULL;
  this->m_inputYOperation = NULL;
}

void ScaleAbsoluteOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  PixelSampler effective_sampler = getEffectiveSampler(sampler);

  float scaleX[4];
  float scaleY[4];

  this->m_inputXOperation->readSampled(scaleX, x, y, effective_sampler);
  this->m_inputYOperation->readSampled(scaleY, x, y, effective_sampler);

  const float scx = scaleX[0];  // target absolute scale
  const float scy = scaleY[0];  // target absolute scale

  const float width = this->getWidth();
  const float height = this->getHeight();
  // div
  float relativeXScale = scx / width;
  float relativeYScale = scy / height;

  float nx = this->m_centerX + (x - this->m_centerX) / relativeXScale;
  float ny = this->m_centerY + (y - this->m_centerY) / relativeYScale;

  this->m_inputOperation->readSampled(output, nx, ny, effective_sampler);
}

bool ScaleAbsoluteOperation::determineDependingAreaOfInterest(rcti *input,
                                                              ReadBufferOperation *readOperation,
                                                              rcti *output)
{
  rcti newInput;
  if (!m_variable_size) {
    float scaleX[4];
    float scaleY[4];

    this->m_inputXOperation->readSampled(scaleX, 0, 0, COM_PS_NEAREST);
    this->m_inputYOperation->readSampled(scaleY, 0, 0, COM_PS_NEAREST);

    const float scx = scaleX[0];
    const float scy = scaleY[0];
    const float width = this->getWidth();
    const float height = this->getHeight();
    // div
    float relateveXScale = scx / width;
    float relateveYScale = scy / height;

    newInput.xmax = this->m_centerX + (input->xmax - this->m_centerX) / relateveXScale;
    newInput.xmin = this->m_centerX + (input->xmin - this->m_centerX) / relateveXScale;
    newInput.ymax = this->m_centerY + (input->ymax - this->m_centerY) / relateveYScale;
    newInput.ymin = this->m_centerY + (input->ymin - this->m_centerY) / relateveYScale;
  }
  else {
    newInput.xmax = this->getWidth();
    newInput.xmin = 0;
    newInput.ymax = this->getHeight();
    newInput.ymin = 0;
  }

  return BaseScaleOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

// Absolute fixed size
ScaleFixedSizeOperation::ScaleFixedSizeOperation() : BaseScaleOperation()
{
  this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = NULL;
  this->m_is_offset = false;
}
void ScaleFixedSizeOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_relX = this->m_inputOperation->getWidth() / (float)this->m_newWidth;
  this->m_relY = this->m_inputOperation->getHeight() / (float)this->m_newHeight;

  /* *** all the options below are for a fairly special case - camera framing *** */
  if (this->m_offsetX != 0.0f || this->m_offsetY != 0.0f) {
    this->m_is_offset = true;

    if (this->m_newWidth > this->m_newHeight) {
      this->m_offsetX *= this->m_newWidth;
      this->m_offsetY *= this->m_newWidth;
    }
    else {
      this->m_offsetX *= this->m_newHeight;
      this->m_offsetY *= this->m_newHeight;
    }
  }

  if (this->m_is_aspect) {
    /* apply aspect from clip */
    const float w_src = this->m_inputOperation->getWidth();
    const float h_src = this->m_inputOperation->getHeight();

    /* destination aspect is already applied from the camera frame */
    const float w_dst = this->m_newWidth;
    const float h_dst = this->m_newHeight;

    const float asp_src = w_src / h_src;
    const float asp_dst = w_dst / h_dst;

    if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
      if ((asp_src > asp_dst) == (this->m_is_crop == true)) {
        /* fit X */
        const float div = asp_src / asp_dst;
        this->m_relX /= div;
        this->m_offsetX += ((w_src - (w_src * div)) / (w_src / w_dst)) / 2.0f;
      }
      else {
        /* fit Y */
        const float div = asp_dst / asp_src;
        this->m_relY /= div;
        this->m_offsetY += ((h_src - (h_src * div)) / (h_src / h_dst)) / 2.0f;
      }

      this->m_is_offset = true;
    }
  }
  /* *** end framing options *** */
}

void ScaleFixedSizeOperation::deinitExecution()
{
  this->m_inputOperation = NULL;
}

void ScaleFixedSizeOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  PixelSampler effective_sampler = getEffectiveSampler(sampler);

  if (this->m_is_offset) {
    float nx = ((x - this->m_offsetX) * this->m_relX);
    float ny = ((y - this->m_offsetY) * this->m_relY);
    this->m_inputOperation->readSampled(output, nx, ny, effective_sampler);
  }
  else {
    this->m_inputOperation->readSampled(
        output, x * this->m_relX, y * this->m_relY, effective_sampler);
  }
}

bool ScaleFixedSizeOperation::determineDependingAreaOfInterest(rcti *input,
                                                               ReadBufferOperation *readOperation,
                                                               rcti *output)
{
  rcti newInput;

  newInput.xmax = (input->xmax - m_offsetX) * this->m_relX + 1;
  newInput.xmin = (input->xmin - m_offsetX) * this->m_relX;
  newInput.ymax = (input->ymax - m_offsetY) * this->m_relY + 1;
  newInput.ymin = (input->ymin - m_offsetY) * this->m_relY;

  return BaseScaleOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void ScaleFixedSizeOperation::determineResolution(unsigned int resolution[2],
                                                  unsigned int /*preferredResolution*/[2])
{
  unsigned int nr[2];
  nr[0] = this->m_newWidth;
  nr[1] = this->m_newHeight;
  BaseScaleOperation::determineResolution(resolution, nr);
  resolution[0] = this->m_newWidth;
  resolution[1] = this->m_newHeight;
}
