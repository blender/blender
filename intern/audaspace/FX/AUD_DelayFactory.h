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

#ifndef AUD_DELAYFACTORY
#define AUD_DELAYFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory plays another factory delayed.
 */
class AUD_DelayFactory : public AUD_EffectFactory
{
private:
	/**
	 * The delay in samples.
	 */
	const float m_delay;

	// hide copy constructor and operator=
	AUD_DelayFactory(const AUD_DelayFactory&);
	AUD_DelayFactory& operator=(const AUD_DelayFactory&);

public:
	/**
	 * Creates a new delay factory.
	 * \param factory The input factory.
	 * \param delay The desired delay in seconds.
	 */
	AUD_DelayFactory(AUD_IFactory* factory, float delay = 0);

	/**
	 * Returns the delay in seconds.
	 */
	float getDelay() const;

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_DELAYFACTORY
