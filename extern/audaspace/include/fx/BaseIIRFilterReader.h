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
 * @file BaseIIRFilterReader.h
 * @ingroup fx
 * The BaseIIRFilterReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is a base class for infinite impulse response filters.
 */
class AUD_API BaseIIRFilterReader : public EffectReader
{
private:
	/**
	 * Specs.
	 */
	Specs m_specs;

	/**
	 * Length of input samples needed.
	 */
	int m_xlen;

	/**
	 * Length of output samples needed.
	 */
	int m_ylen;

	/**
	 * The last in samples array.
	 */
	sample_t* m_x;

	/**
	 * The last out samples array.
	 */
	sample_t* m_y;

	/**
	 * Position of the current input sample in the input array.
	 */
	int m_xpos;

	/**
	 * Position of the current output sample in the output array.
	 */
	int m_ypos;

	/**
	 * Current channel.
	 */
	int m_channel;

	// delete copy constructor and operator=
	BaseIIRFilterReader(const BaseIIRFilterReader&) = delete;
	BaseIIRFilterReader& operator=(const BaseIIRFilterReader&) = delete;

protected:
	/**
	 * Creates a new base IIR filter reader.
	 * \param reader The reader to read from.
	 * \param in The count of past input samples needed.
	 * \param out The count of past output samples needed.
	 */
	BaseIIRFilterReader(std::shared_ptr<IReader> reader, int in, int out);

	/**
	 * Sets the length for the required input and output samples of the IIR filter.
	 * @param in The amount of past input samples needed, including the current one.
	 * @param out The amount of past output samples needed.
	 */
	void setLengths(int in, int out);

public:
	/**
	 * Retrieves the last input samples.
	 * \param pos The position, valid are 0 (current) or negative values.
	 * \return The sample value.
	 */
	inline sample_t x(int pos)
	{
		return m_x[(m_xpos + pos + m_xlen) % m_xlen * m_specs.channels + m_channel];
	}

	/**
	 * Retrieves the last output samples.
	 * \param pos The position, valid are negative values.
	 * \return The sample value.
	 */
	inline sample_t y(int pos)
	{
		return m_y[(m_ypos + pos + m_ylen) % m_ylen * m_specs.channels + m_channel];
	}

	virtual ~BaseIIRFilterReader();

	virtual void read(int& length, bool& eos, sample_t* buffer);

	/**
	 * Runs the filtering function.
	 * \return The current output sample value.
	 */
	virtual sample_t filter()=0;

	/**
	 * Notifies the filter about a sample rate change.
	 * \param rate The new sample rate.
	 */
	virtual void sampleRateChanged(SampleRate rate);
};

AUD_NAMESPACE_END
