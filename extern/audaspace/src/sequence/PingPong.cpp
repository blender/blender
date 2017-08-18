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

#include "sequence/PingPong.h"
#include "sequence/DoubleReader.h"
#include "fx/ReverseReader.h"

AUD_NAMESPACE_BEGIN

PingPong::PingPong(std::shared_ptr<ISound> sound) :
		Effect(sound)
{
}

std::shared_ptr<IReader> PingPong::createReader()
{
	std::shared_ptr<IReader> reader = getReader();
	std::shared_ptr<IReader> reader2 = std::shared_ptr<IReader>(new ReverseReader(getReader()));

	return std::shared_ptr<IReader>(new DoubleReader(reader, reader2));
}

AUD_NAMESPACE_END
