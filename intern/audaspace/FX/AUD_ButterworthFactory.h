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

#ifndef AUD_BUTTERWORTHFACTORY
#define AUD_BUTTERWORTHFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory creates a butterworth filter reader.
 */
class AUD_ButterworthFactory : public AUD_EffectFactory
{
private:
	/**
	 * The attack value in seconds.
	 */
	float m_frequency;

public:
	/**
	 * Creates a new butterworth factory.
	 * \param factory The input factory.
	 * \param frequency The cutoff frequency.
	 */
	AUD_ButterworthFactory(AUD_IFactory* factory, float frequency);

	/**
	 * Creates a new butterworth factory.
	 * \param frequency The cutoff frequency.
	 */
	AUD_ButterworthFactory(float frequency);

	virtual AUD_IReader* createReader();
};

#endif //AUD_BUTTERWORTHFACTORY
