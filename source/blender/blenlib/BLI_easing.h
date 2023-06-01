/* SPDX-FileCopyrightText: 2001 Robert Penner. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

float BLI_easing_back_ease_in(
    float time, float begin, float change, float duration, float overshoot);
float BLI_easing_back_ease_out(
    float time, float begin, float change, float duration, float overshoot);
float BLI_easing_back_ease_in_out(
    float time, float begin, float change, float duration, float overshoot);
float BLI_easing_bounce_ease_out(float time, float begin, float change, float duration);
float BLI_easing_bounce_ease_in(float time, float begin, float change, float duration);
float BLI_easing_bounce_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_in(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_out(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_in(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_out(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_elastic_ease_in(
    float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_out(
    float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_in_out(
    float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_expo_ease_in(float time, float begin, float change, float duration);
float BLI_easing_expo_ease_out(float time, float begin, float change, float duration);
float BLI_easing_expo_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_linear_ease(float time, float begin, float change, float duration);
float BLI_easing_quad_ease_in(float time, float begin, float change, float duration);
float BLI_easing_quad_ease_out(float time, float begin, float change, float duration);
float BLI_easing_quad_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_quart_ease_in(float time, float begin, float change, float duration);
float BLI_easing_quart_ease_out(float time, float begin, float change, float duration);
float BLI_easing_quart_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_quint_ease_in(float time, float begin, float change, float duration);
float BLI_easing_quint_ease_out(float time, float begin, float change, float duration);
float BLI_easing_quint_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_sine_ease_in(float time, float begin, float change, float duration);
float BLI_easing_sine_ease_out(float time, float begin, float change, float duration);
float BLI_easing_sine_ease_in_out(float time, float begin, float change, float duration);

#ifdef __cplusplus
}
#endif
