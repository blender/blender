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

/** \file audaspace/FX/AUD_LoopFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_LOOPFACTORY_H__
#define __AUD_LOOPFACTORY_H__

#include "AUD_EffectFactory.h"

/**
 * This factory loops another factory.
 * \note The reader has to be seekable.
 */
class AUD_LoopFactory : public AUD_EffectFactory
{
private:
	/**
	 * The loop count.
	 */
	const int m_loop;

	// hide copy constructor and operator=
	AUD_LoopFactory(const AUD_LoopFactory&);
	AUD_LoopFactory& operator=(const AUD_LoopFactory&);

public:
	/**
	 * Creates a new loop factory.
	 * \param factory The input factory.
	 * \param loop The desired loop count, negative values result in endless
	 *        looping.
	 */
	AUD_LoopFactory(AUD_Reference<AUD_IFactory> factory, int loop = -1);

	/**
	 * Returns the loop count.
	 */
	int getLoop() const;

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_LOOPFACTORY_H__
