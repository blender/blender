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

/** \file audaspace/FX/AUD_RectifyFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_RECTIFYFACTORY_H__
#define __AUD_RECTIFYFACTORY_H__

#include "AUD_EffectFactory.h"
class AUD_CallbackIIRFilterReader;

/**
 * This factory rectifies another factory.
 */
class AUD_RectifyFactory : public AUD_EffectFactory
{
private:
	// hide copy constructor and operator=
	AUD_RectifyFactory(const AUD_RectifyFactory&);
	AUD_RectifyFactory& operator=(const AUD_RectifyFactory&);

public:
	/**
	 * Creates a new rectify factory.
	 * \param factory The input factory.
	 */
	AUD_RectifyFactory(boost::shared_ptr<AUD_IFactory> factory);

	virtual boost::shared_ptr<AUD_IReader> createReader();

	static sample_t rectifyFilter(AUD_CallbackIIRFilterReader* reader, void* useless);
};

#endif //__AUD_RECTIFYFACTORY_H__
