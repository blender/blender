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

#ifndef AUD_ENVELOPEREADER
#define AUD_ENVELOPEREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class represents an envelope follower.
 */
class AUD_EnvelopeReader : public AUD_EffectReader
{
private:
	/**
	 * Attack b value.
	 */
	const float m_bAttack;

	/**
	 * Release b value.
	 */
	const float m_bRelease;

	/**
	 * Threshold value.
	 */
	const float m_threshold;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The last envelopes buffer.
	 */
	AUD_Buffer m_envelopes;

	// hide copy constructor and operator=
	AUD_EnvelopeReader(const AUD_EnvelopeReader&);
	AUD_EnvelopeReader& operator=(const AUD_EnvelopeReader&);

public:
	/**
	 * Creates a new envelope reader.
	 * \param reader The reader to read from.
	 * \param attack The attack value in seconds.
	 * \param release The release value in seconds.
	 * \param threshold The threshold value.
	 * \param arthreshold The attack/release threshold value.
	 */
	AUD_EnvelopeReader(AUD_IReader* reader, float attack, float release,
					   float threshold, float arthreshold);

	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_ENVELOPEREADER
