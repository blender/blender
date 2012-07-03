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
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
}
void ScaleOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	this->m_inputXOperation = this->getInputSocketReader(1);
	this->m_inputYOperation = this->getInputSocketReader(2);
	this->m_centerX = this->getWidth() / 2.0;
	this->m_centerY = this->getHeight() / 2.0;
}

void ScaleOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
}


void ScaleOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	float scaleX[4];
	float scaleY[4];

	this->m_inputXOperation->read(scaleX, x, y, sampler, inputBuffers);
	this->m_inputYOperation->read(scaleY, x, y, sampler, inputBuffers);

	const float scx = scaleX[0];
	const float scy = scaleY[0];

	float nx = this->m_centerX + (x - this->m_centerX) / scx;
	float ny = this->m_centerY + (y - this->m_centerY) / scy;
	this->m_inputOperation->read(color, nx, ny, sampler, inputBuffers);
}

bool ScaleOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	float scaleX[4];
	float scaleY[4];

	this->m_inputXOperation->read(scaleX, 0, 0, COM_PS_NEAREST, NULL);
	this->m_inputYOperation->read(scaleY, 0, 0, COM_PS_NEAREST, NULL);

	const float scx = scaleX[0];
	const float scy = scaleY[0];

	newInput.xmax = this->m_centerX + (input->xmax - this->m_centerX) / scx;
	newInput.xmin = this->m_centerX + (input->xmin - this->m_centerX) / scx;
	newInput.ymax = this->m_centerY + (input->ymax - this->m_centerY) / scy;
	newInput.ymin = this->m_centerY + (input->ymin - this->m_centerY) / scy;

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
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
}
void ScaleAbsoluteOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	this->m_inputXOperation = this->getInputSocketReader(1);
	this->m_inputYOperation = this->getInputSocketReader(2);
	this->m_centerX = this->getWidth() / 2.0;
	this->m_centerY = this->getHeight() / 2.0;
}

void ScaleAbsoluteOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
}


void ScaleAbsoluteOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	float scaleX[4];
	float scaleY[4];

	this->m_inputXOperation->read(scaleX, x, y, sampler, inputBuffers);
	this->m_inputYOperation->read(scaleY, x, y, sampler, inputBuffers);

	const float scx = scaleX[0]; // target absolute scale
	const float scy = scaleY[0]; // target absolute scale

	const float width = this->getWidth();
	const float height = this->getHeight();
	//div
	float relativeXScale = scx / width;
	float relativeYScale = scy / height;

	float nx = this->m_centerX + (x - this->m_centerX) / relativeXScale;
	float ny = this->m_centerY + (y - this->m_centerY) / relativeYScale;

	this->m_inputOperation->read(color, nx, ny, sampler, inputBuffers);
}

bool ScaleAbsoluteOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	float scaleX[4];
	float scaleY[4];

	this->m_inputXOperation->read(scaleX, 0, 0, COM_PS_NEAREST, NULL);
	this->m_inputYOperation->read(scaleY, 0, 0, COM_PS_NEAREST, NULL);

	const float scx = scaleX[0];
	const float scy = scaleY[0];
	const float width = this->getWidth();
	const float height = this->getHeight();
	//div
	float relateveXScale = scx / width;
	float relateveYScale = scy / height;

	newInput.xmax = this->m_centerX + (input->xmax - this->m_centerX) / relateveXScale;
	newInput.xmin = this->m_centerX + (input->xmin - this->m_centerX) / relateveXScale;
	newInput.ymax = this->m_centerY + (input->ymax - this->m_centerY) / relateveYScale;
	newInput.ymin = this->m_centerY + (input->ymin - this->m_centerY) / relateveYScale;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}


// Absolute fixed siez
ScaleFixedSizeOperation::ScaleFixedSizeOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputOperation = NULL;
}
void ScaleFixedSizeOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	this->m_relX = this->m_inputOperation->getWidth() / (float)this->m_newWidth;
	this->m_relY = this->m_inputOperation->getHeight() / (float)this->m_newHeight;


	/* *** all the options below are for a fairly special case - camera framing *** */
	if (this->m_offsetX != 0.0f || this->m_offsetY != 0.0f) {
		this->m_is_offset = true;

		if (this->m_newWidth > this->m_newHeight) {
			this->m_offsetX *= this->m_newWidth;
			this->m_offsetY *= this->m_newWidth;
		}
		else {
			this->m_offsetX *= this->m_newHeight;
			this->m_offsetY *= this->m_newHeight;
		}
	}

	if (this->m_is_aspect) {
		/* apply aspect from clip */
		const float w_src = this->m_inputOperation->getWidth();
		const float h_src = this->m_inputOperation->getHeight();

		/* destination aspect is already applied from the camera frame */
		const float w_dst = this->m_newWidth;
		const float h_dst = this->m_newHeight;

		const float asp_src = w_src / h_src;
		const float asp_dst = w_dst / h_dst;

		if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
			if ((asp_src > asp_dst) == (this->m_is_crop == true)) {
				/* fit X */
				const float div = asp_src / asp_dst;
				this->m_relX /= div;
				this->m_offsetX += ((w_src - (w_src * div)) / (w_src / w_dst)) / 2.0f;
			}
			else {
				/* fit Y */
				const float div = asp_dst / asp_src;
				this->m_relY /= div;
				this->m_offsetY += ((h_src - (h_src * div)) / (h_src / h_dst)) / 2.0f;
			}

			this->m_is_offset = true;
		}
	}
	/* *** end framing options *** */
}

void ScaleFixedSizeOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}


void ScaleFixedSizeOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
#ifdef USE_FORCE_BICUBIC
	sampler = COM_PS_BICUBIC;
#endif

	if (this->m_is_offset) {
		float nx = ((x - this->m_offsetX) * this->m_relX);
		float ny = ((y - this->m_offsetY) * this->m_relY);
		this->m_inputOperation->read(color, nx, ny, sampler, inputBuffers);
	}
	else {
		this->m_inputOperation->read(color, x * this->m_relX, y * this->m_relY, sampler, inputBuffers);
	}
}

bool ScaleFixedSizeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmax = input->xmax * this->m_relX;
	newInput.xmin = input->xmin * this->m_relX;
	newInput.ymax = input->ymax * this->m_relY;
	newInput.ymin = input->ymin * this->m_relY;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void ScaleFixedSizeOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	unsigned int nr[2];
	nr[0] = this->m_newWidth;
	nr[1] = this->m_newHeight;
	NodeOperation::determineResolution(resolution, nr);
	resolution[0] = this->m_newWidth;
	resolution[1] = this->m_newHeight;
}
