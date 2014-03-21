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

/** \file blender/blenlib/intern/math_easing.c
 *  \ingroup bli
 */
 
#include <math.h>
#include <stdlib.h>

#include "BLI_math.h"
#include "BLI_math_easing.h"


float BackEaseIn(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0)
		overshoot = 1.70158f;
	time /= duration;
	return change * time * time * ((overshoot + 1) * time - overshoot) + begin;
}

float BackEaseOut(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0)
		overshoot = 1.70158f;
	time = time / duration - 1;
	return change * (time * time * ((overshoot + 1) * time + overshoot) + 1) + begin;
}

float BackEaseInOut(float time, float begin, float change, float duration, float overshoot)
{
	if (overshoot == 0)
		overshoot = 1.70158f; 
	overshoot *= 1.525f;
	if ((time /= duration / 2) < 1) {
		return change / 2 * (time * time * ((overshoot + 1) * time - overshoot)) + begin;
	}
	time -= 2;
	return change / 2 * (time * time * ((overshoot + 1) * time + overshoot) + 2) + begin;

}

float BounceEaseOut(float time, float begin, float change, float duration)
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
		return change * ((7.5625f * time) * time + .984375f) + begin;
	}
}

float BounceEaseIn(float time, float begin, float change, float duration)
{
	return change - BounceEaseOut(duration - time, 0, change, duration) + begin;
}

float BounceEaseInOut(float time, float begin, float change, float duration)
{
	if (time < duration / 2)
		return BounceEaseIn(time * 2, 0, change, duration) * 0.5f + begin;
	else
		return BounceEaseOut(time * 2 - duration, 0, change, duration) * 0.5f + change * 0.5f + begin;
}

float CircEaseIn(float time, float begin, float change, float duration)
{
	time /= duration;
	return -change * (sqrt(1 - time * time) - 1) + begin;
}

float CircEaseOut(float time, float begin, float change, float duration)
{
	time = time / duration - 1;
	return change * sqrt(1 - time * time) + begin;
}

float CircEaseInOut(float time, float begin, float change, float duration)
{
	if ((time /= duration / 2) < 1)
		return -change / 2 * (sqrt(1 - time * time) - 1) + begin;
	time -= 2;
	return change / 2 * (sqrt(1 - time * time) + 1) + begin;
}

float CubicEaseIn(float time, float begin, float change, float duration)
{
	time /= duration;
	return change * time * time * time + begin;
}

float CubicEaseOut(float time, float begin, float change, float duration)
{
	time = time / duration - 1;
	return change * (time * time * time + 1) + begin;
}

float CubicEaseInOut(float time, float begin, float change, float duration)
{
	if ((time /= duration / 2) < 1)
		return change / 2 * time * time * time + begin;
	time -= 2;
	return change / 2 * (time * time * time + 2) + begin;
}

float ElasticEaseIn(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0)
		return begin;

	if ((time /= duration) == 1)
		return begin + change;

	if (!period)
		period = duration * 0.3f;

	if (!amplitude || amplitude < abs(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / (2 * M_PI) * asin(change / amplitude);

	time -= 1;
	return -(amplitude * pow(2, 10 * time) * sin((time * duration - s) * (2 * M_PI) / period)) + begin;
}

float ElasticEaseOut(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0)
		return begin;
	if ((time /= duration) == 1)
		return begin + change;
	if (!period)
		period = duration * 0.3f;
	if (!amplitude || amplitude < abs(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / (2 * M_PI) * asin(change / amplitude);

	return (amplitude * pow(2, -10 * time) * sin((time * duration - s) * (2 * M_PI) / period ) + change + begin);
}

float ElasticEaseInOut(float time, float begin, float change, float duration, float amplitude, float period)
{
	float s;

	if (time == 0)
		return begin;
	if ((time /= duration / 2) == 2)
		return begin + change;
	if (!period)
		period = duration * (0.3f * 1.5f);
	if (!amplitude || amplitude < abs(change)) {
		amplitude = change;
		s = period / 4;
	}
	else
		s = period / ( 2 * M_PI) * asin(change / amplitude);
	if (time < 1) {
		time -= 1;
		return -0.5f * (amplitude * pow(2, 10 * time) * sin((time * duration - s) * (2 * M_PI) / period)) + begin;
	}

	time -= 1;
	return amplitude * pow(2, -10 * time) * sin((time * duration - s) * (2 * M_PI) / period) * 0.5f + change + begin;
}

float ExpoEaseIn(float time, float begin, float change, float duration)
{
	return (time == 0) ? begin : change * pow(2, 10 * (time / duration - 1)) + begin;
}

float ExpoEaseOut(float time, float begin, float change, float duration)
{
	return (time == duration) ? begin + change : change * (-pow(2, -10 * time / duration) + 1) + begin;
}

float ExpoEaseInOut(float time, float begin, float change, float duration)
{
	if (time == 0)
		return begin;
	if (time == duration)
		return begin + change;
	if ((time /= duration / 2) < 1)
		return change/2 * pow(2, 10 * (time - 1)) + begin;
	--time;
	return change / 2 * (-pow(2, -10 * time) + 2) + begin;
}

float LinearEase(float time, float begin, float change, float duration)
{
	return change * time / duration + begin;
}

float QuadEaseIn(float time, float begin, float change, float duration)
{
	time /= duration;
	return change * time * time + begin;
}

float QuadEaseOut(float time, float begin, float change, float duration)
{
	time /= duration;
	return -change * time * (time - 2) + begin;
}

float QuadEaseInOut(float time, float begin, float change, float duration)
{
	if ((time /= duration / 2) < 1)
		return change / 2 * time * time + begin;
	--time;
	return -change / 2 * (time * (time - 2) - 1) + begin;
}


float QuartEaseIn(float time, float begin, float change, float duration)
{
	time /= duration;
	return change * time * time * time * time + begin;
}

float QuartEaseOut(float time, float begin, float change, float duration)
{
	time = time / duration - 1;
	return -change * (time * time * time * time - 1) + begin;
}

float QuartEaseInOut(float time, float begin, float change, float duration)
{
	if ((time /= duration / 2) < 1)
		return change / 2 * time * time * time * time + begin;
	time -= 2;
	return -change/2 * ( time * time * time * time - 2) + begin;
}

float QuintEaseIn(float time, float begin, float change, float duration)
{
	time /= duration;
	return change * time * time * time * time * time + begin;
}
float QuintEaseOut(float time, float begin, float change, float duration)
{
	time = time / duration - 1;
	return change * (time * time * time * time * time + 1) + begin;
}
float QuintEaseInOut(float time, float begin, float change, float duration)
{
	if ((time /= duration / 2) < 1)
		 return change/2 * time * time * time * time * time + begin;
	time -= 2;
	return change / 2 * (time * time * time * time * time + 2) + begin;
}

float SineEaseIn(float time, float begin, float change, float duration)
{
	return -change * cos(time / duration * M_PI_2) + change + begin;
}

float SineEaseOut(float time, float begin, float change, float duration)
{
	return change * sin(time / duration * M_PI_2) + begin;
}

float SineEaseInOut(float time, float begin, float change, float duration)
{
	return -change / 2 * (cos(M_PI * time / duration) - 1) + begin;
}

