/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/types.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

ccl_device float3 svm_mix_blend(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, col2, t);
}

ccl_device float3 svm_mix_add(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, col1 + col2, t);
}

ccl_device float3 svm_mix_mul(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, col1 * col2, t);
}

ccl_device float3 svm_mix_screen(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;
  const float3 one = make_float3(1.0f, 1.0f, 1.0f);
  const float3 tm3 = make_float3(tm, tm, tm);

  return one - (tm3 + t * (one - col2)) * (one - col1);
}

ccl_device float3 svm_mix_overlay(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;

  float3 outcol = col1;

  if (outcol.x < 0.5f) {
    outcol.x *= tm + 2.0f * t * col2.x;
  }
  else {
    outcol.x = 1.0f - (tm + 2.0f * t * (1.0f - col2.x)) * (1.0f - outcol.x);
  }

  if (outcol.y < 0.5f) {
    outcol.y *= tm + 2.0f * t * col2.y;
  }
  else {
    outcol.y = 1.0f - (tm + 2.0f * t * (1.0f - col2.y)) * (1.0f - outcol.y);
  }

  if (outcol.z < 0.5f) {
    outcol.z *= tm + 2.0f * t * col2.z;
  }
  else {
    outcol.z = 1.0f - (tm + 2.0f * t * (1.0f - col2.z)) * (1.0f - outcol.z);
  }

  return outcol;
}

ccl_device float3 svm_mix_sub(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, col1 - col2, t);
}

ccl_device float3 svm_mix_div(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;

  float3 outcol = col1;

  if (col2.x != 0.0f) {
    outcol.x = tm * outcol.x + t * outcol.x / col2.x;
  }
  if (col2.y != 0.0f) {
    outcol.y = tm * outcol.y + t * outcol.y / col2.y;
  }
  if (col2.z != 0.0f) {
    outcol.z = tm * outcol.z + t * outcol.z / col2.z;
  }

  return outcol;
}

ccl_device float3 svm_mix_diff(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, fabs(col1 - col2), t);
}

ccl_device float3 svm_mix_exclusion(const float t, const float3 col1, const float3 col2)
{
  return max(interp(col1, col1 + col2 - 2.0f * col1 * col2, t), zero_float3());
}

ccl_device float3 svm_mix_dark(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, min(col1, col2), t);
}

ccl_device float3 svm_mix_light(const float t, const float3 col1, const float3 col2)
{
  return interp(col1, max(col1, col2), t);
}

ccl_device float3 svm_mix_dodge(const float t, const float3 col1, const float3 col2)
{
  float3 outcol = col1;

  if (outcol.x != 0.0f) {
    float tmp = 1.0f - t * col2.x;
    if (tmp <= 0.0f) {
      outcol.x = 1.0f;
    }
    else {
      tmp = outcol.x / tmp;
      if (tmp > 1.0f) {
        outcol.x = 1.0f;
      }
      else {
        outcol.x = tmp;
      }
    }
  }
  if (outcol.y != 0.0f) {
    float tmp = 1.0f - t * col2.y;
    if (tmp <= 0.0f) {
      outcol.y = 1.0f;
    }
    else {
      tmp = outcol.y / tmp;
      if (tmp > 1.0f) {
        outcol.y = 1.0f;
      }
      else {
        outcol.y = tmp;
      }
    }
  }
  if (outcol.z != 0.0f) {
    float tmp = 1.0f - t * col2.z;
    if (tmp <= 0.0f) {
      outcol.z = 1.0f;
    }
    else {
      tmp = outcol.z / tmp;
      if (tmp > 1.0f) {
        outcol.z = 1.0f;
      }
      else {
        outcol.z = tmp;
      }
    }
  }

  return outcol;
}

ccl_device float3 svm_mix_burn(const float t, const float3 col1, const float3 col2)
{
  float tmp;
  const float tm = 1.0f - t;

  float3 outcol = col1;

  tmp = tm + t * col2.x;
  if (tmp <= 0.0f) {
    outcol.x = 0.0f;
  }
  else {
    tmp = (1.0f - (1.0f - outcol.x) / tmp);
    if (tmp < 0.0f) {
      outcol.x = 0.0f;
    }
    else if (tmp > 1.0f) {
      outcol.x = 1.0f;
    }
    else {
      outcol.x = tmp;
    }
  }

  tmp = tm + t * col2.y;
  if (tmp <= 0.0f) {
    outcol.y = 0.0f;
  }
  else {
    tmp = (1.0f - (1.0f - outcol.y) / tmp);
    if (tmp < 0.0f) {
      outcol.y = 0.0f;
    }
    else if (tmp > 1.0f) {
      outcol.y = 1.0f;
    }
    else {
      outcol.y = tmp;
    }
  }

  tmp = tm + t * col2.z;
  if (tmp <= 0.0f) {
    outcol.z = 0.0f;
  }
  else {
    tmp = (1.0f - (1.0f - outcol.z) / tmp);
    if (tmp < 0.0f) {
      outcol.z = 0.0f;
    }
    else if (tmp > 1.0f) {
      outcol.z = 1.0f;
    }
    else {
      outcol.z = tmp;
    }
  }

  return outcol;
}

ccl_device float3 svm_mix_hue(const float t, const float3 col1, const float3 col2)
{
  float3 outcol = col1;

  const float3 hsv2 = rgb_to_hsv(col2);

  if (hsv2.y != 0.0f) {
    float3 hsv = rgb_to_hsv(outcol);
    hsv.x = hsv2.x;
    const float3 tmp = hsv_to_rgb(hsv);

    outcol = interp(outcol, tmp, t);
  }

  return outcol;
}

