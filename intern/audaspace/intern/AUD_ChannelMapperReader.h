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

#ifndef AUD_CHANNELMAPPERREADER
#define AUD_CHANNELMAPPERREADER

#include "AUD_EffectReader.h"
class AUD_Buffer;

/**
 * This class maps a sound source's channels to a specific output channel count.
 * \note The input sample format must be float.
 */
class AUD_ChannelMapperReader : public AUD_EffectReader
{
private:
	/**
	 * The sound output buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The output specification.
	 */
	AUD_Specs m_specs;

	/**
	 * The channel count of the reader.
	 */
	int m_rch;

	/**
	 * The mapping specification.
	 */
	float **m_mapping;

public:
	/**
	 * Creates a channel mapper reader.
	 * \param reader The reader to map.
	 * \param mapping The mapping specification as two dimensional float array.
	 * \exception AUD_Exception Thrown if the reader is NULL.
	 */
	AUD_ChannelMapperReader(AUD_IReader* reader, float **mapping);
	/**
	 * Destroys the reader.
	 */
	~AUD_ChannelMapperReader();

	virtual AUD_Specs getSpecs();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_CHANNELMAPPERREADER
