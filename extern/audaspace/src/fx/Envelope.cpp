/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "fx/Envelope.h"
#include "fx/CallbackIIRFilterReader.h"

#include <cmath>

AUD_NAMESPACE_BEGIN

struct EnvelopeParameters
{
	float attack;
	float release;
	float threshold;
	float arthreshold;
};

sample_t Envelope::envelopeFilter(CallbackIIRFilterReader* reader, EnvelopeParameters* param)
{
	float in = std::fabs(reader->x(0));
	float out = reader->y(-1);
	if(in < param->threshold)
		in = 0.0f;
	return (in > out ? param->attack : param->release) * (out - in) + in;
}

void Envelope::endEnvelopeFilter(EnvelopeParameters* param)
{
	delete param;
}

Envelope::Envelope(std::shared_ptr<ISound> sound, float attack, float release, float threshold, float arthreshold) :
		Effect(sound),
		m_attack(attack),
		m_release(release),
		m_threshold(threshold),
		m_arthreshold(arthreshold)
{
}

std::shared_ptr<IReader> Envelope::createReader()
{
	std::shared_ptr<IReader> reader = getReader();

	EnvelopeParameters* param = new EnvelopeParameters();
	param->arthreshold = m_arthreshold;
	param->attack = std::pow(m_arthreshold, 1.0f/(static_cast<float>(reader->getSpecs().rate) * m_attack));
	param->release = std::pow(m_arthreshold, 1.0f/(static_cast<float>(reader->getSpecs().rate) * m_release));
	param->threshold = m_threshold;

	return std::shared_ptr<IReader>(new CallbackIIRFilterReader(reader, 1, 2,
										   (doFilterIIR) envelopeFilter,
										   (endFilterIIR) endEnvelopeFilter,
										   param));
}

AUD_NAMESPACE_END
