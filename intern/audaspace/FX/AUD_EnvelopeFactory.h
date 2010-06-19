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

#ifndef AUD_ENVELOPEFACTORY
#define AUD_ENVELOPEFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory creates an envelope follower reader.
 */
class AUD_EnvelopeFactory : public AUD_EffectFactory
{
private:
	/**
	 * The attack value in seconds.
	 */
	float m_attack;

	/**
	 * The release value in seconds.
	 */
	float m_release;

	/**
	 * The threshold value.
	 */
	float m_threshold;

	/**
	 * The attack/release threshold value.
	 */
	float m_arthreshold;

public:
	/**
	 * Creates a new envelope factory.
	 * \param factory The input factory.
	 * \param attack The attack value in seconds.
	 * \param release The release value in seconds.
	 * \param threshold The threshold value.
	 * \param arthreshold The attack/release threshold value.
	 */
	AUD_EnvelopeFactory(AUD_IFactory* factory, float attack, float release,
						float threshold, float arthreshold);

	/**
	 * Creates a new envelope factory.
	 * \param attack The attack value in seconds.
	 * \param release The release value in seconds.
	 * \param threshold The threshold value.
	 * \param arthreshold The attack/release threshold value.
	 */
	AUD_EnvelopeFactory(float attack, float release, float threshold,
						float arthreshold);

	virtual AUD_IReader* createReader();
};

#endif //AUD_ENVELOPEFACTORY
