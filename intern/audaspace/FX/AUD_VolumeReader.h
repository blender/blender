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

#ifndef AUD_VOLUMEREADER
#define AUD_VOLUMEREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This class reads another reader and changes it's volume.
 */
class AUD_VolumeReader : public AUD_EffectReader
{
private:
	/**
	 * The playback buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The volume level.
	 */
	const float m_volume;

	// hide copy constructor and operator=
	AUD_VolumeReader(const AUD_VolumeReader&);
	AUD_VolumeReader& operator=(const AUD_VolumeReader&);

public:
	/**
	 * Creates a new volume reader.
	 * \param reader The reader to read from.
	 * \param volume The size of the buffer.
	 */
	AUD_VolumeReader(AUD_IReader* reader, float volume);

	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_VOLUMEREADER
