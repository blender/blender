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

#include "IMB_colormanagement.h"

namespace blender::compositor {

ColorCorrectionOperation::ColorCorrectionOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->m_inputImage = nullptr;
  this->m_inputMask = nullptr;
  this->m_redChannelEnabled = true;
  this->m_greenChannelEnabled = true;
  this->m_blueChannelEnabled = true;
  flags.can_be_constant = true;
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
  value = MIN2(1.0f, value);
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

  /* Mix with mask. */
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

void ColorCorrectionOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_color = p.ins[0];
    const float *in_mask = p.ins[1];

    const float level = (in_color[0] + in_color[1] + in_color[2]) / 3.0f;
    float level_shadows = 0.0f;
    float level_midtones = 0.0f;
    float level_highlights = 0.0f;
    constexpr float MARGIN = 0.10f;
    constexpr float MARGIN_DIV = 0.5f / MARGIN;
    if (level < this->m_data->startmidtones - MARGIN) {
      level_shadows = 1.0f;
    }
    else if (level < this->m_data->startmidtones + MARGIN) {
      level_midtones = ((level - this->m_data->startmidtones) * MARGIN_DIV) + 0.5f;
      level_shadows = 1.0f - level_midtones;
    }
    else if (level < this->m_data->endmidtones - MARGIN) {
      level_midtones = 1.0f;
    }
    else if (level < this->m_data->endmidtones + MARGIN) {
      level_highlights = ((level - this->m_data->endmidtones) * MARGIN_DIV) + 0.5f;
      level_midtones = 1.0f - level_highlights;
    }
    else {
      level_highlights = 1.0f;
    }
    float contrast = this->m_data->master.contrast;
    float saturation = this->m_data->master.saturation;
    float gamma = this->m_data->master.gamma;
    float gain = this->m_data->master.gain;
    float lift = this->m_data->master.lift;
    contrast *= level_shadows * this->m_data->shadows.contrast +
                level_midtones * this->m_data->midtones.contrast +
                level_highlights * this->m_data->highlights.contrast;
    saturation *= level_shadows * this->m_data->shadows.saturation +
                  level_midtones * this->m_data->midtones.saturation +
                  level_highlights * this->m_data->highlights.saturation;
    gamma *= level_shadows * this->m_data->shadows.gamma +
             level_midtones * this->m_data->midtones.gamma +
             level_highlights * this->m_data->highlights.gamma;
    gain *= level_shadows * this->m_data->shadows.gain +
            level_midtones * this->m_data->midtones.gain +
            level_highlights * this->m_data->highlights.gain;
    lift += level_shadows * this->m_data->shadows.lift +
            level_midtones * this->m_data->midtones.lift +
            level_highlights * this->m_data->highlights.lift;

    const float inv_gamma = 1.0f / gamma;
    const float luma = IMB_colormanagement_get_luminance(in_color);

    float r = luma + saturation * (in_color[0] - luma);
    float g = luma + saturation * (in_color[1] - luma);
    float b = luma + saturation * (in_color[2] - luma);

    r = 0.5f + (r - 0.5f) * contrast;
    g = 0.5f + (g - 0.5f) * contrast;
    b = 0.5f + (b - 0.5f) * contrast;

    /* Check for negative values to avoid nan. */
    r = color_correct_powf_safe(r * gain + lift, inv_gamma, r);
    g = color_correct_powf_safe(g * gain + lift, inv_gamma, g);
    b = color_correct_powf_safe(b * gain + lift, inv_gamma, b);

    /* Mix with mask. */
    const float value = MIN2(1.0f, in_mask[0]);
    const float m_value = 1.0f - value;
    r = m_value * in_color[0] + value * r;
    g = m_value * in_color[1] + value * g;
    b = m_value * in_color[2] + value * b;

    p.out[0] = m_redChannelEnabled ? r : in_color[0];
    p.out[1] = m_greenChannelEnabled ? g : in_color[1];
    p.out[2] = m_blueChannelEnabled ? b : in_color[2];
    p.out[3] = in_color[3];
  }
}

void ColorCorrectionOperation::deinitExecution()
{
  this->m_inputImage = nullptr;
  this->m_inputMask = nullptr;
}

}  // namespace blender::compositor
