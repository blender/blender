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

/** \file audaspace/intern/AUD_ConverterFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_CONVERTERFACTORY_H__
#define __AUD_CONVERTERFACTORY_H__

#include "AUD_MixerFactory.h"

/**
 * This factory creates a converter reader that is able to convert from one
 * audio format to another.
 */
class AUD_ConverterFactory : public AUD_MixerFactory
{
private:
	// hide copy constructor and operator=
	AUD_ConverterFactory(const AUD_ConverterFactory&);
	AUD_ConverterFactory& operator=(const AUD_ConverterFactory&);

public:
	/**
	 * Creates a new factory.
	 * \param factory The input factory.
	 * \param specs The target specifications.
	 */
	AUD_ConverterFactory(AUD_Reference<AUD_IFactory> factory, AUD_DeviceSpecs specs);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_CONVERTERFACTORY_H__
