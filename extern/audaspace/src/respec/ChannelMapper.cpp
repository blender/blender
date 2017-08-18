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

#include "respec/ChannelMapper.h"
#include "respec/ChannelMapperReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

ChannelMapper::ChannelMapper(std::shared_ptr<ISound> sound, DeviceSpecs specs) :
		SpecsChanger(sound, specs)
{
}

std::shared_ptr<IReader> ChannelMapper::createReader()
{
	std::shared_ptr<IReader> reader = getReader();
	return std::shared_ptr<IReader>(new ChannelMapperReader(reader, m_specs.channels));
}

AUD_NAMESPACE_END
