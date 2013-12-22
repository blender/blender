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

#include "COM_SplitOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "BKE_image.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

extern "C" {
#  include "MEM_guardedalloc.h"
#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"
}


SplitOperation::SplitOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_image1Input = NULL;
	this->m_image2Input = NULL;
}

void SplitOperation::initExecution()
{
	// When initializing the tree during initial load the width and height can be zero.
	this->m_image1Input = getInputSocketReader(0);
	this->m_image2Input = getInputSocketReader(1);
}

void SplitOperation::deinitExecution()
{
	this->m_image1Input = NULL;
	this->m_image2Input = NULL;
}

void SplitOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	int perc = this->m_xSplit ? this->m_splitPercentage * this->getWidth() / 100.0f : this->m_splitPercentage * this->getHeight() / 100.0f;
	bool image1 = this->m_xSplit ? x > perc : y > perc;
	if (image1) {
		this->m_image1Input->readSampled(output, x, y, COM_PS_NEAREST);
	}
	else {
		this->m_image2Input->readSampled(output, x, y, COM_PS_NEAREST);
	}
}

void SplitOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	unsigned int tempPreferredResolution[2] = {0, 0};
	unsigned int tempResolution[2];

	this->getInputSocket(0)->determineResolution(tempResolution, tempPreferredResolution);
	this->setResolutionInputSocketIndex((tempResolution[0] && tempResolution[1]) ? 0 : 1);

	NodeOperation::determineResolution(resolution, preferredResolution);
}
