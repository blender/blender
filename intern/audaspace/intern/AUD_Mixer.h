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

#ifndef AUD_MIXER
#define AUD_MIXER

#define AUD_MIXER_RESAMPLER AUD_SRCResampleFactory

#include "AUD_ConverterFunctions.h"
class AUD_ConverterFactory;
class AUD_MIXER_RESAMPLER;
class AUD_ChannelMapperFactory;
class AUD_Buffer;
class AUD_IReader;
#include <list>

struct AUD_MixerBuffer
{
	sample_t* buffer;
	int start;
	int length;
	float volume;
};

/**
 * This class is able to mix audiosignals of different channel count and sample
 * rate and convert it to a specific output format.
 * It uses a default ChannelMapperFactory and a SRCResampleFactory for
 * the perparation.
 */
class AUD_Mixer
{
private:
	/**
	 * The resampling factory that resamples all readers for superposition.
	 */
	AUD_MIXER_RESAMPLER* m_resampler;

	/**
	 * The channel mapper factory that maps all readers for superposition.
	 */
	AUD_ChannelMapperFactory* m_mapper;

	/**
	 * The list of buffers to superpose.
	 */
	std::list<AUD_MixerBuffer> m_buffers;

	/**
	 * The output specification.
	 */
	AUD_DeviceSpecs m_specs;

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
	AUD_Mixer();

	/**
	 * Destroys the mixer.
	 */
	~AUD_Mixer();

	/**
	 * This funuction prepares a reader for playback.
	 * \param reader The reader to prepare.
	 * \return The reader that should be used for playback.
	 */
	AUD_IReader* prepare(AUD_IReader* reader);

	/**
	 * Returns the target specification for superposing.
	 * \return The target specification.
	 */
	AUD_DeviceSpecs getSpecs();

	/**
	 * Sets the target specification for superposing.
	 * \param specs The target specification.
	 */
	void setSpecs(AUD_DeviceSpecs specs);

	/**
	 * Adds a buffer for superposition.
	 * \param buffer The buffer to superpose.
	 * \param start The start sample of the buffer.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void add(sample_t* buffer, int start, int length, float volume);

	/**
	 * Superposes all added buffers into an output buffer.
	 * \param buffer The target buffer for superposing.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void superpose(data_t* buffer, int length, float volume);
};

#endif //AUD_MIXER
