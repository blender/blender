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

/** \file audaspace/SRC/AUD_SRCResampleFactory.h
 *  \ingroup audsrc
 */


#ifndef __AUD_SRCRESAMPLEFACTORY_H__
#define __AUD_SRCRESAMPLEFACTORY_H__

#include "AUD_MixerFactory.h"

/**
 * This factory creates a resampling reader that uses libsamplerate for
 * resampling.
 */
class AUD_SRCResampleFactory : public AUD_MixerFactory
{
private:
	// hide copy constructor and operator=
	AUD_SRCResampleFactory(const AUD_SRCResampleFactory&);
	AUD_SRCResampleFactory& operator=(const AUD_SRCResampleFactory&);

public:
	/**
	 * Creates a new factory.
	 * \param factory The input factory.
	 * \param specs The target specifications.
	 */
	AUD_SRCResampleFactory(AUD_Reference<AUD_IFactory> factory, AUD_DeviceSpecs specs);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_SRCRESAMPLEFACTORY_H__
