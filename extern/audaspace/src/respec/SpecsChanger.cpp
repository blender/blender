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

#include "respec/SpecsChanger.h"

AUD_NAMESPACE_BEGIN

std::shared_ptr<IReader> SpecsChanger::getReader() const
{
	return m_sound->createReader();
}

SpecsChanger::SpecsChanger(std::shared_ptr<ISound> sound,
								   DeviceSpecs specs) :
	m_specs(specs), m_sound(sound)
{
}

DeviceSpecs SpecsChanger::getSpecs() const
{
	return m_specs;
}

std::shared_ptr<ISound> SpecsChanger::getSound() const
{
	return m_sound;
}

AUD_NAMESPACE_END
