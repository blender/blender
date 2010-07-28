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

#ifndef AUD_PITCHFACTORY
#define AUD_PITCHFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory changes the pitch of another factory.
 */
class AUD_PitchFactory : public AUD_EffectFactory
{
private:
	/**
	 * The pitch.
	 */
	const float m_pitch;

	// hide copy constructor and operator=
	AUD_PitchFactory(const AUD_PitchFactory&);
	AUD_PitchFactory& operator=(const AUD_PitchFactory&);

public:
	/**
	 * Creates a new pitch factory.
	 * \param factory The input factory.
	 * \param pitch The desired pitch.
	 */
	AUD_PitchFactory(AUD_IFactory* factory, float pitch);

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_PITCHFACTORY
