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

#ifndef AUD_IMIXER
#define AUD_IMIXER

#include "AUD_Space.h"
class AUD_IReader;

/**
 * This class is able to mix audiosignals of different format and channel count.
 * \note This class doesn't do resampling!
 */
class AUD_IMixer
{
public:
	/**
	 * Destroys the mixer.
	 */
	virtual ~AUD_IMixer(){}

	/**
	 * This funuction prepares a reader for playback.
	 * \param reader The reader to prepare.
	 * \return The reader that should be used for playback.
	 */
	virtual AUD_IReader* prepare(AUD_IReader* reader)=0;

	/**
	 * Sets the target specification for superposing.
	 * \param specs The target specification.
	 */
	virtual void setSpecs(AUD_Specs specs)=0;

	/**
	 * Adds a buffer for superposition.
	 * \param buffer The buffer to superpose.
	 * \param specs The specification of the buffer.
	 * \param start The start sample of the buffer.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	virtual void add(sample_t* buffer, AUD_Specs specs, int length,
					 float volume)=0;

	/**
	 * Superposes all added buffers into an output buffer.
	 * \param buffer The target buffer for superposing.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	virtual void superpose(sample_t* buffer, int length, float volume)=0;
};

#endif //AUD_IMIXER
