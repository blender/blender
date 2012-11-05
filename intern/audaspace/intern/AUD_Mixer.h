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

/** \file audaspace/intern/AUD_Mixer.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_MIXER_H__
#define __AUD_MIXER_H__

#include "AUD_ConverterFunctions.h"
#include "AUD_Buffer.h"
class AUD_IReader;

#include <boost/shared_ptr.hpp>

/**
 * This abstract class is able to mix audiosignals with same channel count
 * and sample rate and convert it to a specific output format.
 */
class AUD_Mixer
{
protected:
	/**
	 * The output specification.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The length of the mixing buffer.
	 */
	int m_length;

	/**
	 * The mixing buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * Converter function.
	 */
	AUD_convert_f m_convert;

public:
	/**
	 * Creates the mixer.
	 */
	AUD_Mixer(AUD_DeviceSpecs specs);

	/**
	 * Destroys the mixer.
	 */
	virtual ~AUD_Mixer() {}

	/**
	 * Returns the target specification for superposing.
	 * \return The target specification.
	 */
	AUD_DeviceSpecs getSpecs() const;

	/**
	 * Sets the target specification for superposing.
	 * \param specs The target specification.
	 */
	void setSpecs(AUD_Specs specs);

	/**
	 * Mixes a buffer.
	 * \param buffer The buffer to superpose.
	 * \param start The start sample of the buffer.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void mix(sample_t* buffer, int start, int length, float volume);

	/**
	 * Writes the mixing buffer into an output buffer.
	 * \param buffer The target buffer for superposing.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void read(data_t* buffer, float volume);

	/**
	 * Clears the mixing buffer.
	 * \param length The length of the buffer in samples.
	 */
	void clear(int length);
};

#endif //__AUD_MIXER_H__
