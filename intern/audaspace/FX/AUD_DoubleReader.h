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

#ifndef AUD_DOUBLEREADER
#define AUD_DOUBLEREADER

#include "AUD_IReader.h"
#include "AUD_Buffer.h"

/**
 * This reader plays two readers with the same specs sequently.
 */
class AUD_DoubleReader : public AUD_IReader
{
private:
	/**
	 * The first reader.
	 */
	AUD_IReader* m_reader1;

	/**
	 * The second reader.
	 */
	AUD_IReader* m_reader2;

	/**
	 * Whether we've reached the end of the first reader.
	 */
	bool m_finished1;

	/**
	 * The playback buffer for the intersecting part.
	 */
	AUD_Buffer m_buffer;

	// hide copy constructor and operator=
	AUD_DoubleReader(const AUD_DoubleReader&);
	AUD_DoubleReader& operator=(const AUD_DoubleReader&);

public:
	/**
	 * Creates a new ping pong reader.
	 * \param reader1 The first reader to read from.
	 * \param reader2 The second reader to read from.
	 * \exception AUD_Exception Thrown if the specs from the readers differ.
	 */
	AUD_DoubleReader(AUD_IReader* reader1, AUD_IReader* reader2);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_DoubleReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_DOUBLEREADER
