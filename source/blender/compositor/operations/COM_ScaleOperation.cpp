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

#include "COM_ScaleOperation.h"

#define USE_FORCE_BICUBIC
/* XXX - ignore input and use default from old compositor,
 * could become an option like the transform node - campbell */

ScaleOperation::ScaleOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}
void ScaleOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
	this->inputXOperation = this->getInputSocketReader(1);
	this->inputYOperation = this->getInputSocketReader(2);
	this->centerX = this->getWidth() / 2.0;
	this->centerY = this->getHeight() / 2.0;
}

void ScaleOperation::deinitExecution()
{
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}


void ScaleOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	float scaleX[4];
	float scaleY[4];

	this->inputXOperation->read(scaleX, x, y, sampler, inputBuffers);
	this->inputYOperation->read(scaleY, x, y, sampler, inputBuffers);

	const float scx = scaleX[0];
	const float scy = scaleY[0];

	float nx = this->centerX + (x - this->centerX) / scx;
	float ny = this->centerY + (y - this->centerY) / scy;
	this->inputOperation->read(color, nx, ny, sampler, inputBuffers);
}

bool ScaleOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	float scaleX[4];
	float scaleY[4];

	this->inputXOperation->read(scaleX, 0, 0, COM_PS_NEAREST, NULL);
	this->inputYOperation->read(scaleY, 0, 0, COM_PS_NEAREST, NULL);

	const float scx = scaleX[0];
	const float scy = scaleY[0];

	newInput.xmax = this->centerX + (input->xmax - this->centerX) / scx;
	newInput.xmin = this->centerX + (input->xmin - this->centerX) / scx;
	newInput.ymax = this->centerY + (input->ymax - this->centerY) / scy;
	newInput.ymin = this->centerY + (input->ymin - this->centerY) / scy;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}


// SCALE ABSOLUTE
ScaleAbsoluteOperation::ScaleAbsoluteOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}
void ScaleAbsoluteOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
	this->inputXOperation = this->getInputSocketReader(1);
	this->inputYOperation = this->getInputSocketReader(2);
	this->centerX = this->getWidth() / 2.0;
	this->centerY = this->getHeight() / 2.0;
}

void ScaleAbsoluteOperation::deinitExecution()
{
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}


void ScaleAbsoluteOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	float scaleX[4];
	float scaleY[4];

	this->inputXOperation->read(scaleX, x, y, sampler, inputBuffers);
	this->inputYOperation->read(scaleY, x, y, sampler, inputBuffers);

	const float scx = scaleX[0]; // target absolute scale
	const float scy = scaleY[0]; // target absolute scale

	const float width = this->getWidth();
	const float height = this->getHeight();
	//div
	float relativeXScale = scx / width;
	float relativeYScale = scy / height;

	float nx = this->centerX + (x - this->centerX) / relativeXScale;
	float ny = this->centerY + (y - this->centerY) / relativeYScale;

	this->inputOperation->read(color, nx, ny, sampler, inputBuffers);
}

bool ScaleAbsoluteOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	float scaleX[4];
	float scaleY[4];

	this->inputXOperation->read(scaleX, 0, 0, COM_PS_NEAREST, NULL);
	this->inputYOperation->read(scaleY, 0, 0, COM_PS_NEAREST, NULL);

	const float scx = scaleX[0];
	const float scy = scaleY[0];
	const float width = this->getWidth();
	const float height = this->getHeight();
	//div
	float relateveXScale = scx / width;
	float relateveYScale = scy / height;

	newInput.xmax = this->centerX + (input->xmax - this->centerX) / relateveXScale;
	newInput.xmin = this->centerX + (input->xmin - this->centerX) / relateveXScale;
	newInput.ymax = this->centerY + (input->ymax - this->centerY) / relateveYScale;
	newInput.ymin = this->centerY + (input->ymin - this->centerY) / relateveYScale;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}


// Absolute fixed siez
ScaleFixedSizeOperation::ScaleFixedSizeOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
}
void ScaleFixedSizeOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
	this->relX = inputOperation->getWidth() / (float)this->newWidth;
	this->relY = inputOperation->getHeight() / (float)this->newHeight;


	/* *** all the options below are for a fairly special case - camera framing *** */
	if (this->offsetX != 0.0f || this->offsetY != 0.0f) {
		this->is_offset = true;

		if (this->newWidth > this->newHeight) {
			this->offsetX *= this->newWidth;
			this->offsetY *= this->newWidth;
		}
		else {
			this->offsetX *= this->newHeight;
			this->offsetY *= this->newHeight;
		}
	}

	if (this->is_aspect) {
		/* apply aspect from clip */
		const float w_src = inputOperation->getWidth();
		const float h_src = inputOperation->getHeight();

		/* destination aspect is already applied from the camera frame */
		const float w_dst = this->newWidth;
		const float h_dst = this->newHeight;

		const float asp_src = w_src / h_src;
		const float asp_dst = w_dst / h_dst;

		if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
			if ((asp_src > asp_dst) == (this->is_crop == true)) {
				/* fit X */
				const float div = asp_src / asp_dst;
				this->relX /= div;
				this->offsetX += ((w_src - (w_src * div)) / (w_src / w_dst)) / 2.0f;
			}
			else {
				/* fit Y */
				const float div = asp_dst / asp_src;
				this->relY /= div;
				this->offsetY += ((h_src - (h_src * div)) / (h_src / h_dst)) / 2.0f;
			}

			this->is_offset = true;
		}
	}
	/* *** end framing options *** */
}

void ScaleFixedSizeOperation::deinitExecution()
{
	this->inputOperation = NULL;
}


void ScaleFixedSizeOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	if (this->is_offset) {
		float nx = ((x - this->offsetX) * relX);
		float ny = ((y - this->offsetY) * relY);
		this->inputOperation->read(color, nx, ny, sampler, inputBuffers);
	}
	else {
		this->inputOperation->read(color, x * relX, y * relY, sampler, inputBuffers);
	}
}

bool ScaleFixedSizeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmax = input->xmax * relX;
	newInput.xmin = input->xmin * relX;
	newInput.ymax = input->ymax * relY;
	newInput.ymin = input->ymin * relY;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void ScaleFixedSizeOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	unsigned int nr[2];
	nr[0] = newWidth;
	nr[1] = newHeight;
	NodeOperation::determineResolution(resolution, nr);
	resolution[0] = newWidth;
	resolution[1] = newHeight;
}
