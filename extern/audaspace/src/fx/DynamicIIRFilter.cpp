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

#include "fx/DynamicIIRFilter.h"
#include "fx/DynamicIIRFilterReader.h"

AUD_NAMESPACE_BEGIN

DynamicIIRFilter::DynamicIIRFilter(std::shared_ptr<ISound> sound,
														 std::shared_ptr<IDynamicIIRFilterCalculator> calculator) :
	Effect(sound),
	m_calculator(calculator)
{
}

std::shared_ptr<IReader> DynamicIIRFilter::createReader()
{
	return std::shared_ptr<IReader>(new DynamicIIRFilterReader(getReader(), m_calculator));
}


AUD_NAMESPACE_END
