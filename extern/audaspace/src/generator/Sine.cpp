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

#include "generator/Sine.h"
#include "generator/SineReader.h"

AUD_NAMESPACE_BEGIN

Sine::Sine(float frequency, SampleRate sampleRate) :
	m_frequency(frequency),
	m_sampleRate(sampleRate)
{
}

float Sine::getFrequency() const
{
	return m_frequency;
}

std::shared_ptr<IReader> Sine::createReader()
{
	return std::shared_ptr<IReader>(new SineReader(m_frequency, m_sampleRate));
}

AUD_NAMESPACE_END
