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

/** \file audaspace/intern/AUD_SilenceReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SILENCEREADER_H__
#define __AUD_SILENCEREADER_H__

#include "AUD_IReader.h"
#include "AUD_Buffer.h"

/**
 * This class is used for silence playback.
 * The signal generated is 44.1kHz mono.
 */
class AUD_SilenceReader : public AUD_IReader
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	// hide copy constructor and operator=
	AUD_SilenceReader(const AUD_SilenceReader&);
	AUD_SilenceReader& operator=(const AUD_SilenceReader&);

public:
	/**
	 * Creates a new reader.
	 */
	AUD_SilenceReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SILENCEREADER_H__