ccl_device float3 svm_mix_sat(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;

  float3 outcol = col1;

  float3 hsv = rgb_to_hsv(outcol);

  if (hsv.y != 0.0f) {
    const float3 hsv2 = rgb_to_hsv(col2);

    hsv.y = tm * hsv.y + t * hsv2.y;
    outcol = hsv_to_rgb(hsv);
  }

  return outcol;
}

ccl_device float3 svm_mix_val(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;

  float3 hsv = rgb_to_hsv(col1);
  const float3 hsv2 = rgb_to_hsv(col2);

  hsv.z = tm * hsv.z + t * hsv2.z;

  return hsv_to_rgb(hsv);
}

ccl_device float3 svm_mix_color(const float t, const float3 col1, const float3 col2)
{
  float3 outcol = col1;
  const float3 hsv2 = rgb_to_hsv(col2);

  if (hsv2.y != 0.0f) {
    float3 hsv = rgb_to_hsv(outcol);
    hsv.x = hsv2.x;
    hsv.y = hsv2.y;
    const float3 tmp = hsv_to_rgb(hsv);

    outcol = interp(outcol, tmp, t);
  }

  return outcol;
}

ccl_device float3 svm_mix_soft(const float t, const float3 col1, const float3 col2)
{
  const float tm = 1.0f - t;

  const float3 one = make_float3(1.0f, 1.0f, 1.0f);
  const float3 scr = one - (one - col2) * (one - col1);

  return tm * col1 + t * ((one - col1) * col2 * col1 + col1 * scr);
}

ccl_device float3 svm_mix_linear(const float t, const float3 col1, const float3 col2)
{
  return col1 + t * (2.0f * col2 + make_float3(-1.0f, -1.0f, -1.0f));
}

ccl_device float3 svm_mix_clamp(const float3 col)
{
  return saturate(col);
}

ccl_device_noinline_cpu float3 svm_mix(NodeMix type,
                                       const float t,
                                       const float3 c1,
                                       const float3 c2)
{
  switch (type) {
    case NODE_MIX_BLEND:
      return svm_mix_blend(t, c1, c2);
    case NODE_MIX_ADD:
      return svm_mix_add(t, c1, c2);
    case NODE_MIX_MUL:
      return svm_mix_mul(t, c1, c2);
    case NODE_MIX_SCREEN:
      return svm_mix_screen(t, c1, c2);
    case NODE_MIX_OVERLAY:
      return svm_mix_overlay(t, c1, c2);
    case NODE_MIX_SUB:
      return svm_mix_sub(t, c1, c2);
    case NODE_MIX_DIV:
      return svm_mix_div(t, c1, c2);
    case NODE_MIX_DIFF:
      return svm_mix_diff(t, c1, c2);
    case NODE_MIX_EXCLUSION:
      return svm_mix_exclusion(t, c1, c2);
    case NODE_MIX_DARK:
      return svm_mix_dark(t, c1, c2);
    case NODE_MIX_LIGHT:
      return svm_mix_light(t, c1, c2);
    case NODE_MIX_DODGE:
      return svm_mix_dodge(t, c1, c2);
    case NODE_MIX_BURN:
      return svm_mix_burn(t, c1, c2);
    case NODE_MIX_HUE:
      return svm_mix_hue(t, c1, c2);
    case NODE_MIX_SAT:
      return svm_mix_sat(t, c1, c2);
    case NODE_MIX_VAL:
      return svm_mix_val(t, c1, c2);
    case NODE_MIX_COL:
      return svm_mix_color(t, c1, c2);
    case NODE_MIX_SOFT:
      return svm_mix_soft(t, c1, c2);
    case NODE_MIX_LINEAR:
      return svm_mix_linear(t, c1, c2);
    case NODE_MIX_CLAMP:
      return svm_mix_clamp(c1);
  }

  return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_noinline_cpu float3 svm_mix_clamped_factor(NodeMix type,
                                                      const float t,
                                                      const float3 c1,
                                                      const float3 c2)
{
  const float fac = saturatef(t);
  return svm_mix(type, fac, c1, c2);
}

ccl_device_inline float3 svm_brightness_contrast(float3 color,
                                                 const float brightness,
                                                 const float contrast)
{
  const float a = 1.0f + contrast;
  const float b = brightness - contrast * 0.5f;

  color.x = max(a * color.x + b, 0.0f);
  color.y = max(a * color.y + b, 0.0f);
  color.z = max(a * color.z + b, 0.0f);

  return color;
}

ccl_device float3 svm_combine_color(NodeCombSepColorType type, const float3 color)
{
  switch (type) {
    case NODE_COMBSEP_COLOR_HSV:
      return hsv_to_rgb(color);
    case NODE_COMBSEP_COLOR_HSL:
      return hsl_to_rgb(color);
    case NODE_COMBSEP_COLOR_RGB:
    default:
      return color;
  }
}

ccl_device float3 svm_separate_color(NodeCombSepColorType type, const float3 color)
{
  switch (type) {
    case NODE_COMBSEP_COLOR_HSV:
      return rgb_to_hsv(color);
    case NODE_COMBSEP_COLOR_HSL:
      return rgb_to_hsl(color);
    case NODE_COMBSEP_COLOR_RGB:
    default:
      return color;
  }
}

CCL_NAMESPACE_END
