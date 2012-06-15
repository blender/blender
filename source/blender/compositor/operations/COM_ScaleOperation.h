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

#ifndef _COM_ScaleOperation_h_
#define _COM_ScaleOperation_h_

#include "COM_NodeOperation.h"

class ScaleOperation : public NodeOperation {
private:
	SocketReader *inputOperation;
	SocketReader *inputXOperation;
	SocketReader *inputYOperation;
	float centerX;
	float centerY;
public:
	ScaleOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void initExecution();
	void deinitExecution();
};

class ScaleAbsoluteOperation : public NodeOperation {
	SocketReader *inputOperation;
	SocketReader *inputXOperation;
	SocketReader *inputYOperation;
	float centerX;
	float centerY;

public:
	ScaleAbsoluteOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void initExecution();
	void deinitExecution();
};

class ScaleFixedSizeOperation : public NodeOperation {
	SocketReader *inputOperation;
	int newWidth;
	int newHeight;
	float relX;
	float relY;

	/* center is only used for aspect correction */
	float offsetX;
	float offsetY;
	bool is_aspect;
	bool is_crop;
	/* set from other properties on initialization,
	 * check if we need to apply offset */
	bool is_offset;
public:
	ScaleFixedSizeOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void initExecution();
	void deinitExecution();
	void setNewWidth(int width) { this->newWidth = width; }
	void setNewHeight(int height) { this->newHeight = height; }
	void setIsAspect(bool is_aspect) { this->is_aspect = is_aspect; }
	void setIsCrop(bool is_crop) { this->is_crop = is_crop; }
	void setOffset(float x, float y) { this->offsetX = x; this->offsetY = y; }
};

#endif
