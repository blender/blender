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

/** \file audaspace/FX/AUD_LowpassFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_LowpassFactory.h"
#include "AUD_IIRFilterReader.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AUD_LowpassFactory::AUD_LowpassFactory(boost::shared_ptr<AUD_IFactory> factory, float frequency,
									   float Q) :
		AUD_DynamicIIRFilterFactory(factory),
		m_frequency(frequency),
		m_Q(Q)
{
}

void AUD_LowpassFactory::recalculateCoefficients(AUD_SampleRate rate,
												 std::vector<float> &b,
												 std::vector<float> &a)
{
	float w0 = 2 * M_PI * m_frequency / rate;
	float alpha = sin(w0) / (2 * m_Q);
	float norm = 1 + alpha;
	float c = cos(w0);
	a.push_back(1);
	a.push_back(-2 * c / norm);
	a.push_back((1 - alpha) / norm);
	b.push_back((1 - c) / (2 * norm));
	b.push_back((1 - c) / norm);
	b.push_back(b[0]);
}
