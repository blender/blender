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

/** \file audaspace/FX/AUD_LoopReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_LOOPREADER_H__
#define __AUD_LOOPREADER_H__

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class reads another reader and loops it.
 * \note The other reader must be seekable.
 */
class AUD_LoopReader : public AUD_EffectReader
{
private:
	/**
	 * The loop count.
	 */
	const int m_count;

	/**
	 * The left loop count.
	 */
	int m_left;

	// hide copy constructor and operator=
	AUD_LoopReader(const AUD_LoopReader&);
	AUD_LoopReader& operator=(const AUD_LoopReader&);

public:
	/**
	 * Creates a new loop reader.
	 * \param reader The reader to read from.
	 * \param loop The desired loop count, negative values result in endless
	 *        looping.
	 */
	AUD_LoopReader(AUD_Reference<AUD_IReader> reader, int loop);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_LOOPREADER_H__
