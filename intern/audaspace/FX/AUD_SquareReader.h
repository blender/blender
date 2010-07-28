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

#ifndef AUD_SQUAREREADER
#define AUD_SQUAREREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class changes another signal into a square signal.
 */
class AUD_SquareReader : public AUD_EffectReader
{
private:
	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The threshold level.
	 */
	const float m_threshold;

	// hide copy constructor and operator=
	AUD_SquareReader(const AUD_SquareReader&);
	AUD_SquareReader& operator=(const AUD_SquareReader&);

public:
	/**
	 * Creates a new square reader.
	 * \param reader The reader to read from.
	 * \param threshold The size of the buffer.
	 */
	AUD_SquareReader(AUD_IReader* reader, float threshold);

	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SQUAREREADER
