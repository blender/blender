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
 * @file ReverseReader.h
 * @ingroup fx
 * The ReverseReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads another reader from back to front.
 * \note The underlying reader must be seekable.
 */
class AUD_API ReverseReader : public EffectReader
{
private:
	/**
	 * The sample count.
	 */
	const int m_length;

	/**
	 * The current position.
	 */
	int m_position;

	// delete copy constructor and operator=
	ReverseReader(const ReverseReader&) = delete;
	ReverseReader& operator=(const ReverseReader&) = delete;

public:
	/**
	 * Creates a new reverse reader.
	 * \param reader The reader to read from.
	 * \exception Exception Thrown if the reader specified has an
	 *            undeterminable/infinite length or is not seekable.
	 */
	ReverseReader(std::shared_ptr<IReader> reader);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
