/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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
 * @file Buffer.h
 * @ingroup util
 * The Buffer class.
 */

#include "Audaspace.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is a simple buffer in RAM which is 32 Byte aligned and provides
 * resize functionality.
 */
class AUD_API Buffer
{
private:
	/// The size of the buffer in bytes.
	long long m_size;

	/// The pointer to the buffer memory.
	data_t* m_buffer;

	// delete copy constructor and operator=
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

public:
	/**
	 * Creates a new buffer.
	 * \param size The size of the buffer in bytes.
	 */
	Buffer(long long size = 0);

	/**
	 * Destroys the buffer.
	 */
	~Buffer();

	/**
	 * Returns the pointer to the buffer in memory.
	 */
	const sample_t* getBuffer() const;

	/**
	 * Returns the pointer to the buffer in memory.
	 */
	sample_t* getBuffer();

	/**
	 * Returns the size of the buffer in bytes.
	 */
	long long getSize() const;

	/**
	 * Resizes the buffer.
	 * \param size The new size of the buffer, measured in bytes.
	 * \param keep Whether to keep the old data. If the new buffer is smaller,
	 *        the data at the end will be lost.
	 */
	void resize(long long size, bool keep = false);

	/**
	 * Makes sure the buffer has a minimum size.
	 * If size is >= current size, nothing will happen.
	 * Otherwise the buffer is resized with keep as parameter.
	 * \param size The new minimum size of the buffer, measured in bytes.
	 * \param keep Whether to keep the old data. If the new buffer is smaller,
	 *        the data at the end will be lost.
	 */
	void assureSize(long long size, bool keep = false);
};

AUD_NAMESPACE_END
