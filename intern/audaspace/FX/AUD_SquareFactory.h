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

#ifndef AUD_SQUAREFACTORY
#define AUD_SQUAREFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory Transforms any signal to a square signal.
 */
class AUD_SquareFactory : public AUD_EffectFactory
{
private:
	/**
	 * The threshold.
	 */
	float m_threshold;

public:
	/**
	 * Creates a new square factory.
	 * \param factory The input factory.
	 * \param threshold The threshold.
	 */
	AUD_SquareFactory(AUD_IFactory* factory = 0, float threshold = 0.0f);

	/**
	 * Creates a new square factory.
	 * \param threshold The threshold.
	 */
	AUD_SquareFactory(float threshold);

	/**
	 * Returns the threshold.
	 */
	float getThreshold();

	/**
	 * Sets the threshold.
	 * \param threshold The new threshold value. Should be between 0.0 and 1.0.
	 */
	void setThreshold(float threshold);

	virtual AUD_IReader* createReader();
};

#endif //AUD_SQUAREFACTORY
