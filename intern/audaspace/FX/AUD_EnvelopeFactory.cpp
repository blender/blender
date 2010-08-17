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

sample_t envelopeFilter(AUD_CallbackIIRFilterReader* reader, EnvelopeParameters* param)
{
	float in = fabs(reader->x(0));
	float out = reader->y(-1);
	if(in < param->threshold)
		in = 0.0f;
	return (in > out ? param->attack : param->release) * (out - in) + in;
}

void endEnvelopeFilter(EnvelopeParameters* param)
{
	delete param;
}

AUD_EnvelopeFactory::AUD_EnvelopeFactory(AUD_IFactory* factory, float attack,
										 float release, float threshold,
										 float arthreshold) :
		AUD_EffectFactory(factory),
		m_attack(attack),
		m_release(release),
		m_threshold(threshold),
		m_arthreshold(arthreshold)
{
}

AUD_IReader* AUD_EnvelopeFactory::createReader() const
{
	AUD_IReader* reader = getReader();

	EnvelopeParameters* param = new EnvelopeParameters();
	param->arthreshold = m_arthreshold;
	param->attack = pow(m_arthreshold, 1.0f/(reader->getSpecs().rate * m_attack));
	param->release = pow(m_arthreshold, 1.0f/(reader->getSpecs().rate * m_release));
	param->threshold = m_threshold;

	return new AUD_CallbackIIRFilterReader(reader, 1, 2,
										   (doFilterIIR) envelopeFilter,
										   (endFilterIIR) endEnvelopeFilter,
										   param);
}
