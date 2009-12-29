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

#ifndef AUD_BANDPASSFACTORY
#define AUD_BANDPASSFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory creates a band pass filter for a sound wave.
 */
class AUD_BandPassFactory : public AUD_EffectFactory
{
private:
	/**
	 * The lowest frequency to be passed.
	 */
	float m_low;

	/**
	 * The highest frequency to be passed.
	 */
	float m_high;

public:
	/**
	 * Creates a new band pass factory.
	 * \param factory The input factory.
	 * \param low The lowest passed frequency.
	 * \param high The highest passed frequency.
	 */
	AUD_BandPassFactory(AUD_IFactory* factory, float low, float high);

	/**
	 * Creates a new band pass factory.
	 * \param low The lowest passed frequency.
	 * \param high The highest passed frequency.
	 */
	AUD_BandPassFactory(float low, float high);

	/**
	 * Returns the lowest passed frequency.
	 */
	float getLow();

	/**
	 * Returns the highest passed frequency.
	 */
	float getHigh();

	/**
	 * Sets the lowest passed frequency.
	 * \param low The lowest passed frequency.
	 */
	void setLow(float low);

	/**
	 * Sets the highest passed frequency.
	 * \param high The highest passed frequency.
	 */
	void setHigh(float hight);

	virtual AUD_IReader* createReader();
};

#endif //AUD_BANDPASSFACTORY
