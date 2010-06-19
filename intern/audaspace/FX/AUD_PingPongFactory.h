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

#ifndef AUD_PINGPONGFACTORY
#define AUD_PINGPONGFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory plays another factory first normal, then reversed.
 * \note Readers from the underlying factory must be from the buffer type.
 */
class AUD_PingPongFactory : public AUD_EffectFactory
{
public:
	/**
	 * Creates a new ping pong factory.
	 * \param factory The input factory.
	 */
	AUD_PingPongFactory(AUD_IFactory* factory = 0);

	/**
	 * Destroys the factory.
	 */

	virtual AUD_IReader* createReader();
};

#endif //AUD_PINGPONGFACTORY
