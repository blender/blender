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

#ifndef _COM_SocketReader_h
#define _COM_SocketReader_h
#include "BLI_rect.h"
#include "COM_defines.h"

typedef enum PixelSampler {
	COM_PS_NEAREST,
	COM_PS_BILINEAR,
	COM_PS_BICUBIC
} PixelSampler;

class MemoryBuffer;
/**
 * @brief Helper class for reading socket data.
 * Only use this class for dispatching (un-ary and n-ary) executions.
 * @ingroup Execution
 */
class SocketReader {
private:
protected:
	/**
	 * @brief Holds the width of the output of this operation.
	 */
	unsigned int width;

	/**
	 * @brief Holds the height of the output of this operation.
	 */
	unsigned int height;


	/**
	 * @brief calculate a single pixel
	 * @note this method is called for non-complex
	 * @param result is a float[4] array to store the result
	 * @param x the x-coordinate of the pixel to calculate in image space
	 * @param y the y-coordinate of the pixel to calculate in image space
	 * @param inputBuffers chunks that can be read by their ReadBufferOperation.
	 */
	virtual void executePixel(float *result, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {}

	/**
	 * @brief calculate a single pixel
	 * @note this method is called for complex
	 * @param result is a float[4] array to store the result
	 * @param x the x-coordinate of the pixel to calculate in image space
	 * @param y the y-coordinate of the pixel to calculate in image space
	 * @param inputBuffers chunks that can be read by their ReadBufferOperation.
	 * @param chunkData chunk specific data a during execution time.
	 */
	virtual void executePixel(float *result, int x, int y, MemoryBuffer *inputBuffers[], void *chunkData) {
		executePixel(result, x, y, COM_PS_NEAREST, inputBuffers);
	}

	/**
	 * @brief calculate a single pixel using an EWA filter
	 * @note this method is called for complex
	 * @param result is a float[4] array to store the result
	 * @param x the x-coordinate of the pixel to calculate in image space
	 * @param y the y-coordinate of the pixel to calculate in image space
	 * @param dx
	 * @param dy
	 * @param inputBuffers chunks that can be read by their ReadBufferOperation.
	 */
	virtual void executePixel(float *result, float x, float y, float dx, float dy, MemoryBuffer *inputBuffers[]) {}

public:
	inline void read(float *result, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
		executePixel(result, x, y, sampler, inputBuffers);
	}
	inline void read(float *result, int x, int y, MemoryBuffer *inputBuffers[], void *chunkData) {
		executePixel(result, x, y, inputBuffers, chunkData);
	}
	inline void read(float *result, float x, float y, float dx, float dy, MemoryBuffer *inputBuffers[]) {
		executePixel(result, x, y, dx, dy, inputBuffers);
	}

	virtual void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) { return 0; }
	virtual void deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data) {
	}
	
	virtual MemoryBuffer *getInputMemoryBuffer(MemoryBuffer **memoryBuffers) { return 0; }


	inline const unsigned int getWidth() const { return this->width; }
	inline const unsigned int getHeight() const { return this->height; }
};

#endif
