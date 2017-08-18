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

#include "fx/Threshold.h"
#include "fx/CallbackIIRFilterReader.h"

AUD_NAMESPACE_BEGIN

sample_t Threshold::thresholdFilter(CallbackIIRFilterReader* reader, float* threshold)
{
	float in = reader->x(0);
	if(in >= *threshold)
		return 1;
	else if(in <= -*threshold)
		return -1;
	else
		return 0;
}

void Threshold::endThresholdFilter(float* threshold)
{
	delete threshold;
}

Threshold::Threshold(std::shared_ptr<ISound> sound, float threshold) :
		Effect(sound),
		m_threshold(threshold)
{
}

float Threshold::getThreshold() const
{
	return m_threshold;
}

std::shared_ptr<IReader> Threshold::createReader()
{
	return std::shared_ptr<IReader>(new CallbackIIRFilterReader(getReader(), 1, 0, doFilterIIR(thresholdFilter), endFilterIIR(endThresholdFilter), new float(m_threshold)));
}

AUD_NAMESPACE_END
