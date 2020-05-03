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

#include "fx/Fader.h"

AUD_NAMESPACE_BEGIN

Fader::Fader(std::shared_ptr<ISound> sound, FadeType type, double start, double length) :
		Effect(sound),
		m_type(type),
		m_start(start),
		m_length(length)
{
}

FadeType Fader::getType() const
{
	return m_type;
}

double Fader::getStart() const
{
	return m_start;
}

double Fader::getLength() const
{
	return m_length;
}

std::shared_ptr<IReader> Fader::createReader()
{
	return std::shared_ptr<IReader>(new FaderReader(getReader(), m_type, m_start, m_length));
}

AUD_NAMESPACE_END
