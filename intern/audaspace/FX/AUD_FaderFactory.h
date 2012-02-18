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

/** \file audaspace/FX/AUD_FaderFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_FADERFACTORY_H__
#define __AUD_FADERFACTORY_H__

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
	const AUD_FadeType m_type;

	/**
	 * The fading start.
	 */
	const float m_start;

	/**
	 * The fading length.
	 */
	const float m_length;

	// hide copy constructor and operator=
	AUD_FaderFactory(const AUD_FaderFactory&);
	AUD_FaderFactory& operator=(const AUD_FaderFactory&);

public:
	/**
	 * Creates a new fader factory.
	 * \param factory The input factory.
	 * \param type The fading type.
	 * \param start The time where fading should start in seconds.
	 * \param length How long fading should last in seconds.
	 */
	AUD_FaderFactory(AUD_Reference<AUD_IFactory> factory,
					  AUD_FadeType type = AUD_FADE_IN,
					  float start = 0.0f, float length = 1.0f);

	/**
	 * Returns the fading type.
	 */
	AUD_FadeType getType() const;

	/**
	 * Returns the fading start.
	 */
	float getStart() const;

	/**
	 * Returns the fading length.
	 */
	float getLength() const;

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_FADERFACTORY_H__
