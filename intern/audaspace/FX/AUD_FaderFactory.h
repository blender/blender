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

#ifndef AUD_FADERFACTORY
#define AUD_FADERFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory fades another factory.
 * If the fading type is AUD_FADE_IN, everything before the fading start will be
 * silenced, for AUD_FADE_OUT that's true for everything after fading ends.
 */
class AUD_FaderFactory : public AUD_EffectFactory
{
private:
	/**
	 * The fading type.
	 */
	AUD_FadeType m_type;

	/**
	 * The fading start.
	 */
	float m_start;

	/**
	 * The fading length.
	 */
	float m_length;

public:
	/**
	 * Creates a new fader factory.
	 * \param factory The input factory.
	 * \param type The fading type.
	 * \param start The time where fading should start in seconds.
	 * \param length How long fading should last in seconds.
	 */
	AUD_FaderFactory(AUD_IFactory* factory = 0,
					  AUD_FadeType type = AUD_FADE_IN,
					  float start = 0.0f, float length = 1.0f);

	/**
	 * Creates a new fader factory.
	 * \param type The fading type.
	 * \param start The time where fading should start in seconds.
	 * \param length How long fading should last in seconds.
	 */
	AUD_FaderFactory(AUD_FadeType type = AUD_FADE_IN,
					  float start = 0.0f, float length = 1.0f);

	/**
	 * Returns the fading type.
	 */
	AUD_FadeType getType();

	/**
	 * Sets the fading type.
	 * \param type The new fading type: AUD_FADE_IN or AUD_FADE_OUT.
	 */
	void setType(AUD_FadeType type);

	/**
	 * Returns the fading start.
	 */
	float getStart();

	/**
	 * Sets the fading start.
	 * \param start The new fading start.
	 */
	void setStart(float start);

	/**
	 * Returns the fading length.
	 */
	float getLength();

	/**
	 * Sets the fading length.
	 * \param start The new fading length.
	 */
	void setLength(float length);

	virtual AUD_IReader* createReader();
};

#endif //AUD_FADERFACTORY
