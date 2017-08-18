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
 * @file LinearResampleReader.h
 * @ingroup respec
 * The LinearResampleReader class.
 */

#include "respec/ResampleReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This resampling reader does simple first-order hold resampling.
 */
class AUD_API LinearResampleReader : public ResampleReader
{
private:
	/**
	 * The reader channels.
	 */
	Channels m_channels;

	/**
	 * The position in the cache.
	 */
	float m_cache_pos;

	/**
	 * The sound output buffer.
	 */
	Buffer m_buffer;

	/**
	 * The input caching buffer.
	 */
	Buffer m_cache;

	/**
	 * Whether the cache contains valid data.
	 */
	bool m_cache_ok;

	// delete copy constructor and operator=
	LinearResampleReader(const LinearResampleReader&) = delete;
	LinearResampleReader& operator=(const LinearResampleReader&) = delete;

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param rate The target sampling rate.
	 */
	LinearResampleReader(std::shared_ptr<IReader> reader, SampleRate rate);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
