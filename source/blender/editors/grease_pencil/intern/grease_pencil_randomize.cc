/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_noise.hh"
#include "BLI_rand.hh"

#include "BKE_colortools.hh"

#include "ED_grease_pencil.hh"

#include "DNA_brush_types.h"

namespace blender::ed::greasepencil {

float randomize_radius(const BrushGpencilSettings &settings,
                       const float stroke_factor,
                       const float distance,
                       const float radius,
                       const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !(settings.draw_random_press > 0.0f)) {
    return radius;
  }
  float random_factor = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_PRESS_AT_STROKE) == 0) {
    /* TODO: This should be exposed as a setting to scale the noise along the stroke. */
    constexpr float noise_scale = 1 / 20.0f;
    random_factor = noise::perlin_signed(float2(distance * noise_scale, stroke_factor));
  }
  else {
    random_factor = stroke_factor;
  }

  if ((settings.flag2 & GP_BRUSH_USE_PRESSURE_RAND_PRESS) != 0) {
    random_factor *= BKE_curvemapping_evaluateF(settings.curve_rand_pressure, 0, pressure);
  }

  const float randomized_radius = math::interpolate(
      radius, radius * (1.0f + random_factor), settings.draw_random_press);
  return math::max(randomized_radius, 0.0f);
}

float randomize_opacity(const BrushGpencilSettings &settings,
                        const float stroke_factor,
                        const float distance,
                        const float opacity,
                        const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !(settings.draw_random_strength > 0.0f)) {
    return opacity;
  }
  float random_factor = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_STRENGTH_AT_STROKE) == 0) {
    /* TODO: This should be exposed as a setting to scale the noise along the stroke. */
    constexpr float noise_scale = 1 / 20.0f;
    random_factor = noise::perlin_signed(float2(distance * noise_scale, stroke_factor));
  }
  else {
    random_factor = stroke_factor;
  }

  if ((settings.flag2 & GP_BRUSH_USE_STRENGTH_RAND_PRESS) != 0) {
    random_factor *= BKE_curvemapping_evaluateF(settings.curve_rand_strength, 0, pressure);
  }

  const float randomized_opacity = math::interpolate(
      opacity, opacity + random_factor, settings.draw_random_strength);
  return math::clamp(randomized_opacity, 0.0f, 1.0f);
}

float randomize_rotation(const BrushGpencilSettings &settings,
                         const float stroke_factor,
                         const float distance,
                         const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !(settings.uv_random > 0.0f)) {
    return 0.0f;
  }
  float random_factor = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_UV_AT_STROKE) == 0) {
    /* TODO: This should be exposed as a setting to scale the noise along the stroke. */
    constexpr float noise_scale = 1 / 20.0f;
    random_factor = noise::perlin_signed(float2(distance * noise_scale, stroke_factor));
  }
  else {
    random_factor = stroke_factor;
  }

  if ((settings.flag2 & GP_BRUSH_USE_UV_RAND_PRESS) != 0) {
    random_factor *= BKE_curvemapping_evaluateF(settings.curve_rand_uv, 0, pressure);
  }

  const float random_rotation = random_factor * math::numbers::pi;
  return math::interpolate(0.0f, random_rotation, settings.uv_random);
}

float randomize_rotation(const BrushGpencilSettings &settings,
                         RandomNumberGenerator &rng,
                         const float stroke_factor,
                         const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !(settings.uv_random > 0.0f)) {
    return 0.0f;
  }
  float random_factor = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_UV_AT_STROKE) == 0) {
    random_factor = rng.get_float() * 2.0f - 1.0f;
  }
  else {
    random_factor = stroke_factor;
  }

  if ((settings.flag2 & GP_BRUSH_USE_UV_RAND_PRESS) != 0) {
    random_factor *= BKE_curvemapping_evaluateF(settings.curve_rand_uv, 0, pressure);
  }

  const float random_rotation = random_factor * math::numbers::pi;
  return math::interpolate(0.0f, random_rotation, settings.uv_random);
}

