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

#include "AUD_ButterworthFactory.h"
#include "AUD_IIRFilterReader.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BWPB41 0.76536686473
#define BWPB42 1.84775906502

AUD_ButterworthFactory::AUD_ButterworthFactory(AUD_IFactory* factory,
											   float frequency) :
		AUD_EffectFactory(factory),
		m_frequency(frequency)
{
}

AUD_IReader* AUD_ButterworthFactory::createReader() const
{
	AUD_IReader* reader = getReader();

	// calculate coefficients
	float omega = 2 * tan(m_frequency * M_PI / reader->getSpecs().rate);
	float o2 = omega * omega;
	float o4 = o2 * o2;
	float x1 = o2 + 2 * BWPB41 * omega + 4;
	float x2 = o2 + 2 * BWPB42 * omega + 4;
	float y1 = o2 - 2 * BWPB41 * omega + 4;
	float y2 = o2 - 2 * BWPB42 * omega + 4;
	float o228 = 2 * o2 - 8;
	float norm = x1 * x2;
	std::vector<float> a, b;
	a.push_back(1);
	a.push_back((x1 + x2) * o228 / norm);
	a.push_back((x1 * y2 + x2 * y1 + o228 * o228) / norm);
	a.push_back((y1 + y2) * o228 / norm);
	a.push_back(y1 * y2 / norm);
	b.push_back(o4 / norm);
	b.push_back(4 * o4 / norm);
	b.push_back(6 * o4 / norm);
	b.push_back(b[1]);
	b.push_back(b[0]);

	return new AUD_IIRFilterReader(reader, b, a);
}
