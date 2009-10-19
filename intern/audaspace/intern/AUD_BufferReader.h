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

#ifndef AUD_BUFFERREADER
#define AUD_BUFFERREADER

#include "AUD_IReader.h"
#include "AUD_Reference.h"
class AUD_Buffer;

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
	AUD_Reference<AUD_Buffer> m_buffer;

	/**
	 * The specification of the sample data in the buffer.
	 */
	AUD_Specs m_specs;

public:
	/**
	 * Creates a new buffer reader.
	 * \param buffer The buffer to read from.
	 * \param specs The specification of the sample data in the buffer.
	 */
	AUD_BufferReader(AUD_Reference<AUD_Buffer> buffer, AUD_Specs specs);

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_BUFFERREADER
