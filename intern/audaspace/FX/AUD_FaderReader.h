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

#ifndef AUD_FADERREADER
#define AUD_FADERREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class fades another reader.
 * If the fading type is AUD_FADE_IN, everything before the fading start will be
 * silenced, for AUD_FADE_OUT that's true for everything after fading ends.
 */
class AUD_FaderReader : public AUD_EffectReader
{
private:
	/**
	 * The fading type.
	 */
	const AUD_FadeType m_type;

	/**
	 * The fading start.
	 */
	const float m_start;

	/**
	 * The fading length.
	 */
	const float m_length;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * Whether the buffer is empty.
	 */
	bool m_empty;

	// hide copy constructor and operator=
	AUD_FaderReader(const AUD_FaderReader&);
	AUD_FaderReader& operator=(const AUD_FaderReader&);

public:
	/**
	 * Creates a new fader reader.
	 * \param type The fading type.
	 * \param start The time where fading should start in seconds.
	 * \param length How long fading should last in seconds.
	 */
	AUD_FaderReader(AUD_IReader* reader, AUD_FadeType type,
					float start,float length);

	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_FADERREADER
