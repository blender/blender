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

#ifndef AUD_LOOPFACTORY
#define AUD_LOOPFACTORY

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
	AUD_LoopFactory(AUD_IFactory* factory, int loop = -1);

	/**
	 * Returns the loop count.
	 */
	int getLoop() const;

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_LOOPFACTORY
