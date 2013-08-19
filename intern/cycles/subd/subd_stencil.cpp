/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include "subd_stencil.h"

#include "util_debug.h"
#include "util_math.h"

CCL_NAMESPACE_BEGIN

StencilMask::StencilMask()
{
}

StencilMask::StencilMask(int size)
{
	/* initialize weights to zero. */
	weights.resize(size, 0.0f);
}

void StencilMask::resize(int size)
{
	weights.resize(size, 0.0f);
}

StencilMask& StencilMask::operator=(float value)
{
	const int size = weights.size();
	for(int i = 0; i < size; i++)
		weights[i] = value;

	return *this;
}

void StencilMask::operator+=(const StencilMask& mask)
{
	assert(mask.size() == size());
	
	const int size = weights.size();
	for(int i = 0; i < size; i++)
		weights[i] += mask.weights[i];
}

void StencilMask::operator-=(const StencilMask& mask)
{
	assert(mask.size() == size());
	
	const int size = weights.size();
	for(int i = 0; i < size; i++)
		weights[i] -= mask.weights[i];
}

void StencilMask::operator*=(float scale)
{
	const int size = weights.size();

	for(int i = 0; i < size; i++)
		weights[i] *= scale;
}

void StencilMask::operator/=(float scale)
{
	*this *= 1.0f/scale;
}

float StencilMask::sum() const
{
	float total = 0.0f;
	const int size = weights.size();

	for(int i = 0; i < size; i++)
		total += weights[i];
	
	return total;
}

bool StencilMask::is_normalized() const
{
	return fabsf(sum() - 1.0f) < 0.0001f;
}

void StencilMask::normalize()
{
	*this /= sum();
}

CCL_NAMESPACE_END

