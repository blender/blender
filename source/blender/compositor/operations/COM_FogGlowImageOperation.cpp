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
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_FogGlowImageOperation.h"
#include "BLI_math.h"

FogGlowImageOperation::FogGlowImageOperation(): NodeOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
}
void FogGlowImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	const float cs_r = 1.f, cs_g = 1.f, cs_b = 1.f;

	float u, v, w, d, r;
	
	v = 2.f*(y / (float)512) - 1.f;
	u = 2.f*(x / (float)512) - 1.f;
	r = (u*u + v*v)*256;
	d = -sqrtf(sqrtf(sqrtf(r)));
	w = (0.5f + 0.5f*cos((double)u*M_PI))*(0.5f + 0.5f*cos((double)v*M_PI));
	color[0] = expf(d*cs_r) * w;
	color[1] = expf(d*cs_g) * w;
	color[2] = expf(d*cs_b) * w;
	color[3] = 1.0f;
}

void FogGlowImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	resolution[0] = 512;
	resolution[1] = 512;
}
