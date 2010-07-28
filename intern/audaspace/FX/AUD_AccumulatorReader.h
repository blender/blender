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

#ifndef AUD_ACCUMULATORREADER
#define AUD_ACCUMULATORREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class represents an accumulator.
 */
class AUD_AccumulatorReader : public AUD_EffectReader
{
private:
	/**
	 * Whether the accumulator is additive.
	 */
	const bool m_additive;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The sums of the specific channels.
	 */
	AUD_Buffer m_sums;

	/**
	 * The previous results of the specific channels.
	 */
	AUD_Buffer m_prevs;

	// hide copy constructor and operator=
	AUD_AccumulatorReader(const AUD_AccumulatorReader&);
	AUD_AccumulatorReader& operator=(const AUD_AccumulatorReader&);

public:
	/**
	 * Creates a new accumulator reader.
	 * \param reader The reader to read from.
	 * \param additive Whether the accumulator is additive.
	 */
	AUD_AccumulatorReader(AUD_IReader* reader, bool additive);

	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_ACCUMULATORREADER
