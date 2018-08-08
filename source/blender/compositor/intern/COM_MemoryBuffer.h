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

class MemoryBuffer;

#ifndef __COM_MEMORYBUFFER_H__
#define __COM_MEMORYBUFFER_H__

#include "COM_ExecutionGroup.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"

extern "C" {
#  include "BLI_math.h"
#  include "BLI_rect.h"
}

/**
 * @brief state of a memory buffer
 * @ingroup Memory
 */
typedef enum MemoryBufferState {
	/** @brief memory has been allocated on creator device and CPU machine, but kernel has not been executed */
	COM_MB_ALLOCATED = 1,
	/** @brief memory is available for use, content has been created */
	COM_MB_AVAILABLE = 2,
	/** @brief chunk is consolidated from other chunks. special state.*/
	COM_MB_TEMPORARILY = 6
} MemoryBufferState;

typedef enum MemoryBufferExtend {
	COM_MB_CLIP,
	COM_MB_EXTEND,
	COM_MB_REPEAT
} MemoryBufferExtend;

class MemoryProxy;

/**
 * @brief a MemoryBuffer contains access to the data of a chunk
 */
class MemoryBuffer {
private:
	/**
	 * @brief proxy of the memory (same for all chunks in the same buffer)
	 */
	MemoryProxy *m_memoryProxy;

	/**
	 * @brief the type of buffer COM_DT_VALUE, COM_DT_VECTOR, COM_DT_COLOR
	 */
	DataType m_datatype;


	/**
	 * @brief region of this buffer inside relative to the MemoryProxy
	 */
	rcti m_rect;

	/**
	 * brief refers to the chunknumber within the executiongroup where related to the MemoryProxy
	 * @see memoryProxy
	 */
	unsigned int m_chunkNumber;

	/**
	 * @brief state of the buffer
	 */
	MemoryBufferState m_state;

	/**
	 * @brief the actual float buffer/data
	 */
	float *m_buffer;

	/**
	 * @brief the number of channels of a single value in the buffer.
	 * For value buffers this is 1, vector 3 and color 4
	 */
	unsigned int m_num_channels;

	int m_width;
	int m_height;

public:
	/**
	 * @brief construct new MemoryBuffer for a chunk
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect);

	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, rcti *rect);

	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 */
	MemoryBuffer(DataType datatype, rcti *rect);

	/**
	 * @brief destructor
	 */
	~MemoryBuffer();

	/**
	 * @brief read the ChunkNumber of this MemoryBuffer
	 */
	unsigned int getChunkNumber() { return this->m_chunkNumber; }

	unsigned int get_num_channels() { return this->m_num_channels; }

	/**
	 * @brief get the data of this MemoryBuffer
	 * @note buffer should already be available in memory
	 */
	float *getBuffer() { return this->m_buffer; }

	/**
	 * @brief after execution the state will be set to available by calling this method
	 */
	void setCreatedState()
	{
		this->m_state = COM_MB_AVAILABLE;
	}

