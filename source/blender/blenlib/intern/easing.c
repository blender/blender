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

/** \file blender/blenlib/intern/easing.c
 *  \ingroup bli
 */

#include "BLI_math_base.h"

#include "BLI_easing.h"  /* own include */

#include "BLI_strict_flags.h"


float BLI_easing_back_ease_in(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0.0f)
		overshoot = 1.70158f;
	time /= duration;
	return change * time * time * ((overshoot + 1) * time - overshoot) + begin;
}

float BLI_easing_back_ease_out(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0.0f)
		overshoot = 1.70158f;
	time = time / duration - 1;
	return change * (time * time * ((overshoot + 1) * time + overshoot) + 1) + begin;
}

float BLI_easing_back_ease_in_out(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0.0f)
		overshoot = 1.70158f; 
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
	else if (time < (2 / 2.75f)) {
		time -= (1.5f / 2.75f);
		return change * ((7.5625f * time) * time + 0.75f) + begin;
	}
	else if (time < (2.5f / 2.75f)) {
		time -= (2.25f / 2.75f);
		return change * ((7.5625f * time) * time + 0.9375f) + begin;
	}
	else {
		time -= (2.625f / 2.75f);
		return change * ((7.5625f * time) * time + 0.984375f) + begin;
	}
}

float BLI_easing_bounce_ease_in(float time, float begin, float change, float duration)
{
	return change - BLI_easing_bounce_ease_out(duration - time, 0, change, duration) + begin;
}

float BLI_easing_bounce_ease_in_out(float time, float begin, float change, float duration)
{
	if (time < duration / 2)
		return BLI_easing_bounce_ease_in(time * 2, 0, change, duration) * 0.5f + begin;
	else
		return BLI_easing_bounce_ease_out(time * 2 - duration, 0, change, duration) * 0.5f + change * 0.5f + begin;
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
	if ((time /= duration / 2) < 1.0f)
		return -change / 2 * (sqrtf(1 - time * time) - 1) + begin;
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
	if ((time /= duration / 2) < 1.0f)
		return change / 2 * time * time * time + begin;
	time -= 2.0f;
	return change / 2 * (time * time * time + 2) + begin;
}

float BLI_easing_elastic_ease_in(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0.0f)
		return begin;

	if ((time /= duration) == 1.0f)
		return begin + change;

	if (!period)
		period = duration * 0.3f;

	if (!amplitude || amplitude < fabsf(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / (2 * (float)M_PI) * asinf(change / amplitude);

	time -= 1.0f;
	return -(amplitude * powf(2, 10 * time) * sinf((time * duration - s) * (2 * (float)M_PI) / period)) + begin;
}

float BLI_easing_elastic_ease_out(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0.0f)
		return begin;
	if ((time /= duration) == 1.0f)
		return begin + change;
	if (!period)
		period = duration * 0.3f;
	if (!amplitude || amplitude < fabsf(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / (2 * (float)M_PI) * asinf(change / amplitude);

	return (amplitude * powf(2, -10 * time) * sinf((time * duration - s) * (2 * (float)M_PI) / period) + change + begin);
}

float BLI_easing_elastic_ease_in_out(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0.0f)
		return begin;
	if ((time /= duration / 2) == 2.0f)
		return begin + change;
	if (!period)
		period = duration * (0.3f * 1.5f);
	if (!amplitude || amplitude < fabsf(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / (2 * (float)M_PI) * asinf(change / amplitude);
	if (time < 1.0f) {
		time -= 1.0f;
		return -0.5f * (amplitude * powf(2, 10 * time) * sinf((time * duration - s) * (2 * (float)M_PI) / period)) + begin;
	}

	time -= 1.0f;
	return amplitude * powf(2, -10 * time) * sinf((time * duration - s) * (2 * (float)M_PI) / period) * 0.5f + change + begin;
}

float BLI_easing_expo_ease_in(float time, float begin, float change, float duration)
{
	return (time == 0.0f) ? begin : change * powf(2, 10 * (time / duration - 1)) + begin;
}

float BLI_easing_expo_ease_out(float time, float begin, float change, float duration)
{
	return (time == duration) ? begin + change : change * (-powf(2, -10 * time / duration) + 1) + begin;
}

float BLI_easing_expo_ease_in_out(float time, float begin, float change, float duration)
{
	if (time == 0.0f)
		return begin;
	if (time == duration)
		return begin + change;
	if ((time /= duration / 2) < 1)
		return change / 2 * powf(2, 10 * (time - 1)) + begin;
	time -= 1.0f;
	return change / 2 * (-powf(2, -10 * time) + 2) + begin;
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
	if ((time /= duration / 2) < 1.0f)
		return change / 2 * time * time + begin;
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
	if ((time /= duration / 2) < 1.0f)
		return change / 2 * time * time * time * time + begin;
	time -= 2.0f;
	return -change / 2 * ( time * time * time * time - 2) + begin;
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
	if ((time /= duration / 2) < 1.0f)
		 return change / 2 * time * time * time * time * time + begin;
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
