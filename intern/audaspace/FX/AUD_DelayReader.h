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

#ifndef AUD_DELAYREADER
#define AUD_DELAYREADER

#include "AUD_EffectReader.h"
class AUD_Buffer;

/**
 * This class reads another reader and changes it's delay.
 */
class AUD_DelayReader : public AUD_EffectReader
{
private:
	/**
	 * The playback buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The delay level.
	 */
	int m_delay;

	/**
	 * The remaining delay for playback.
	 */
	int m_remdelay;

public:
	/**
	 * Creates a new delay reader.
	 * \param reader The reader to read from.
	 * \param delay The delay in seconds.
	 * \exception AUD_Exception Thrown if the reader specified is NULL.
	 */
	AUD_DelayReader(AUD_IReader* reader, float delay);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_DelayReader();

	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_DELAYREADER
