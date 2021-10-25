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

/** \file audaspace/fftw/AUD_BandPassReader.h
 *  \ingroup audfftw
 */


#ifndef __AUD_BANDPASSREADER_H__
#define __AUD_BANDPASSREADER_H__

#include <fftw3.h>

#include "AUD_EffectReader.h"
class AUD_Buffer;

/**
 * This class only passes a specific frequency band of another reader.
 */
class AUD_BandPassReader : public AUD_EffectReader
{
private:
	/**
	 * The playback buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The input buffer for fourier transformations.
	 */
	AUD_Buffer *m_in;

	/**
	 * The output buffer for fourier transformations.
	 */
	AUD_Buffer *m_out;

	/**
	 * The lowest passed frequency.
	 */
	float m_low;

	/**
	 * The highest passed frequency.
	 */
	float m_high;

	/**
	 * The fftw plan for forward transformation.
	 */
	fftw_plan m_forward;

	/**
	 * The fftw plan for backward transformation.
	 */
	fftw_plan m_backward;

	/**
	 * The length of the plans.
	 */
	int m_length;

public:
	/**
	 * Creates a new band pass reader.
	 * \param reader The reader to read from.
	 * \param low The lowest passed frequency.
	 * \param high The highest passed frequency.
	 */
	AUD_BandPassReader(AUD_IReader* reader, float low, float high);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_BandPassReader();

	virtual AUD_ReaderType getType();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //__AUD_BANDPASSREADER_H__
