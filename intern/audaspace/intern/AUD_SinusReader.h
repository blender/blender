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

/** \file audaspace/intern/AUD_SinusReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SINUSREADER_H__
#define __AUD_SINUSREADER_H__

#include "AUD_IReader.h"
#include "AUD_Buffer.h"

/**
 * This class is used for sine tone playback.
 * The sample rate can be specified, the signal is mono.
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
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SINUSREADER_H__
