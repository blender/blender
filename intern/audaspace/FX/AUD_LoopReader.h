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

#ifndef AUD_LOOPREADER
#define AUD_LOOPREADER

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
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

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
	AUD_LoopReader(AUD_IReader* reader, int loop);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_LOOPREADER
