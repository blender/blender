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

#include "fx/ADSRReader.h"

AUD_NAMESPACE_BEGIN

ADSRReader::ADSRReader(std::shared_ptr<IReader> reader, float attack, float decay, float sustain, float release) :
	EffectReader(reader),
	m_attack(attack), m_decay(decay), m_sustain(sustain), m_release(release)
{
	nextState(ADSR_STATE_ATTACK);
}

ADSRReader::~ADSRReader()
{
}

void ADSRReader::nextState(ADSRState state)
{
	m_state = state;

	switch(m_state)
	{
	case ADSR_STATE_ATTACK:
		m_level = 0;
		if(m_attack <= 0)
		{
			nextState(ADSR_STATE_DECAY);
			return;
		}
		return;
	case ADSR_STATE_DECAY:
		if(m_decay <= 0)
		{
			nextState(ADSR_STATE_SUSTAIN);
			return;
		}
		if(m_level > 1.0)
			m_level = 1 - (m_level - 1) * m_attack / m_decay * (1 - m_sustain);
		if(m_level <= m_sustain)
			nextState(ADSR_STATE_SUSTAIN);
		break;
	case ADSR_STATE_SUSTAIN:
		m_level = m_sustain;
		break;
	case ADSR_STATE_RELEASE:
		if(m_release <= 0)
		{
			nextState(ADSR_STATE_INVALID);
			return;
		}
		break;
	case ADSR_STATE_INVALID:
		break;
	}
}

void ADSRReader::read(int & length, bool &eos, sample_t* buffer)
{
	Specs specs = m_reader->getSpecs();
	m_reader->read(length, eos, buffer);

	for(int i = 0; i < length; i++)
	{
		for(int channel = 0; channel < specs.channels; channel++)
		{
			buffer[i * specs.channels + channel] *= m_level;
		}

		switch(m_state)
		{
		case ADSR_STATE_ATTACK:
			m_level += 1 / m_attack / specs.rate;
			if(m_level >= 1)
				nextState(ADSR_STATE_DECAY);
			break;
		case ADSR_STATE_DECAY:
			m_level -= (1 - m_sustain) /  m_decay / specs.rate;
			if(m_level <= m_sustain)
				nextState(ADSR_STATE_SUSTAIN);
			break;
		case ADSR_STATE_SUSTAIN:
			break;
		case ADSR_STATE_RELEASE:
			m_level -= m_sustain / m_release / specs.rate ;
			if(m_level <= 0)
				nextState(ADSR_STATE_INVALID);
			break;
		case ADSR_STATE_INVALID:
			length = i;
			return;
		}
	}
}

void ADSRReader::release()
{
	nextState(ADSR_STATE_RELEASE);
}

AUD_NAMESPACE_END
