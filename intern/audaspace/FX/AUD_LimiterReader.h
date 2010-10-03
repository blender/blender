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

#ifndef AUD_LIMITERREADER
#define AUD_LIMITERREADER

#include "AUD_EffectReader.h"

/**
 * This reader limits another reader in start and end sample.
 */
class AUD_LimiterReader : public AUD_EffectReader
{
private:
	/**
	 * The start sample: inclusive.
	 */
	const int m_start;

	/**
	 * The end sample: exlusive.
	 */
	const int m_end;

	// hide copy constructor and operator=
	AUD_LimiterReader(const AUD_LimiterReader&);
	AUD_LimiterReader& operator=(const AUD_LimiterReader&);

public:
	/**
	 * Creates a new limiter reader.
	 * \param reader The reader to read from.
	 * \param start The desired start sample (inclusive).
	 * \param end The desired end sample (exklusive), a negative value signals
	 *            that it should play to the end.
	 */
	AUD_LimiterReader(AUD_IReader* reader, float start = 0, float end = -1);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_LIMITERREADER
