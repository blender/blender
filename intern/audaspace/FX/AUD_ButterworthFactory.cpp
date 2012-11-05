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

/** \file audaspace/FX/AUD_ButterworthFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_ButterworthFactory.h"
#include "AUD_IIRFilterReader.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BWPB41 0.76536686473
#define BWPB42 1.84775906502

AUD_ButterworthFactory::AUD_ButterworthFactory(AUD_Reference<AUD_IFactory> factory,
											   float frequency) :
		AUD_DynamicIIRFilterFactory(factory),
		m_frequency(frequency)
{
}

void AUD_ButterworthFactory::recalculateCoefficients(AUD_SampleRate rate,
													 std::vector<float> &b,
													 std::vector<float> &a)
{
	float omega = 2 * tan(m_frequency * M_PI / rate);
	float o2 = omega * omega;
	float o4 = o2 * o2;
	float x1 = o2 + 2.0f * (float)BWPB41 * omega + 4.0f;
	float x2 = o2 + 2.0f * (float)BWPB42 * omega + 4.0f;
	float y1 = o2 - 2.0f * (float)BWPB41 * omega + 4.0f;
	float y2 = o2 - 2.0f * (float)BWPB42 * omega + 4.0f;
	float o228 = 2.0f * o2 - 8.0f;
	float norm = x1 * x2;
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
}
