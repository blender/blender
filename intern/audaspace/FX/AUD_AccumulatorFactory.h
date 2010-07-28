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
