/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorCorrectionOperation.h"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

ColorCorrectionOperation::ColorCorrectionOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  red_channel_enabled_ = true;
  green_channel_enabled_ = true;
  blue_channel_enabled_ = true;
  flags_.can_be_constant = true;
}

/* Calculate x^y if the function is defined. Otherwise return the given fallback value. */
BLI_INLINE float color_correct_powf_safe(const float x, const float y, const float fallback_value)
{
  if (x < 0) {
    return fallback_value;
  }
  return powf(x, y);
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
    if (level < data_->startmidtones - MARGIN) {
      level_shadows = 1.0f;
    }
    else if (level < data_->startmidtones + MARGIN) {
      level_midtones = ((level - data_->startmidtones) * MARGIN_DIV) + 0.5f;
      level_shadows = 1.0f - level_midtones;
    }
    else if (level < data_->endmidtones - MARGIN) {
      level_midtones = 1.0f;
    }
    else if (level < data_->endmidtones + MARGIN) {
      level_highlights = ((level - data_->endmidtones) * MARGIN_DIV) + 0.5f;
      level_midtones = 1.0f - level_highlights;
    }
    else {
      level_highlights = 1.0f;
    }
    float contrast = data_->master.contrast;
    float saturation = data_->master.saturation;
    float gamma = data_->master.gamma;
    float gain = data_->master.gain;
    float lift = data_->master.lift;
    contrast *= level_shadows * data_->shadows.contrast +
                level_midtones * data_->midtones.contrast +
                level_highlights * data_->highlights.contrast;
    saturation *= level_shadows * data_->shadows.saturation +
                  level_midtones * data_->midtones.saturation +
                  level_highlights * data_->highlights.saturation;
    gamma *= level_shadows * data_->shadows.gamma + level_midtones * data_->midtones.gamma +
             level_highlights * data_->highlights.gamma;
    gain *= level_shadows * data_->shadows.gain + level_midtones * data_->midtones.gain +
            level_highlights * data_->highlights.gain;
    lift += level_shadows * data_->shadows.lift + level_midtones * data_->midtones.lift +
            level_highlights * data_->highlights.lift;

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
    const float value = std::min(1.0f, in_mask[0]);
    const float value_ = 1.0f - value;
    r = value_ * in_color[0] + value * r;
    g = value_ * in_color[1] + value * g;
    b = value_ * in_color[2] + value * b;

    p.out[0] = red_channel_enabled_ ? r : in_color[0];
    p.out[1] = green_channel_enabled_ ? g : in_color[1];
    p.out[2] = blue_channel_enabled_ ? b : in_color[2];
    p.out[3] = in_color[3];
  }
}

}  // namespace blender::compositor
