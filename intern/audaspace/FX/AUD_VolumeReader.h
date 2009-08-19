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
#include "AUD_ConverterFunctions.h"
class AUD_Buffer;

/**
 * This class reads another reader and changes it's volume.
 */
class AUD_VolumeReader : public AUD_EffectReader
{
private:
	/**
	 * The playback buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The volume level.
	 */
	float m_volume;

	/**
	 * Volume adjustment function.
	 */
	AUD_volume_adjust_f m_adjust;

public:
	/**
	 * Creates a new volume reader.
	 * \param reader The reader to read from.
	 * \param volume The size of the buffer.
	 * \exception AUD_Exception Thrown if the reader specified is NULL.
	 */
	AUD_VolumeReader(AUD_IReader* reader, float volume);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_VolumeReader();

	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_VOLUMEREADER