	inline void wrap_pixel(int &x, int &y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
	{
		int w = this->m_width;
		int h = this->m_height;
		x = x - m_rect.xmin;
		y = y - m_rect.ymin;

		switch (extend_x) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (x < 0) x = 0;
				if (x >= w) x = w;
				break;
			case COM_MB_REPEAT:
				x = (x >= 0.0f ? (x % w) : (x % w) + w);
				break;
		}

		switch (extend_y) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (y < 0) y = 0;
				if (y >= h) y = h;
				break;
			case COM_MB_REPEAT:
				y = (y >= 0.0f ? (y % h) : (y % h) + h);
				break;
		}
	}

	inline void wrap_pixel(float &x, float &y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
	{
		float w = (float)this->m_width;
		float h = (float)this->m_height;
		x = x - m_rect.xmin;
		y = y - m_rect.ymin;

		switch (extend_x) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (x < 0) x = 0.0f;
				if (x >= w) x = w;
				break;
			case COM_MB_REPEAT:
				x = fmodf(x, w);
				break;
		}

		switch (extend_y) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (y < 0) y = 0.0f;
				if (y >= h) y = h;
				break;
			case COM_MB_REPEAT:
				y = fmodf(y, h);
				break;
		}
	}

	inline void read(float *result, int x, int y,
	                 MemoryBufferExtend extend_x = COM_MB_CLIP,
	                 MemoryBufferExtend extend_y = COM_MB_CLIP)
	{
		bool clip_x = (extend_x == COM_MB_CLIP && (x < m_rect.xmin || x >= m_rect.xmax));
		bool clip_y = (extend_y == COM_MB_CLIP && (y < m_rect.ymin || y >= m_rect.ymax));
		if (clip_x || clip_y) {
			/* clip result outside rect is zero */
			memset(result, 0, this->m_num_channels * sizeof(float));
		}
		else {
			int u = x;
			int v = y;
			this->wrap_pixel(u, v, extend_x, extend_y);
			const int offset = (this->m_width * y + x) * this->m_num_channels;
			float *buffer = &this->m_buffer[offset];
			memcpy(result, buffer, sizeof(float) * this->m_num_channels);
		}
	}

	inline void readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x = COM_MB_CLIP,
	                        MemoryBufferExtend extend_y = COM_MB_CLIP)
	{
		int u = x;
		int v = y;

		this->wrap_pixel(u, v, extend_x, extend_y);
		const int offset = (this->m_width * v + u) * this->m_num_channels;

		BLI_assert(offset >= 0);
		BLI_assert(offset < this->determineBufferSize() * this->m_num_channels);
		BLI_assert(!(extend_x == COM_MB_CLIP && (u < m_rect.xmin || u >= m_rect.xmax)) &&
		           !(extend_y == COM_MB_CLIP && (v < m_rect.ymin || v >= m_rect.ymax)));
#if 0
		/* always true */
		BLI_assert((int)(MEM_allocN_len(this->m_buffer) / sizeof(*this->m_buffer)) ==
		           (int)(this->determineBufferSize() * COM_NUMBER_OF_CHANNELS));
#endif
		float *buffer = &this->m_buffer[offset];
		memcpy(result, buffer, sizeof(float) * this->m_num_channels);
	}

	void writePixel(int x, int y, const float color[4]);
	void addPixel(int x, int y, const float color[4]);
	inline void readBilinear(float *result, float x, float y,
	                         MemoryBufferExtend extend_x = COM_MB_CLIP,
	                         MemoryBufferExtend extend_y = COM_MB_CLIP)
	{
		float u = x;
		float v = y;
		this->wrap_pixel(u, v, extend_x, extend_y);
		if ((extend_x != COM_MB_REPEAT && (u < 0.0f || u >= this->m_width)) ||
		    (extend_y != COM_MB_REPEAT && (v < 0.0f || v >= this->m_height)))
		{
			copy_vn_fl(result, this->m_num_channels, 0.0f);
			return;
		}
		BLI_bilinear_interpolation_wrap_fl(
		        this->m_buffer, result, this->m_width, this->m_height, this->m_num_channels, u, v,
		        extend_x == COM_MB_REPEAT, extend_y == COM_MB_REPEAT);
	}

	void readEWA(float *result, const float uv[2], const float derivatives[2][2]);

	/**
	 * @brief is this MemoryBuffer a temporarily buffer (based on an area, not on a chunk)
	 */
	inline const bool isTemporarily() const { return this->m_state == COM_MB_TEMPORARILY; }

	/**
	 * @brief add the content from otherBuffer to this MemoryBuffer
	 * @param otherBuffer source buffer
	 *
	 * @note take care when running this on a new buffer since it wont fill in
	 *       uninitialized values in areas where the buffers don't overlap.
	 */
	void copyContentFrom(MemoryBuffer *otherBuffer);

	/**
	 * @brief get the rect of this MemoryBuffer
	 */
	rcti *getRect() { return &this->m_rect; }

	/**
	 * @brief get the width of this MemoryBuffer
	 */
	int getWidth() const;

	/**
	 * @brief get the height of this MemoryBuffer
	 */
	int getHeight() const;

	/**
	 * @brief clear the buffer. Make all pixels black transparent.
	 */
	void clear();

	MemoryBuffer *duplicate();

	float getMaximumValue();
	float getMaximumValue(rcti *rect);
private:
	unsigned int determineBufferSize();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBuffer")
#endif
};

#endif
