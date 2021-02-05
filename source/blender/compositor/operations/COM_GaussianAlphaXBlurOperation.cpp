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

#include "COM_GaussianAlphaXBlurOperation.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"

GaussianAlphaXBlurOperation::GaussianAlphaXBlurOperation() : BlurBaseOperation(COM_DT_VALUE)
{
  this->m_gausstab = nullptr;
  this->m_filtersize = 0;
  this->m_falloff = -1; /* intentionally invalid, so we can detect uninitialized values */
}

void *GaussianAlphaXBlurOperation::initializeTileData(rcti * /*rect*/)
{
  lockMutex();
  if (!this->m_sizeavailable) {
    updateGauss();
  }
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  unlockMutex();
  return buffer;
}

void GaussianAlphaXBlurOperation::initExecution()
{
  /* Until we support size input - comment this. */
  // BlurBaseOperation::initExecution();

  initMutex();

  if (this->m_sizeavailable) {
    float rad = max_ff(m_size * m_data.sizex, 0.0f);
    m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
    m_distbuf_inv = BlurBaseOperation::make_dist_fac_inverse(rad, m_filtersize, m_falloff);
  }
}

void GaussianAlphaXBlurOperation::updateGauss()
{
  if (this->m_gausstab == nullptr) {
    updateSize();
    float rad = max_ff(m_size * m_data.sizex, 0.0f);
    m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
  }

  if (this->m_distbuf_inv == nullptr) {
    updateSize();
    float rad = max_ff(m_size * m_data.sizex, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    m_distbuf_inv = BlurBaseOperation::make_dist_fac_inverse(rad, m_filtersize, m_falloff);
  }
}

BLI_INLINE float finv_test(const float f, const bool test)
{
  return (LIKELY(test == false)) ? f : 1.0f - f;
}

void GaussianAlphaXBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  const bool do_invert = this->m_do_subtract;
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();
  int bufferwidth = inputBuffer->getWidth();
  int bufferstartx = inputBuffer->getRect()->xmin;
  int bufferstarty = inputBuffer->getRect()->ymin;

  rcti &rect = *inputBuffer->getRect();
  int xmin = max_ii(x - m_filtersize, rect.xmin);
  int xmax = min_ii(x + m_filtersize + 1, rect.xmax);
  int ymin = max_ii(y, rect.ymin);

  /* *** this is the main part which is different to 'GaussianXBlurOperation'  *** */
  int step = getStep();
  int bufferindex = ((xmin - bufferstartx)) + ((ymin - bufferstarty) * bufferwidth);

  /* gauss */
  float alpha_accum = 0.0f;
  float multiplier_accum = 0.0f;

  /* dilate */
  float value_max = finv_test(
      buffer[(x) + (y * bufferwidth)],
      do_invert);              /* init with the current color to avoid unneeded lookups */
  float distfacinv_max = 1.0f; /* 0 to 1 */

  for (int nx = xmin; nx < xmax; nx += step) {
    const int index = (nx - x) + this->m_filtersize;
    float value = finv_test(buffer[bufferindex], do_invert);
    float multiplier;

    /* gauss */
    {
      multiplier = this->m_gausstab[index];
      alpha_accum += value * multiplier;
      multiplier_accum += multiplier;
    }

    /* dilate - find most extreme color */
    if (value > value_max) {
      multiplier = this->m_distbuf_inv[index];
      value *= multiplier;
      if (value > value_max) {
        value_max = value;
        distfacinv_max = multiplier;
      }
    }
    bufferindex += step;
  }

  /* blend between the max value and gauss blue - gives nice feather */
  const float value_blur = alpha_accum / multiplier_accum;
  const float value_final = (value_max * distfacinv_max) + (value_blur * (1.0f - distfacinv_max));
  output[0] = finv_test(value_final, do_invert);
}

void GaussianAlphaXBlurOperation::deinitExecution()
{
  BlurBaseOperation::deinitExecution();

  if (this->m_gausstab) {
    MEM_freeN(this->m_gausstab);
    this->m_gausstab = nullptr;
  }

  if (this->m_distbuf_inv) {
    MEM_freeN(this->m_distbuf_inv);
    this->m_distbuf_inv = nullptr;
  }

  deinitMutex();
}

bool GaussianAlphaXBlurOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;
#if 0 /* until we add size input */
  rcti sizeInput;
  sizeInput.xmin = 0;
  sizeInput.ymin = 0;
  sizeInput.xmax = 5;
  sizeInput.ymax = 5;

  NodeOperation *operation = this->getInputOperation(1);
  if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
    return true;
  }
  else
#endif
  {
    if (this->m_sizeavailable && this->m_gausstab != nullptr) {
      newInput.xmax = input->xmax + this->m_filtersize + 1;
      newInput.xmin = input->xmin - this->m_filtersize - 1;
      newInput.ymax = input->ymax;
      newInput.ymin = input->ymin;
    }
    else {
      newInput.xmax = this->getWidth();
      newInput.xmin = 0;
      newInput.ymax = this->getHeight();
      newInput.ymin = 0;
    }
    return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
  }
}
