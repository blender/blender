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

/** \file audaspace/FX/AUD_EnvelopeFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_EnvelopeFactory.h"
#include "AUD_CallbackIIRFilterReader.h"

#include <cmath>

struct EnvelopeParameters
{
	float attack;
	float release;
	float threshold;
	float arthreshold;
};

sample_t AUD_EnvelopeFactory::envelopeFilter(AUD_CallbackIIRFilterReader* reader, EnvelopeParameters* param)
{
	float in = fabs(reader->x(0));
	float out = reader->y(-1);
	if(in < param->threshold)
		in = 0.0f;
	return (in > out ? param->attack : param->release) * (out - in) + in;
}

void AUD_EnvelopeFactory::endEnvelopeFilter(EnvelopeParameters* param)
{
	delete param;
}

AUD_EnvelopeFactory::AUD_EnvelopeFactory(boost::shared_ptr<AUD_IFactory> factory, float attack,
										 float release, float threshold,
										 float arthreshold) :
		AUD_EffectFactory(factory),
		m_attack(attack),
		m_release(release),
		m_threshold(threshold),
		m_arthreshold(arthreshold)
{
}

boost::shared_ptr<AUD_IReader> AUD_EnvelopeFactory::createReader()
{
	boost::shared_ptr<AUD_IReader> reader = getReader();

	EnvelopeParameters* param = new EnvelopeParameters();
	param->arthreshold = m_arthreshold;
	param->attack = pow(m_arthreshold, 1.0f/(static_cast<float>(reader->getSpecs().rate) * m_attack));
	param->release = pow(m_arthreshold, 1.0f/(static_cast<float>(reader->getSpecs().rate) * m_release));
	param->threshold = m_threshold;

	return boost::shared_ptr<AUD_IReader>(new AUD_CallbackIIRFilterReader(reader, 1, 2,
										   (doFilterIIR) envelopeFilter,
										   (endFilterIIR) endEnvelopeFilter,
										   param));
}
