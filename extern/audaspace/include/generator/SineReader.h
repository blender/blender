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
 * @file SineReader.h
 * @ingroup generator
 * The SineReader class.
 */

#include "IReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is used for sine tone playback.
 * The sample rate can be specified, the signal is mono.
 */
class AUD_API SineReader : public IReader
{
private:
	/**
	 * The frequency of the sine wave.
	 */
	float m_frequency;

	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The sample rate for the output.
	 */
	const SampleRate m_sampleRate;

	// delete copy constructor and operator=
	SineReader(const SineReader&) = delete;
	SineReader& operator=(const SineReader&) = delete;

public:
	/**
	 * Creates a new reader.
	 * \param frequency The frequency of the sine wave.
	 * \param sampleRate The output sample rate.
	 */
	SineReader(float frequency, SampleRate sampleRate);

	/**
	 * Sets the frequency of the wave.
	 * @param frequency The new frequency in Hertz.
	 */
	void setFrequency(float frequency);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
