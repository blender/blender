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

#ifndef AUD_CHANNELMAPPERFACTORY
#define AUD_CHANNELMAPPERFACTORY

#include "AUD_MixerFactory.h"

/**
 * This factory creates a reader that maps a sound source's channels to a
 * specific output channel count.
 */
class AUD_ChannelMapperFactory : public AUD_MixerFactory
{
private:
	/**
	 * The mapping specification.
	 */
	float **m_mapping[9];

public:
	AUD_ChannelMapperFactory(AUD_IReader* reader, AUD_DeviceSpecs specs);
	AUD_ChannelMapperFactory(AUD_IFactory* factory, AUD_DeviceSpecs specs);
	AUD_ChannelMapperFactory(AUD_DeviceSpecs specs);

	virtual ~AUD_ChannelMapperFactory();

	/**
	 * Returns the mapping array for editing.
	 * \param ic The count of input channels the array should have.
	 * \note The count of output channels is read of the desired output specs.
	 */
	float** getMapping(int ic);

	/**
	 * Deletes the current channel mapping.
	 */
	void deleteMapping(int ic);

	virtual AUD_IReader* createReader();
};

#endif //AUD_CHANNELMAPPERFACTORY
