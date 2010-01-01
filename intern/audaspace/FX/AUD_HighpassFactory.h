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

#ifndef AUD_HIGHPASSFACTORY
#define AUD_HIGHPASSFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory creates a highpass filter reader.
 */
class AUD_HighpassFactory : public AUD_EffectFactory
{
private:
	/**
	 * The attack value in seconds.
	 */
	float m_frequency;

	/**
	 * The Q factor.
	 */
	float m_Q;

public:
	/**
	 * Creates a new highpass factory.
	 * \param factory The input factory.
	 * \param frequency The cutoff frequency.
	 * \param Q The Q factor.
	 */
	AUD_HighpassFactory(AUD_IFactory* factory, float frequency, float Q = 1.0);

	/**
	 * Creates a new highpass factory.
	 * \param frequency The cutoff frequency.
	 * \param Q The Q factor.
	 */
	AUD_HighpassFactory(float frequency, float Q = 1.0);

	virtual AUD_IReader* createReader();
};

#endif //AUD_HIGHPASSFACTORY
