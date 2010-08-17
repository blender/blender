/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_SINUSREADER
#define AUD_SINUSREADER

#include "AUD_IReader.h"
#include "AUD_Buffer.h"

/**
 * This class is used for sine tone playback.
 * The output format is in the 16 bit format and stereo, the sample rate can be
 * specified.
 * As the two channels both play the same the output could also be mono, but
 * in most cases this will result in having to resample for output, so stereo
 * sound is created directly.
 */
class AUD_SinusReader : public AUD_IReader
{
private:
	/**
	 * The frequency of the sine wave.
	 */
	const float m_frequency;

	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The sample rate for the output.
	 */
	const AUD_SampleRate m_sampleRate;

	// hide copy constructor and operator=
	AUD_SinusReader(const AUD_SinusReader&);
	AUD_SinusReader& operator=(const AUD_SinusReader&);

public:
	/**
	 * Creates a new reader.
	 * \param frequency The frequency of the sine wave.
	 * \param sampleRate The output sample rate.
	 */
	AUD_SinusReader(float frequency, AUD_SampleRate sampleRate);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SINUSREADER
