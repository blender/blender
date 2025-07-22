/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_brush.hh"
#include "BKE_paint.hh"
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
                                const std::optional<BrushColorJitterSettings> &jitter,
                                const float stroke_hue_factor,
                                const float stroke_saturation_factor,
                                const float stroke_value_factor,
                                const float distance,
                                const ColorGeometry4f color,
                                const float pressure)
{
  const bool use_random = (settings.flag & GP_BRUSH_GROUP_RANDOM) != 0;
  if (!use_random || !jitter) {
    return color;
  }
  blender::float3 initial_hsv_jitter = {
      stroke_hue_factor, stroke_saturation_factor, stroke_value_factor};

  blender::float3 jittered = BKE_paint_randomize_color(
      *jitter, initial_hsv_jitter, distance, pressure, {color.r, color.g, color.b});

  return {jittered[0], jittered[1], jittered[2], color.a};
}

}  // namespace blender::ed::greasepencil