ColorGeometry4f randomize_color(const BrushGpencilSettings &settings,
                                const float stroke_hue_factor,
                                const float stroke_saturation_factor,
                                const float stroke_value_factor,
                                const float distance,
                                const ColorGeometry4f color,
                                const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !(settings.random_hue > 0.0f || settings.random_saturation > 0.0f ||
                       settings.random_value > 0.0f))
  {
    return color;
  }
  /* TODO: This should be exposed as a setting to scale the noise along the stroke. */
  constexpr float noise_scale = 1 / 20.0f;

  float random_hue = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_HUE_AT_STROKE) == 0) {
    random_hue = noise::perlin_signed(float2(distance * noise_scale, stroke_hue_factor));
  }
  else {
    random_hue = stroke_hue_factor;
  }

  float random_saturation = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_SAT_AT_STROKE) == 0) {
    random_saturation = noise::perlin_signed(
        float2(distance * noise_scale, stroke_saturation_factor));
  }
  else {
    random_saturation = stroke_saturation_factor;
  }

  float random_value = 0.0f;
  if ((settings.flag2 & GP_BRUSH_USE_VAL_AT_STROKE) == 0) {
    random_value = noise::perlin_signed(float2(distance * noise_scale, stroke_value_factor));
  }
  else {
    random_value = stroke_value_factor;
  }

  if ((settings.flag2 & GP_BRUSH_USE_HUE_RAND_PRESS) != 0) {
    random_hue *= BKE_curvemapping_evaluateF(settings.curve_rand_hue, 0, pressure);
  }
  if ((settings.flag2 & GP_BRUSH_USE_SAT_RAND_PRESS) != 0) {
    random_saturation *= BKE_curvemapping_evaluateF(settings.curve_rand_saturation, 0, pressure);
  }
  if ((settings.flag2 & GP_BRUSH_USE_VAL_RAND_PRESS) != 0) {
    random_value *= BKE_curvemapping_evaluateF(settings.curve_rand_value, 0, pressure);
  }

  float3 hsv;
  linearrgb_to_srgb_v3_v3(hsv, color);
  rgb_to_hsv_v(hsv, hsv);

  hsv[0] += random_hue * settings.random_hue;

  const float absolute_brightness = hsv[2] + random_value * settings.random_value;

  /*
   * To match relative brightness we want the ratio of the original to modified Value to not
   * depend on the brightness of the input Value, Exp is used because we need a function that
   * is positive for all 'x' and has the property that 'f(-x) = 1/f(x)' this is so that
   * we make the Value on average growth and shrink by the same amount. To detriment the rate
   * of the Exponential we set slope to match addition for small random values at an arbitrary
   * Base Value.
   */
  constexpr float base_value = 0.5f;
  const float relative_brightness = hsv[2] *
                                    math::exp(random_value * settings.random_value / base_value);

  /* Use the fourth power. */
  const float blend_factor = math::pow(settings.random_value, 4.0f);
  /* Use relative brightness at low randomness, and switch to absolute at high. */
  hsv[2] = math::interpolate(relative_brightness, absolute_brightness, blend_factor);

  /* Multiply by the current saturation to prevent grays from becoming red. */
  hsv[1] += random_saturation * settings.random_saturation * 2.0f * hsv[1];

  /* Wrap hue, clamp saturation and value. */
  hsv[0] = math::fract(hsv[0]);
  hsv[1] = math::clamp(hsv[1], 0.0f, 1.0f);
  hsv[2] = math::clamp(hsv[2], 0.0f, 1.0f);

  ColorGeometry4f random_color;
  hsv_to_rgb_v(hsv, random_color);
  srgb_to_linearrgb_v3_v3(random_color, random_color);
  random_color.a = color.a;
  return random_color;
}

}  // namespace blender::ed::greasepencil
