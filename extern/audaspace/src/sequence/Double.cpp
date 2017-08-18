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

#include "sequence/Double.h"
#include "sequence/DoubleReader.h"

AUD_NAMESPACE_BEGIN

Double::Double(std::shared_ptr<ISound> sound1, std::shared_ptr<ISound> sound2) :
	m_sound1(sound1), m_sound2(sound2)
{
}

std::shared_ptr<IReader> Double::createReader()
{
	std::shared_ptr<IReader> reader1 = m_sound1->createReader();
	std::shared_ptr<IReader> reader2 = m_sound2->createReader();

	return std::shared_ptr<IReader>(new DoubleReader(reader1, reader2));
}

AUD_NAMESPACE_END
