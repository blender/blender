/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/FX/AUD_PitchFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_PITCHFACTORY_H__
#define __AUD_PITCHFACTORY_H__

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
	AUD_PitchFactory(boost::shared_ptr<AUD_IFactory> factory, float pitch);

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif //__AUD_PITCHFACTORY_H__
