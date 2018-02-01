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
 * @file DelayReader.h
 * @ingroup fx
 * The DelayReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads another reader and delays it.
 */
class AUD_API DelayReader : public EffectReader
{
private:
	/**
	 * The delay level.
	 */
	const int m_delay;

	/**
	 * The remaining delay for playback.
	 */
	int m_remdelay;

	// delete copy constructor and operator=
	DelayReader(const DelayReader&) = delete;
	DelayReader& operator=(const DelayReader&) = delete;

public:
	/**
	 * Creates a new delay reader.
	 * \param reader The reader to read from.
	 * \param delay The delay in seconds.
	 */
	DelayReader(std::shared_ptr<IReader> reader, float delay);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
