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

#ifndef AUD_IIRFILTERFACTORY
#define AUD_IIRFILTERFACTORY

#include "AUD_EffectFactory.h"

#include <vector>

/**
 * This factory creates a IIR filter reader.
 */
class AUD_IIRFilterFactory : public AUD_EffectFactory
{
private:
	/**
	 * Output filter coefficients.
	 */
	std::vector<float> m_a;

	/**
	 * Input filter coefficients.
	 */
	std::vector<float> m_b;

	// hide copy constructor and operator=
	AUD_IIRFilterFactory(const AUD_IIRFilterFactory&);
	AUD_IIRFilterFactory& operator=(const AUD_IIRFilterFactory&);

public:
	/**
	 * Creates a new IIR filter factory.
	 * \param factory The input factory.
	 * \param b The input filter coefficients.
	 * \param a The output filter coefficients.
	 */
	AUD_IIRFilterFactory(AUD_IFactory* factory, std::vector<float> b,
						 std::vector<float> a);

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_IIRFILTERFACTORY
