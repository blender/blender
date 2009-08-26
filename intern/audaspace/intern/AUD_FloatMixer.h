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

#ifndef AUD_FLOATMIXER
#define AUD_FLOATMIXER

#include "AUD_IMixer.h"
#include "AUD_ConverterFunctions.h"
class AUD_ConverterFactory;
class AUD_SRCResampleFactory;
class AUD_ChannelMapperFactory;
class AUD_Buffer;
#include <list>

struct AUD_FloatMixerBuffer
{
	sample_t* buffer;
	int length;
	float volume;
};

/**
 * This class is able to mix two audiosignals with floats.
 */
class AUD_FloatMixer : public AUD_IMixer
{
private:
	/**
	 * The converter factory that converts all readers for superposition.
	 */
	AUD_ConverterFactory* m_converter;

	/**
	 * The resampling factory that resamples all readers for superposition.
	 */
	AUD_SRCResampleFactory* m_resampler;

	/**
	 * The channel mapper factory that maps all readers for superposition.
	 */
	AUD_ChannelMapperFactory* m_mapper;

	/**
	 * The list of buffers to superpose.
	 */
	std::list<AUD_FloatMixerBuffer> m_buffers;

	/**
	 * The output specification.
	 */
	AUD_Specs m_specs;

	/**
	 * The temporary mixing buffer.
	 */
	AUD_Buffer* m_buffer;

	/**
	 * Converter function.
	 */
	AUD_convert_f m_convert;

public:
	/**
	 * Creates the mixer.
	 */
	AUD_FloatMixer();

	virtual ~AUD_FloatMixer();

	virtual AUD_IReader* prepare(AUD_IReader* reader);
	virtual void setSpecs(AUD_Specs specs);
	virtual void add(sample_t* buffer, AUD_Specs specs, int length,
					 float volume);
	virtual void superpose(sample_t* buffer, int length, float volume);
};

#endif //AUD_FLOATMIXER
