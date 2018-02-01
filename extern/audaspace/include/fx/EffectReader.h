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
 * @file EffectReader.h
 * @ingroup fx
 * The EffectReader class.
 */

#include "IReader.h"

#include <memory>

AUD_NAMESPACE_BEGIN

/**
 * This reader is a base class for all effect readers that take one other reader
 * as input.
 */
class AUD_API EffectReader : public IReader
{
private:
	// delete copy constructor and operator=
	EffectReader(const EffectReader&) = delete;
	EffectReader& operator=(const EffectReader&) = delete;

protected:
	/**
	 * The reader to read from.
	 */
	std::shared_ptr<IReader> m_reader;

public:
	/**
	 * Creates a new effect reader.
	 * \param reader The reader to read from.
	 */
	EffectReader(std::shared_ptr<IReader> reader);

	/**
	 * Destroys the reader.
	 */
	virtual ~EffectReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
