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

/** \file audaspace/FX/AUD_SquareFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_SQUAREFACTORY_H__
#define __AUD_SQUAREFACTORY_H__

#include "AUD_EffectFactory.h"
class AUD_CallbackIIRFilterReader;

/**
 * This factory Transforms any signal to a square signal.
 */
class AUD_SquareFactory : public AUD_EffectFactory
{
private:
	/**
	 * The threshold.
	 */
	const float m_threshold;

	// hide copy constructor and operator=
	AUD_SquareFactory(const AUD_SquareFactory&);
	AUD_SquareFactory& operator=(const AUD_SquareFactory&);

public:
	/**
	 * Creates a new square factory.
	 * \param factory The input factory.
	 * \param threshold The threshold.
	 */
	AUD_SquareFactory(AUD_Reference<AUD_IFactory> factory, float threshold = 0.0f);

	/**
	 * Returns the threshold.
	 */
	float getThreshold() const;

	virtual AUD_Reference<AUD_IReader> createReader();

	static sample_t squareFilter(AUD_CallbackIIRFilterReader* reader, float* threshold);
	static void endSquareFilter(float* threshold);
};

#endif //__AUD_SQUAREFACTORY_H__
