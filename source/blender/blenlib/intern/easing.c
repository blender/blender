/* SPDX-FileCopyrightText: 2001 Robert Penner. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"

#include "BLI_easing.h" /* own include */

#include "BLI_strict_flags.h" /* Keep last. */

/* blend if (amplitude < fabsf(change) */
#define USE_ELASTIC_BLEND

float BLI_easing_back_ease_in(
    float time, float begin, float change, float duration, float overshoot)
{
  time /= duration;
  return change * time * time * ((overshoot + 1) * time - overshoot) + begin;
}

float BLI_easing_back_ease_out(
    float time, float begin, float change, float duration, float overshoot)
{
  time = time / duration - 1;
  return change * (time * time * ((overshoot + 1) * time + overshoot) + 1) + begin;
}

float BLI_easing_back_ease_in_out(
    float time, float begin, float change, float duration, float overshoot)
{
  overshoot *= 1.525f;
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * (time * time * ((overshoot + 1) * time - overshoot)) + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * ((overshoot + 1) * time + overshoot) + 2) + begin;
}

float BLI_easing_bounce_ease_out(float time, float begin, float change, float duration)
{
  time /= duration;
  if (time < (1 / 2.75f)) {
    return change * (7.5625f * time * time) + begin;
  }
  if (time < (2 / 2.75f)) {
    time -= (1.5f / 2.75f);
    return change * ((7.5625f * time) * time + 0.75f) + begin;
  }
  if (time < (2.5f / 2.75f)) {
    time -= (2.25f / 2.75f);
    return change * ((7.5625f * time) * time + 0.9375f) + begin;
  }
  time -= (2.625f / 2.75f);
  return change * ((7.5625f * time) * time + 0.984375f) + begin;
}

float BLI_easing_bounce_ease_in(float time, float begin, float change, float duration)
{
  return change - BLI_easing_bounce_ease_out(duration - time, 0, change, duration) + begin;
}

float BLI_easing_bounce_ease_in_out(float time, float begin, float change, float duration)
{
  if (time < duration / 2) {
    return BLI_easing_bounce_ease_in(time * 2, 0, change, duration) * 0.5f + begin;
  }
  return BLI_easing_bounce_ease_out(time * 2 - duration, 0, change, duration) * 0.5f +
         change * 0.5f + begin;
}

float BLI_easing_circ_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return -change * (sqrtf(1 - time * time) - 1) + begin;
}

float BLI_easing_circ_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * sqrtf(1 - time * time) + begin;
}

float BLI_easing_circ_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return -change / 2 * (sqrtf(1 - time * time) - 1) + begin;
  }
  time -= 2.0f;
  return change / 2 * (sqrtf(1 - time * time) + 1) + begin;
}

float BLI_easing_cubic_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time + begin;
}

float BLI_easing_cubic_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * (time * time * time + 1) + begin;
}

float BLI_easing_cubic_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * time + 2) + begin;
}

#ifdef USE_ELASTIC_BLEND
/**
 * When the amplitude is less than the change, we need to blend
 * \a f when we're close to the crossing point (int time), else we get an ugly sharp falloff.
 */
static float elastic_blend(
    float time, float change, float duration, float amplitude, float s, float f)
{
  if (change) {
    /* Looks like a magic number,
     * but this is a part of the sine curve we need to blend from */
    const float t = fabsf(s);
    if (amplitude) {
      f *= amplitude / fabsf(change);
    }
    else {
      f = 0.0f;
    }

    if (fabsf(time * duration) < t) {
      float l = fabsf(time * duration) / t;
      f = (f * l) + (1.0f - l);
    }
  }

  return f;
}
#endif

float BLI_easing_elastic_ease_in(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }

  if ((time /= duration) == 1.0f) {
    return begin + change;
  }
  time -= 1.0f;
  if (!period) {
    period = duration * 0.3f;
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  return (-f * (amplitude * powf(2, 10 * time) *
                sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         begin;
}

float BLI_easing_elastic_ease_out(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }
  if ((time /= duration) == 1.0f) {
    return begin + change;
  }
  time = -time;
  if (!period) {
    period = duration * 0.3f;
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  return (f * (amplitude * powf(2, 10 * time) *
               sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         change + begin;
}

float BLI_easing_elastic_ease_in_out(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }
  if ((time /= duration / 2) == 2.0f) {
    return begin + change;
  }
  time -= 1.0f;
  if (!period) {
    period = duration * (0.3f * 1.5f);
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  if (time < 0.0f) {
    f *= -0.5f;
    return (f * (amplitude * powf(2, 10 * time) *
                 sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
           begin;
  }

  time = -time;
  f *= 0.5f;
  return (f * (amplitude * powf(2, 10 * time) *
               sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         change + begin;
}

static const float pow_min = 0.0009765625f; /* = 2^(-10) */
static const float pow_scale = 1.0f / (1.0f - 0.0009765625f);

float BLI_easing_expo_ease_in(float time, float begin, float change, float duration)
{
  if (time == 0.0) {
    return begin;
  }
  return change * (powf(2, 10 * (time / duration - 1)) - pow_min) * pow_scale + begin;
}

float BLI_easing_expo_ease_out(float time, float begin, float change, float duration)
{
  if (time == 0.0) {
    return begin;
  }
  return change * (1 - (powf(2, -10 * time / duration) - pow_min) * pow_scale) + begin;
}

float BLI_easing_expo_ease_in_out(float time, float begin, float change, float duration)
{
  float duration_half = duration / 2.0f;
  float change_half = change / 2.0f;
  if (time <= duration_half) {
    return BLI_easing_expo_ease_in(time, begin, change_half, duration_half);
  }
  return BLI_easing_expo_ease_out(
      time - duration_half, begin + change_half, change_half, duration_half);
}

float BLI_easing_linear_ease(float time, float begin, float change, float duration)
{
  return change * time / duration + begin;
}

float BLI_easing_quad_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time + begin;
}

float BLI_easing_quad_ease_out(float time, float begin, float change, float duration)
{
  time /= duration;
  return -change * time * (time - 2) + begin;
}

float BLI_easing_quad_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time + begin;
  }
  time -= 1.0f;
  return -change / 2 * (time * (time - 2) - 1) + begin;
}

float BLI_easing_quart_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time * time + begin;
}

float BLI_easing_quart_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return -change * (time * time * time * time - 1) + begin;
}

float BLI_easing_quart_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time * time + begin;
  }
  time -= 2.0f;
  return -change / 2 * (time * time * time * time - 2) + begin;
}

float BLI_easing_quint_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time * time * time + begin;
}
float BLI_easing_quint_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * (time * time * time * time * time + 1) + begin;
}
float BLI_easing_quint_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time * time * time + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * time * time * time + 2) + begin;
}

float BLI_easing_sine_ease_in(float time, float begin, float change, float duration)
{
  return -change * cosf(time / duration * (float)M_PI_2) + change + begin;
}

float BLI_easing_sine_ease_out(float time, float begin, float change, float duration)
{
  return change * sinf(time / duration * (float)M_PI_2) + begin;
}

float BLI_easing_sine_ease_in_out(float time, float begin, float change, float duration)
{
  return -change / 2 * (cosf((float)M_PI * time / duration) - 1) + begin;
}
