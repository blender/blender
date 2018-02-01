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
 * @file SuperposeReader.h
 * @ingroup sequence
 * The SuperposeReader class.
 */

#include "IReader.h"
#include "util/Buffer.h"

#include <memory>

AUD_NAMESPACE_BEGIN

/**
 * This reader plays two readers with the same specs in parallel.
 */
class AUD_API SuperposeReader : public IReader
{
private:
	/**
	 * The first reader.
	 */
	std::shared_ptr<IReader> m_reader1;

	/**
	 * The second reader.
	 */
	std::shared_ptr<IReader> m_reader2;

	/**
	 * Buffer used for mixing.
	 */
	Buffer m_buffer;

	// delete copy constructor and operator=
	SuperposeReader(const SuperposeReader&) = delete;
	SuperposeReader& operator=(const SuperposeReader&) = delete;

public:
	/**
	 * Creates a new superpose reader.
	 * \param reader1 The first reader to read from.
	 * \param reader2 The second reader to read from.
	 * \exception Exception Thrown if the specs from the readers differ.
	 */
	SuperposeReader(std::shared_ptr<IReader> reader1, std::shared_ptr<IReader> reader2);

	/**
	 * Destroys the reader.
	 */
	virtual ~SuperposeReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
