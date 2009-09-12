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
class AUD_Buffer;

/**
 * This class reads another reader from back to front.
 * \note The underlying reader must be a buffer.
 */
class AUD_ReverseReader : public AUD_EffectReader
{
private:
	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The sample count.
	 */
	int m_length;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer* m_buffer;

public:
	/**
	 * Creates a new reverse reader.
	 * \param reader The reader to read from.
	 * \exception AUD_Exception Thrown if the reader specified is NULL or not
	 *            a buffer.
	 */
	AUD_ReverseReader(AUD_IReader* reader);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_ReverseReader();

	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_REVERSEREADER
