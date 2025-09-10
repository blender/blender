/*******************************************************************************
 * Copyright 2009-2021 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

/**
 * @file RingBuffer.h
 * @ingroup util
 * The RingBuffer class.
 */

#include "Audaspace.h"
#include "Buffer.h"

#include <cstddef>

AUD_NAMESPACE_BEGIN

/**
 * This class is a simple ring buffer in RAM which is 32 Byte aligned and provides
 * functionality for concurrent reading and writting without locks.
 */
class AUD_API RingBuffer
{
private:
	/// The buffer storing the actual data.
	Buffer m_buffer;

	/// The reading pointer.
	volatile size_t m_read;

	/// The writing pointer.
	volatile size_t m_write;

	// delete copy constructor and operator=
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

public:
	/**
	 * Creates a new ring buffer.
	 * \param size The size of the buffer in bytes.
	 */
	RingBuffer(int size = 0);

	/**
	 * Returns the pointer to the ring buffer in memory.
	 */
	const sample_t* getBuffer() const;

	/**
	 * Returns the pointer to the ring buffer in memory.
	 */
	sample_t* getBuffer();

	/**
	 * Returns the size of the ring buffer in bytes.
	 */
	int getSize() const;

	size_t getReadSize() const;

	size_t getWriteSize() const;

	size_t read(data_t* target, size_t size);

	size_t write(data_t* source, size_t size);

	void clear();

	/**
	 * Resets the ring buffer to a state where nothing has been written or read.
	 */
	void reset();

	/**
	 * Resizes the ring buffer.
	 * \param size The new size of the ring buffer, measured in bytes.
	 */
	void resize(int size);

	/**
	 * Makes sure the ring buffer has a minimum size.
	 * If size is >= current size, nothing will happen.
	 * Otherwise the ring buffer is resized with keep as parameter.
	 * \param size The new minimum size of the ring buffer, measured in bytes.
	 */
	void assureSize(int size);
};

AUD_NAMESPACE_END
