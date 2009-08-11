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

#ifndef AUD_LIMITERFACTORY
#define AUD_LIMITERFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory limits another factory in start and end time.
 */
class AUD_LimiterFactory : public AUD_EffectFactory
{
private:
	/**
	 * The start time.
	 */
	float m_start;

	/**
	 * The end time.
	 */
	float m_end;

public:
	/**
	 * Creates a new limiter factory.
	 * \param factory The input factory.
	 * \param start The desired start time.
	 * \param end The desired end time, a negative value signals that it should
	 *            play to the end.
	 */
	AUD_LimiterFactory(AUD_IFactory* factory = 0,
					   float start = 0, float end = -1);

	/**
	 * Returns the start time.
	 */
	float getStart();

	/**
	 * Sets the start time.
	 * \param start The new start time.
	 */
	void setStart(float start);

	/**
	 * Returns the end time.
	 */
	float getEnd();

	/**
	 * Sets the end time.
	 * \param end The new end time, a negative value signals that it should play
	 *            to the end.
	 */
	void setEnd(float end);

	virtual AUD_IReader* createReader();
};

#endif //AUD_LIMITERFACTORY
