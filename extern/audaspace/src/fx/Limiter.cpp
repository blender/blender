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

#include "fx/Limiter.h"
#include "fx/LimiterReader.h"

AUD_NAMESPACE_BEGIN

Limiter::Limiter(std::shared_ptr<ISound> sound,
									   float start, float end) :
		Effect(sound),
		m_start(start),
		m_end(end)
{
}

float Limiter::getStart() const
{
	return m_start;
}

float Limiter::getEnd() const
{
	return m_end;
}

std::shared_ptr<IReader> Limiter::createReader()
{
	return std::shared_ptr<IReader>(new LimiterReader(getReader(), m_start, m_end));
}

AUD_NAMESPACE_END
