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

#ifndef AUD_BASEIIRFILTERREADER
#define AUD_BASEIIRFILTERREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class is a base class for infinite impulse response filters.
 */
class AUD_BaseIIRFilterReader : public AUD_EffectReader
{
private:
	/**
	 * Channel count.
	 */
	const int m_channels;

	/**
	 * Length of input samples needed.
	 */
	const int m_xlen;

	/**
	 * Length of output samples needed.
	 */
	const int m_ylen;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

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

	// hide copy constructor and operator=
	AUD_BaseIIRFilterReader(const AUD_BaseIIRFilterReader&);
	AUD_BaseIIRFilterReader& operator=(const AUD_BaseIIRFilterReader&);

protected:
	/**
	 * Creates a new base IIR filter reader.
	 * \param reader The reader to read from.
	 * \param in The count of past input samples needed.
	 * \param out The count of past output samples needed.
	 */
	AUD_BaseIIRFilterReader(AUD_IReader* reader, int in, int out);

public:
	inline sample_t x(int pos)
	{
		return m_x[(m_xpos + pos + m_xlen) % m_xlen * m_channels + m_channel];
	}

	inline sample_t y(int pos)
	{
		return m_y[(m_ypos + pos + m_ylen) % m_ylen * m_channels + m_channel];
	}

	virtual ~AUD_BaseIIRFilterReader();

	virtual void read(int & length, sample_t* & buffer);

	virtual sample_t filter()=0;
};

#endif //AUD_BASEIIRFILTERREADER
