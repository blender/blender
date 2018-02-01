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
 * @file ResampleReader.h
 * @ingroup respec
 * The ResampleReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This is the base class for all resampling readers.
 */
class AUD_API ResampleReader : public EffectReader
{
protected:
	/**
	 * The target sampling rate.
	 */
	SampleRate m_rate;

	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param rate The target sampling rate.
	 */
	ResampleReader(std::shared_ptr<IReader> reader, SampleRate rate);

public:
	/**
	 * Sets the sample rate.
	 * \param rate The target sampling rate.
	 */
	virtual void setRate(SampleRate rate);

	/**
	 * Retrieves the sample rate.
	 * \return The target sampling rate.
	 */
	virtual SampleRate getRate();
};

AUD_NAMESPACE_END
