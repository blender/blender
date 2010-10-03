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

#include "AUD_DefaultMixer.h"
#include "AUD_SRCResampleReader.h"
#include "AUD_ChannelMapperReader.h"
#include "AUD_ChannelMapperFactory.h"

#include <cstring>

AUD_DefaultMixer::AUD_DefaultMixer(AUD_DeviceSpecs specs) :
	AUD_Mixer(specs)
{
}

AUD_IReader* AUD_DefaultMixer::prepare(AUD_IReader* reader)
{
	// hacky for now, until a better channel mapper reader is available
	AUD_ChannelMapperFactory cmf(NULL, m_specs);

	AUD_Specs specs = reader->getSpecs();

	// if channel count is lower in output, rechannel before resampling
	if(specs.channels < m_specs.channels)
	{
		reader = new AUD_ChannelMapperReader(reader,
											 cmf.getMapping(specs.channels));
		specs.channels = m_specs.channels;
	}

	// resample
	if(specs.rate != m_specs.rate)
		reader = new AUD_SRCResampleReader(reader, m_specs.specs);

	// rechannel
	if(specs.channels != m_specs.channels)
		reader = new AUD_ChannelMapperReader(reader,
											 cmf.getMapping(specs.channels));

	return reader;
}
