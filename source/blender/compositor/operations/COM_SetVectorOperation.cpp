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

#include "COM_SetVectorOperation.h"
#include "COM_defines.h"

SetVectorOperation::SetVectorOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VECTOR);
}

void SetVectorOperation::executePixelSampled(float output[4],
                                             float /*x*/, float /*y*/,
                                             PixelSampler /*sampler*/)
{
	output[0] = this->m_x;
	output[1] = this->m_y;
	output[2] = this->m_z;
}

void SetVectorOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	resolution[0] = preferredResolution[0];
	resolution[1] = preferredResolution[1];
}
