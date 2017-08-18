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

#include "fx/Loop.h"
#include "fx/LoopReader.h"

AUD_NAMESPACE_BEGIN

Loop::Loop(std::shared_ptr<ISound> sound, int loop) :
		Effect(sound),
		m_loop(loop)
{
}

int Loop::getLoop() const
{
	return m_loop;
}

std::shared_ptr<IReader> Loop::createReader()
{
	return std::shared_ptr<IReader>(new LoopReader(getReader(), m_loop));
}

AUD_NAMESPACE_END
