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

/** \file audaspace/FX/AUD_ButterworthFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_BUTTERWORTHFACTORY_H__
#define __AUD_BUTTERWORTHFACTORY_H__

#include "AUD_DynamicIIRFilterFactory.h"

/**
 * This factory creates a butterworth lowpass filter reader.
 */
class AUD_ButterworthFactory : public AUD_DynamicIIRFilterFactory
{
private:
	// hide copy constructor and operator=
	AUD_ButterworthFactory(const AUD_ButterworthFactory&);
	AUD_ButterworthFactory& operator=(const AUD_ButterworthFactory&);

public:
	/**
	 * Creates a new butterworth factory.
	 * \param factory The input factory.
	 * \param frequency The cutoff frequency.
	 */
	AUD_ButterworthFactory(boost::shared_ptr<AUD_IFactory> factory, float frequency);
};

#endif //__AUD_BUTTERWORTHFACTORY_H__
