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

/** \file audaspace/FX/AUD_LimiterReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_LIMITERREADER_H__
#define __AUD_LIMITERREADER_H__

#include "AUD_EffectReader.h"

/**
 * This reader limits another reader in start and end times.
 */
class AUD_LimiterReader : public AUD_EffectReader
{
private:
	/**
	 * The start sample: inclusive.
	 */
	const float m_start;

	/**
	 * The end sample: exlusive.
	 */
	const float m_end;

	// hide copy constructor and operator=
	AUD_LimiterReader(const AUD_LimiterReader&);
	AUD_LimiterReader& operator=(const AUD_LimiterReader&);

public:
	/**
	 * Creates a new limiter reader.
	 * \param reader The reader to read from.
	 * \param start The desired start time (inclusive).
	 * \param end The desired end time (sample exklusive), a negative value
	 *            signals that it should play to the end.
	 */
	AUD_LimiterReader(AUD_Reference<AUD_IReader> reader, float start = 0, float end = -1);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_LIMITERREADER_H__
