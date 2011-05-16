/*
 * $Id$
 *
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

/** \file audaspace/FX/AUD_AccumulatorFactory.h
 *  \ingroup audfx
 */


#ifndef AUD_ACCUMULATORFACTORY
#define AUD_ACCUMULATORFACTORY

#include "AUD_EffectFactory.h"

/**
 * This factory creates an accumulator reader.
 */
class AUD_AccumulatorFactory : public AUD_EffectFactory
{
private:
	/**
	 * Whether the accumulator is additive.
	 */
	const bool m_additive;

	// hide copy constructor and operator=
	AUD_AccumulatorFactory(const AUD_AccumulatorFactory&);
	AUD_AccumulatorFactory& operator=(const AUD_AccumulatorFactory&);

public:
	/**
	 * Creates a new accumulator factory.
	 * \param factory The input factory.
	 * \param additive Whether the accumulator is additive.
	 */
	AUD_AccumulatorFactory(AUD_IFactory* factory, bool additive = false);

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_ACCUMULATORFACTORY
