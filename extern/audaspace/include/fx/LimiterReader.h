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
 * @file LimiterReader.h
 * @ingroup fx
 * The LimiterReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This reader limits another reader in start and end times.
 */
class AUD_API LimiterReader : public EffectReader
{
private:
	/**
	 * The start sample: inclusive.
	 */
	const float m_start;

	/**
	 * The end sample: exlusive.
	 */
	const float m_end;

	// delete copy constructor and operator=
	LimiterReader(const LimiterReader&) = delete;
	LimiterReader& operator=(const LimiterReader&) = delete;

public:
	/**
	 * Creates a new limiter reader.
	 * \param reader The reader to read from.
	 * \param start The desired start time (inclusive).
	 * \param end The desired end time (sample exklusive), a negative value
	 *            signals that it should play to the end.
	 */
	LimiterReader(std::shared_ptr<IReader> reader, float start = 0, float end = -1);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
