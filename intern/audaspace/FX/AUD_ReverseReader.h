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

#ifndef AUD_REVERSEREADER
#define AUD_REVERSEREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class reads another reader from back to front.
 * \note The underlying reader must be a buffer.
 */
class AUD_ReverseReader : public AUD_EffectReader
{
private:
	/**
	 * The sample count.
	 */
	const int m_length;

	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	// hide copy constructor and operator=
	AUD_ReverseReader(const AUD_ReverseReader&);
	AUD_ReverseReader& operator=(const AUD_ReverseReader&);

public:
	/**
	 * Creates a new reverse reader.
	 * \param reader The reader to read from.
	 * \exception AUD_Exception Thrown if the reader specified has an
	 *            undeterminable/infinite length or is not seekable.
	 */
	AUD_ReverseReader(AUD_IReader* reader);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_REVERSEREADER
