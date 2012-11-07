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

/** \file audaspace/intern/AUD_BufferReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_BUFFERREADER_H__
#define __AUD_BUFFERREADER_H__

#include "AUD_IReader.h"
class AUD_Buffer;

#include <boost/shared_ptr.hpp>

/**
 * This class represents a simple reader from a buffer that exists in memory.
 * \warning Notice that the buffer used for creating the reader must exist as
 *          long as the reader exists.
 */
class AUD_BufferReader : public AUD_IReader
{
private:
	/**
	 * The current position in the buffer.
	 */
	int m_position;

	/**
	 * The buffer that is read.
	 */
	boost::shared_ptr<AUD_Buffer> m_buffer;

	/**
	 * The specification of the sample data in the buffer.
	 */
	AUD_Specs m_specs;

	// hide copy constructor and operator=
	AUD_BufferReader(const AUD_BufferReader&);
	AUD_BufferReader& operator=(const AUD_BufferReader&);

public:
	/**
	 * Creates a new buffer reader.
	 * \param buffer The buffer to read from.
	 * \param specs The specification of the sample data in the buffer.
	 */
	AUD_BufferReader(boost::shared_ptr<AUD_Buffer> buffer, AUD_Specs specs);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_BUFFERREADER_H__
