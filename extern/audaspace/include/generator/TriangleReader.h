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
 * @file TriangleReader.h
 * @ingroup generator
 * The TriangleReader class.
 */

#include "IReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is used for sawtooth tone playback.
 * The output format is in the 16 bit format and stereo, the sample rate can be
 * specified.
 * As the two channels both play the same the output could also be mono, but
 * in most cases this will result in having to resample for output, so stereo
 * sound is created directly.
 */
class AUD_API TriangleReader : public IReader
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
	 * The value of the current sample.
	 */
	float m_sample;

	/**
	 * The sample rate for the output.
	 */
	const SampleRate m_sampleRate;

	// delete copy constructor and operator=
	TriangleReader(const TriangleReader&) = delete;
	TriangleReader& operator=(const TriangleReader&) = delete;

public:
	/**
	 * Creates a new reader.
	 * \param frequency The frequency of the sine wave.
	 * \param sampleRate The output sample rate.
	 */
	TriangleReader(float frequency, SampleRate sampleRate);

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
	virtual void read(int & length, bool &eos, sample_t* buffer);
};

AUD_NAMESPACE_END
