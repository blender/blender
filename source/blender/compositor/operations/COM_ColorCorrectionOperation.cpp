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

#include "COM_ColorCorrectionOperation.h"
#include "BLI_math.h"

#include "IMB_colormanagement.h"

ColorCorrectionOperation::ColorCorrectionOperation() : NodeOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_inputImage = NULL;
  this->m_inputMask = NULL;
  this->m_redChannelEnabled = true;
  this->m_greenChannelEnabled = true;
  this->m_blueChannelEnabled = true;
}
void ColorCorrectionOperation::initExecution()
{
  this->m_inputImage = this->getInputSocketReader(0);
  this->m_inputMask = this->getInputSocketReader(1);
}

/* Calculate x^y if the function is defined. Otherwise return the given fallback value. */
BLI_INLINE float color_correct_powf_safe(const float x, const float y, const float fallback_value)
{
  if (x < 0) {
    return fallback_value;
  }
  return powf(x, y);
}

void ColorCorrectionOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputImageColor[4];
  float inputMask[4];
  this->m_inputImage->readSampled(inputImageColor, x, y, sampler);
  this->m_inputMask->readSampled(inputMask, x, y, sampler);

  float level = (inputImageColor[0] + inputImageColor[1] + inputImageColor[2]) / 3.0f;
  float contrast = this->m_data->master.contrast;
  float saturation = this->m_data->master.saturation;
  float gamma = this->m_data->master.gamma;
  float gain = this->m_data->master.gain;
  float lift = this->m_data->master.lift;
  float r, g, b;

  float value = inputMask[0];
  value = min(1.0f, value);
  const float mvalue = 1.0f - value;

  float levelShadows = 0.0;
  float levelMidtones = 0.0;
  float levelHighlights = 0.0;
#define MARGIN 0.10f
#define MARGIN_DIV (0.5f / MARGIN)
  if (level < this->m_data->startmidtones - MARGIN) {
    levelShadows = 1.0f;
  }
  else if (level < this->m_data->startmidtones + MARGIN) {
    levelMidtones = ((level - this->m_data->startmidtones) * MARGIN_DIV) + 0.5f;
    levelShadows = 1.0f - levelMidtones;
  }
  else if (level < this->m_data->endmidtones - MARGIN) {
    levelMidtones = 1.0f;
  }
  else if (level < this->m_data->endmidtones + MARGIN) {
    levelHighlights = ((level - this->m_data->endmidtones) * MARGIN_DIV) + 0.5f;
    levelMidtones = 1.0f - levelHighlights;
  }
  else {
    levelHighlights = 1.0f;
  }
#undef MARGIN
#undef MARGIN_DIV
  contrast *= (levelShadows * this->m_data->shadows.contrast) +
              (levelMidtones * this->m_data->midtones.contrast) +
              (levelHighlights * this->m_data->highlights.contrast);
  saturation *= (levelShadows * this->m_data->shadows.saturation) +
                (levelMidtones * this->m_data->midtones.saturation) +
                (levelHighlights * this->m_data->highlights.saturation);
  gamma *= (levelShadows * this->m_data->shadows.gamma) +
           (levelMidtones * this->m_data->midtones.gamma) +
           (levelHighlights * this->m_data->highlights.gamma);
  gain *= (levelShadows * this->m_data->shadows.gain) +
          (levelMidtones * this->m_data->midtones.gain) +
          (levelHighlights * this->m_data->highlights.gain);
  lift += (levelShadows * this->m_data->shadows.lift) +
          (levelMidtones * this->m_data->midtones.lift) +
          (levelHighlights * this->m_data->highlights.lift);

  float invgamma = 1.0f / gamma;
  float luma = IMB_colormanagement_get_luminance(inputImageColor);

  r = inputImageColor[0];
  g = inputImageColor[1];
  b = inputImageColor[2];

  r = (luma + saturation * (r - luma));
  g = (luma + saturation * (g - luma));
  b = (luma + saturation * (b - luma));

  r = 0.5f + ((r - 0.5f) * contrast);
  g = 0.5f + ((g - 0.5f) * contrast);
  b = 0.5f + ((b - 0.5f) * contrast);

  /* Check for negative values to avoid nan. */
  r = color_correct_powf_safe(r * gain + lift, invgamma, r);
  g = color_correct_powf_safe(g * gain + lift, invgamma, g);
  b = color_correct_powf_safe(b * gain + lift, invgamma, b);

  // mix with mask
  r = mvalue * inputImageColor[0] + value * r;
  g = mvalue * inputImageColor[1] + value * g;
  b = mvalue * inputImageColor[2] + value * b;

  if (this->m_redChannelEnabled) {
    output[0] = r;
  }
  else {
    output[0] = inputImageColor[0];
  }
  if (this->m_greenChannelEnabled) {
    output[1] = g;
  }
  else {
    output[1] = inputImageColor[1];
  }
  if (this->m_blueChannelEnabled) {
    output[2] = b;
  }
  else {
    output[2] = inputImageColor[2];
  }
  output[3] = inputImageColor[3];
}

void ColorCorrectionOperation::deinitExecution()
{
  this->m_inputImage = NULL;
  this->m_inputMask = NULL;
}
