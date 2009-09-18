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
	float m_delay;

public:
	/**
	 * Creates a new delay factory.
	 * \param factory The input factory.
	 * \param delay The desired delay in seconds.
	 */
	AUD_DelayFactory(AUD_IFactory* factory = 0, float delay = 0);

	/**
	 * Creates a new delay factory.
	 * \param delay The desired delay in seconds.
	 */
	AUD_DelayFactory(float delay);

	/**
	 * Returns the delay in seconds.
	 */
	float getDelay();

	/**
	 * Sets the delay.
	 * \param delay The new delay value in seconds.
	 */
	void setDelay(float delay);

	virtual AUD_IReader* createReader();
};

#endif //AUD_DELAYFACTORY
