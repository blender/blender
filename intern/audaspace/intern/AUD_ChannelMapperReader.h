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

/** \file audaspace/intern/AUD_ChannelMapperReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_CHANNELMAPPERREADER_H__
#define __AUD_CHANNELMAPPERREADER_H__

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
	 * The sound reading buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The output specification.
	 */
	AUD_Channels m_target_channels;

	/**
	 * The channel count of the reader.
	 */
	AUD_Channels m_source_channels;

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

	static const AUD_Channel MONO_MAP[];
	static const AUD_Channel STEREO_MAP[];
	static const AUD_Channel STEREO_LFE_MAP[];
	static const AUD_Channel SURROUND4_MAP[];
	static const AUD_Channel SURROUND5_MAP[];
	static const AUD_Channel SURROUND51_MAP[];
	static const AUD_Channel SURROUND61_MAP[];
	static const AUD_Channel SURROUND71_MAP[];
	static const AUD_Channel* CHANNEL_MAPS[];

	static const float MONO_ANGLES[];
	static const float STEREO_ANGLES[];
	static const float STEREO_LFE_ANGLES[];
	static const float SURROUND4_ANGLES[];
	static const float SURROUND5_ANGLES[];
	static const float SURROUND51_ANGLES[];
	static const float SURROUND61_ANGLES[];
	static const float SURROUND71_ANGLES[];
	static const float* CHANNEL_ANGLES[];

	// hide copy constructor and operator=
	AUD_ChannelMapperReader(const AUD_ChannelMapperReader&);
	AUD_ChannelMapperReader& operator=(const AUD_ChannelMapperReader&);

	/**
	 * Calculates the mapping matrix.
	 */
	void calculateMapping();

	/**
	 * Calculates the distance between two angles.
	 */
	float angleDistance(float alpha, float beta);

public:
	/**
	 * Creates a channel mapper reader.
	 * \param reader The reader to map.
	 * \param mapping The mapping specification as two dimensional float array.
	 */
	AUD_ChannelMapperReader(AUD_Reference<AUD_IReader> reader, AUD_Channels channels);

	/**
	 * Destroys the reader.
	 */
	~AUD_ChannelMapperReader();

	/**
	 * Sets the requested channel output count.
	 * \param channels The channel output count.
	 */
	void setChannels(AUD_Channels channels);

	/**
	 * Sets the angle for mono sources.
	 * \param angle The angle for mono sources.
	 */
	void setMonoAngle(float angle);

	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_CHANNELMAPPERREADER_H__
