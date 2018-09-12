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

#ifndef __COM_SOCKETREADER_H__
#define __COM_SOCKETREADER_H__
#include "BLI_rect.h"
#include "COM_defines.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

typedef enum PixelSampler {
	COM_PS_NEAREST = 0,
	COM_PS_BILINEAR = 1,
	COM_PS_BICUBIC = 2
} PixelSampler;

class MemoryBuffer;
/**
 * \brief Helper class for reading socket data.
 * Only use this class for dispatching (un-ary and n-ary) executions.
 * \ingroup Execution
 */
class SocketReader {
private:
protected:
	/**
	 * \brief Holds the width of the output of this operation.
	 */
	unsigned int m_width;

	/**
	 * \brief Holds the height of the output of this operation.
	 */
	unsigned int m_height;


	/**
	 * \brief calculate a single pixel
	 * \note this method is called for non-complex
	 * \param result is a float[4] array to store the result
	 * \param x the x-coordinate of the pixel to calculate in image space
	 * \param y the y-coordinate of the pixel to calculate in image space
	 * \param inputBuffers chunks that can be read by their ReadBufferOperation.
	 */
	virtual void executePixelSampled(float /*output*/[4],
	                                 float /*x*/,
	                                 float /*y*/,
	                                 PixelSampler /*sampler*/) { }

	/**
	 * \brief calculate a single pixel
	 * \note this method is called for complex
	 * \param result is a float[4] array to store the result
	 * \param x the x-coordinate of the pixel to calculate in image space
	 * \param y the y-coordinate of the pixel to calculate in image space
	 * \param inputBuffers chunks that can be read by their ReadBufferOperation.
	 * \param chunkData chunk specific data a during execution time.
	 */
	virtual void executePixel(float output[4], int x, int y, void * /*chunkData*/) {
		executePixelSampled(output, x, y, COM_PS_NEAREST);
	}

	/**
	 * \brief calculate a single pixel using an EWA filter
	 * \note this method is called for complex
	 * \param result is a float[4] array to store the result
	 * \param x the x-coordinate of the pixel to calculate in image space
	 * \param y the y-coordinate of the pixel to calculate in image space
	 * \param dx
	 * \param dy
	 * \param inputBuffers chunks that can be read by their ReadBufferOperation.
	 */
	virtual void executePixelFiltered(float /*output*/[4],
	                                  float /*x*/, float /*y*/,
	                                  float /*dx*/[2], float /*dy*/[2]) {}

public:
	inline void readSampled(float result[4], float x, float y, PixelSampler sampler) {
		executePixelSampled(result, x, y, sampler);
	}
	inline void read(float result[4], int x, int y, void *chunkData) {
		executePixel(result, x, y, chunkData);
	}
	inline void readFiltered(float result[4], float x, float y, float dx[2], float dy[2]) {
		executePixelFiltered(result, x, y, dx, dy);
	}

	virtual void *initializeTileData(rcti * /*rect*/) { return 0; }
	virtual void deinitializeTileData(rcti * /*rect*/, void * /*data*/) {}

	virtual ~SocketReader() {}

	virtual MemoryBuffer *getInputMemoryBuffer(MemoryBuffer ** /*memoryBuffers*/) { return 0; }

	inline unsigned int getWidth() const { return this->m_width; }
	inline unsigned int getHeight() const { return this->m_height; }

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SocketReader")
#endif
};

#endif /* __COM_SOCKETREADER_H__ */
