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

#ifndef AUD_DEFAULTMIXER
#define AUD_DEFAULTMIXER

#include "AUD_Mixer.h"

/**
 * This class is able to mix audiosignals of different channel count and sample
 * rate and convert it to a specific output format.
 * It uses a default ChannelMapperFactory and a SRCResampleFactory for
 * the perparation.
 */
class AUD_DefaultMixer : public AUD_Mixer
{
public:
	/**
	 * Creates the mixer.
	 */
	AUD_DefaultMixer(AUD_DeviceSpecs specs);

	/**
	 * This funuction prepares a reader for playback.
	 * \param reader The reader to prepare.
	 * \return The reader that should be used for playback.
	 */
	virtual AUD_IReader* prepare(AUD_IReader* reader);
};

#endif //AUD_DEFAULTMIXER
