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

#include "AUD_HighpassFactory.h"
#include "AUD_IIRFilterReader.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AUD_HighpassFactory::AUD_HighpassFactory(AUD_IFactory* factory, float frequency,
										 float Q) :
		AUD_EffectFactory(factory),
		m_frequency(frequency),
		m_Q(Q)
{
}

AUD_IReader* AUD_HighpassFactory::createReader() const
{
	AUD_IReader* reader = getReader();

	// calculate coefficients
	float w0 = 2 * M_PI * m_frequency / reader->getSpecs().rate;
	float alpha = sin(w0) / (2 * m_Q);
	float norm = 1 + alpha;
	float c = cos(w0);
	std::vector<float> a, b;
	a.push_back(1);
	a.push_back(-2 * c / norm);
	a.push_back((1 - alpha) / norm);
	b.push_back((1 + c) / (2 * norm));
	b.push_back((-1 - c) / norm);
	b.push_back(b[0]);

	return new AUD_IIRFilterReader(reader, b, a);
}
