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

#ifndef __BLI_MATH_EASING_H__
#define __BLI_MATH_EASING_H__

/** \file BLI_math_easing.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

float BackEaseIn(float time, float begin, float change, float duration, float overshoot);
float BackEaseOut(float time, float begin, float change, float duration, float overshoot);
float BackEaseInOut(float time, float begin, float change, float duration, float overshoot);
float BounceEaseOut(float time, float begin, float change, float duration);
float BounceEaseIn(float time, float begin, float change, float duration);
float BounceEaseInOut(float time, float begin, float change, float duration);
float CircEaseIn(float time, float begin, float change, float duration);
float CircEaseOut(float time, float begin, float change, float duration);
float CircEaseInOut(float time, float begin, float change, float duration);
float CubicEaseIn(float time, float begin, float change, float duration);
float CubicEaseOut(float time, float begin, float change, float duration);
float CubicEaseInOut(float time, float begin, float change, float duration);
float ElasticEaseIn(float time, float begin, float change, float duration, float amplitude, float period);
float ElasticEaseOut(float time, float begin, float change, float duration, float amplitude, float period);
float ElasticEaseInOut(float time, float begin, float change, float duration, float amplitude, float period);
float ExpoEaseIn(float time, float begin, float change, float duration);
float ExpoEaseOut(float time, float begin, float change, float duration);
float ExpoEaseInOut(float time, float begin, float change, float duration);
float LinearEase(float time, float begin, float change, float duration);
float QuadEaseIn(float time, float begin, float change, float duration);
float QuadEaseOut(float time, float begin, float change, float duration);
float QuadEaseInOut(float time, float begin, float change, float duration);
float QuartEaseIn(float time, float begin, float change, float duration);
float QuartEaseOut(float time, float begin, float change, float duration);
float QuartEaseInOut(float time, float begin, float change, float duration);
float QuintEaseIn(float time, float begin, float change, float duration);
float QuintEaseOut(float time, float begin, float change, float duration);
float QuintEaseInOut(float time, float begin, float change, float duration);
float SineEaseIn(float time, float begin, float change, float duration);
float SineEaseOut(float time, float begin, float change, float duration);
float SineEaseInOut(float time, float begin, float change, float duration);

#ifdef __cplusplus
}
#endif

#endif // __BLI_MATH_EASING_H__
