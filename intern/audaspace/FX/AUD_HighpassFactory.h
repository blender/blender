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

/** \file audaspace/FX/AUD_HighpassFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_HIGHPASSFACTORY_H__
#define __AUD_HIGHPASSFACTORY_H__

#include "AUD_DynamicIIRFilterFactory.h"

/**
 * This factory creates a highpass filter reader.
 */
class AUD_HighpassFactory : public AUD_DynamicIIRFilterFactory
{
private:
	// hide copy constructor and operator=
	AUD_HighpassFactory(const AUD_HighpassFactory&);
	AUD_HighpassFactory& operator=(const AUD_HighpassFactory&);

public:
	/**
	 * Creates a new highpass factory.
	 * \param factory The input factory.
	 * \param frequency The cutoff frequency.
	 * \param Q The Q factor.
	 */
	AUD_HighpassFactory(boost::shared_ptr<AUD_IFactory> factory, float frequency, float Q = 1.0f);
};

#endif //__AUD_HIGHPASSFACTORY_H__
