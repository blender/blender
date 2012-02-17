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

/** \file audaspace/FX/AUD_SuperposeReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_SUPERPOSEREADER_H__
#define __AUD_SUPERPOSEREADER_H__

#include "AUD_IReader.h"
#include "AUD_Buffer.h"
#include "AUD_Reference.h"

/**
 * This reader plays two readers with the same specs in parallel.
 */
class AUD_SuperposeReader : public AUD_IReader
{
private:
	/**
	 * The first reader.
	 */
	AUD_Reference<AUD_IReader> m_reader1;

	/**
	 * The second reader.
	 */
	AUD_Reference<AUD_IReader> m_reader2;

	/**
	 * Buffer used for mixing.
	 */
	AUD_Buffer m_buffer;

	// hide copy constructor and operator=
	AUD_SuperposeReader(const AUD_SuperposeReader&);
	AUD_SuperposeReader& operator=(const AUD_SuperposeReader&);

public:
	/**
	 * Creates a new superpose reader.
	 * \param reader1 The first reader to read from.
	 * \param reader2 The second reader to read from.
	 * \exception AUD_Exception Thrown if the specs from the readers differ.
	 */
	AUD_SuperposeReader(AUD_Reference<AUD_IReader> reader1, AUD_Reference<AUD_IReader> reader2);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_SuperposeReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SUPERPOSEREADER_H__
