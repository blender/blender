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

#include "fx/DynamicIIRFilterReader.h"
#include "fx/IDynamicIIRFilterCalculator.h"

AUD_NAMESPACE_BEGIN

DynamicIIRFilterReader::DynamicIIRFilterReader(std::shared_ptr<IReader> reader, std::shared_ptr<IDynamicIIRFilterCalculator> calculator) :
	IIRFilterReader(reader, std::vector<float>(), std::vector<float>()),
	m_calculator(calculator)
{
	sampleRateChanged(reader->getSpecs().rate);
}

void DynamicIIRFilterReader::sampleRateChanged(SampleRate rate)
{
	std::vector<float> a, b;
	m_calculator->recalculateCoefficients(rate, b, a);
	setCoefficients(b, a);
}

AUD_NAMESPACE_END
