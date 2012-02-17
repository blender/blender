/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/FX/AUD_BaseIIRFilterReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_BASEIIRFILTERREADER_H__
#define __AUD_BASEIIRFILTERREADER_H__

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class is a base class for infinite impulse response filters.
 */
class AUD_BaseIIRFilterReader : public AUD_EffectReader
{
private:
	/**
	 * Specs.
	 */
	AUD_Specs m_specs;

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
	AUD_BaseIIRFilterReader(AUD_Reference<AUD_IReader> reader, int in, int out);

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

	virtual ~AUD_BaseIIRFilterReader();

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
	virtual void sampleRateChanged(AUD_SampleRate rate);
};

#endif //__AUD_BASEIIRFILTERREADER_H__
