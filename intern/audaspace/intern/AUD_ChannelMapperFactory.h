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

/** \file audaspace/intern/AUD_ChannelMapperFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_CHANNELMAPPERFACTORY_H__
#define __AUD_CHANNELMAPPERFACTORY_H__

#include "AUD_MixerFactory.h"

/**
 * This factory creates a reader that maps a sound source's channels to a
 * specific output channel count.
 */
class AUD_ChannelMapperFactory : public AUD_MixerFactory
{
private:
	// hide copy constructor and operator=
	AUD_ChannelMapperFactory(const AUD_ChannelMapperFactory&);
	AUD_ChannelMapperFactory& operator=(const AUD_ChannelMapperFactory&);

public:
	/**
	 * Creates a new factory.
	 * \param factory The input factory.
	 * \param specs The target specifications.
	 */
	AUD_ChannelMapperFactory(boost::shared_ptr<AUD_IFactory> factory, AUD_DeviceSpecs specs);

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif //__AUD_CHANNELMAPPERFACTORY_H__
