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
 * @file LoopReader.h
 * @ingroup fx
 * The LoopReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads another reader and loops it.
 * \note The other reader must be seekable.
 */
class AUD_API LoopReader : public EffectReader
{
private:
	/**
	 * The loop count.
	 */
	const int m_count;

	/**
	 * The left loop count.
	 */
	int m_left;

	// delete copy constructor and operator=
	LoopReader(const LoopReader&) = delete;
	LoopReader& operator=(const LoopReader&) = delete;

public:
	/**
	 * Creates a new loop reader.
	 * \param reader The reader to read from.
	 * \param loop The desired loop count, negative values result in endless
	 *        looping.
	 */
	LoopReader(std::shared_ptr<IReader> reader, int loop);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
