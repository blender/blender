/*
 * Copyright © 2001 Robert Penner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the author nor the names of contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BLI_EASING_H__
#define __BLI_EASING_H__

/** \file BLI_easing.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

float BLI_easing_back_ease_in(float time, float begin, float change, float duration, float overshoot);
float BLI_easing_back_ease_out(float time, float begin, float change, float duration, float overshoot);
float BLI_easing_back_ease_in_out(float time, float begin, float change, float duration, float overshoot);
float BLI_easing_bounce_ease_out(float time, float begin, float change, float duration);
float BLI_easing_bounce_ease_in(float time, float begin, float change, float duration);
float BLI_easing_bounce_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_in(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_out(float time, float begin, float change, float duration);
float BLI_easing_circ_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_in(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_out(float time, float begin, float change, float duration);
float BLI_easing_cubic_ease_in_out(float time, float begin, float change, float duration);
float BLI_easing_elastic_ease_in(float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_out(float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_in_out(float time, float begin, float change, float duration, float amplitude, float period);
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

#endif  /* __BLI_EASING_H__ */
