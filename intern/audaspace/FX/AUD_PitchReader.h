/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/FX/AUD_PitchReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_PITCHREADER_H__
#define __AUD_PITCHREADER_H__

#include "AUD_EffectReader.h"

/**
 * This class reads another reader and changes it's pitch.
 */
class AUD_PitchReader : public AUD_EffectReader
{
private:
	/**
	 * The pitch level.
	 */
	float m_pitch;

	// hide copy constructor and operator=
	AUD_PitchReader(const AUD_PitchReader&);
	AUD_PitchReader& operator=(const AUD_PitchReader&);

public:
	/**
	 * Creates a new pitch reader.
	 * \param reader The reader to read from.
	 * \param pitch The pitch value.
	 */
	AUD_PitchReader(AUD_Reference<AUD_IReader> reader, float pitch);

	virtual AUD_Specs getSpecs() const;

	/**
	 * Retrieves the pitch.
	 * \return The current pitch value.
	 */
	float getPitch() const;

	/**
	 * Sets the pitch.
	 * \param pitch The new pitch value.
	 */
	void setPitch(float pitch);
};

#endif //__AUD_PITCHREADER_H__
