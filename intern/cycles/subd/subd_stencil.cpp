/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

