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

/** \file audaspace/FX/AUD_DoubleReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_DOUBLEREADER_H__
#define __AUD_DOUBLEREADER_H__

#include "AUD_IReader.h"
#include "AUD_Buffer.h"
#include "AUD_Reference.h"

/**
 * This reader plays two readers sequently.
 */
class AUD_DoubleReader : public AUD_IReader
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
	 * Whether we've reached the end of the first reader.
	 */
	bool m_finished1;

	// hide copy constructor and operator=
	AUD_DoubleReader(const AUD_DoubleReader&);
	AUD_DoubleReader& operator=(const AUD_DoubleReader&);

public:
	/**
	 * Creates a new double reader.
	 * \param reader1 The first reader to read from.
	 * \param reader2 The second reader to read from.
	 */
	AUD_DoubleReader(AUD_Reference<AUD_IReader> reader1, AUD_Reference<AUD_IReader> reader2);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_DoubleReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_DOUBLEREADER_H__
