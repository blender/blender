/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

/**
 * @file ChannelMapperReader.h
 * @ingroup respec
 * The ChannelMapperReader class.
 */

#include "fx/EffectReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This class maps a sound source's channels to a specific output channel count.
 * \note The input sample format must be float.
 */
class AUD_API ChannelMapperReader : public EffectReader
{
private:
	/**
	 * The sound reading buffer.
	 */
	Buffer m_buffer;

	/**
	 * The output specification.
	 */
	Channels m_target_channels;

	/**
	 * The channel count of the reader.
	 */
	Channels m_source_channels;

	/**
	 * The mapping specification.
	 */
	float* m_mapping;

	/**
	 * The size of the mapping.
	 */
	int m_map_size;

	/**
	 * The mono source angle.
	 */
	float m_mono_angle;

	static const Channel MONO_MAP[];
	static const Channel STEREO_MAP[];
	static const Channel STEREO_LFE_MAP[];
	static const Channel SURROUND4_MAP[];
	static const Channel SURROUND5_MAP[];
	static const Channel SURROUND51_MAP[];
	static const Channel SURROUND61_MAP[];
	static const Channel SURROUND71_MAP[];
	static const Channel* CHANNEL_MAPS[];

	static const float MONO_ANGLES[];
	static const float STEREO_ANGLES[];
	static const float STEREO_LFE_ANGLES[];
	static const float SURROUND4_ANGLES[];
	static const float SURROUND5_ANGLES[];
	static const float SURROUND51_ANGLES[];
	static const float SURROUND61_ANGLES[];
	static const float SURROUND71_ANGLES[];
	static const float* CHANNEL_ANGLES[];

	// delete copy constructor and operator=
	ChannelMapperReader(const ChannelMapperReader&) = delete;
	ChannelMapperReader& operator=(const ChannelMapperReader&) = delete;

	/**
	 * Calculates the mapping matrix.
	 */
	void AUD_LOCAL calculateMapping();

	/**
	 * Calculates the distance between two angles.
	 */
	float AUD_LOCAL angleDistance(float alpha, float beta);

public:
	/**
	 * Creates a channel mapper reader.
	 * \param reader The reader to map.
	 * \param channels The target channel count this reader should map to.
	 */
	ChannelMapperReader(std::shared_ptr<IReader> reader, Channels channels);

	/**
	 * Destroys the reader.
	 */
	~ChannelMapperReader();

	/**
	 * Returns the channel configuration of the source reader.
	 * @return The channel configuration of the reader.
	 */
	Channels getSourceChannels() const;

	/**
	 * Returns the target channel configuration.
	 * Equals getSpecs().channels.
	 * @return The target channel configuration.
	 */
	Channels getChannels() const;

	/**
	 * Sets the requested channel output count.
	 * \param channels The channel output count.
	 */
	void setChannels(Channels channels);

	/**
	 * Returns the mapping of the source channel to the target channel.
	 * @param source The number of the source channel. Should be in the range [0, source channels).
	 * @param target The number of the target channel. Should be in the range [0, target channels).
	 * @return The mapping value which should be between 0.0 and 1.0. If source or target are out of range, NaN is returned.
	 */
	float getMapping(int source, int target);

	/**
	 * Sets the angle for mono sources.
	 * \param angle The angle for mono sources.
	 */
	void setMonoAngle(float angle);

	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
