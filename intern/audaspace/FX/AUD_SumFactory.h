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

/** \file audaspace/FX/AUD_SumFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_SUMFACTORY_H__
#define __AUD_SUMFACTORY_H__

#include "AUD_EffectFactory.h"

/**
 * This factory creates a sum reader.
 */
class AUD_SumFactory : public AUD_EffectFactory
{
private:
	// hide copy constructor and operator=
	AUD_SumFactory(const AUD_SumFactory&);
	AUD_SumFactory& operator=(const AUD_SumFactory&);

public:
	/**
	 * Creates a new sum factory.
	 * \param factory The input factory.
	 */
	AUD_SumFactory(AUD_Reference<AUD_IFactory> factory);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_SUMFACTORY_H__
