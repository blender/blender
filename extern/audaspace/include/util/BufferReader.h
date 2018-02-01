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
 * @file BufferReader.h
 * @ingroup util
 * The BufferReader class.
 */

#include "IReader.h"

#include <memory>

AUD_NAMESPACE_BEGIN

class Buffer;

/**
 * This class represents a simple reader from a buffer that exists in memory.
 * \warning Notice that the buffer is not multi-threading ready, so changing the
 *          buffer while the reader is reading is potentially dangerous.
 */
class AUD_API BufferReader : public IReader
{
private:
	/**
	 * The current position in the buffer.
	 */
	int m_position;

	/**
	 * The buffer that is read.
	 */
	std::shared_ptr<Buffer> m_buffer;

	/**
	 * The specification of the sample data in the buffer.
	 */
	Specs m_specs;

	// delete copy constructor and operator=
	BufferReader(const BufferReader&) = delete;
	BufferReader& operator=(const BufferReader&) = delete;

public:
	/**
	 * Creates a new buffer reader.
	 * \param buffer The buffer to read from.
	 * \param specs The specification of the sample data in the buffer.
	 */
	BufferReader(std::shared_ptr<Buffer> buffer, Specs specs);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
