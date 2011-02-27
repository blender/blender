/*
 * $Id$
 *
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

/** \file audaspace/intern/AUD_ChannelMapperReader.h
 *  \ingroup audaspaceintern
 */


#ifndef AUD_CHANNELMAPPERREADER
#define AUD_CHANNELMAPPERREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

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
	AUD_Buffer m_buffer;

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

	// hide copy constructor and operator=
	AUD_ChannelMapperReader(const AUD_ChannelMapperReader&);
	AUD_ChannelMapperReader& operator=(const AUD_ChannelMapperReader&);

public:
	/**
	 * Creates a channel mapper reader.
	 * \param reader The reader to map.
	 * \param mapping The mapping specification as two dimensional float array.
	 */
	AUD_ChannelMapperReader(AUD_IReader* reader, float **mapping);

	/**
	 * Destroys the reader.
	 */
	~AUD_ChannelMapperReader();

	virtual AUD_Specs getSpecs() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_CHANNELMAPPERREADER
