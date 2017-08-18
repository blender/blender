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

#include "fx/Accumulator.h"
#include "fx/CallbackIIRFilterReader.h"

AUD_NAMESPACE_BEGIN

sample_t Accumulator::accumulatorFilterAdditive(CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1) + in - lastin;
	if(in > lastin)
		out += in - lastin;
	return out;
}

sample_t Accumulator::accumulatorFilter(CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1);
	if(in > lastin)
		out += in - lastin;
	return out;
}

Accumulator::Accumulator(std::shared_ptr<ISound> sound,
											   bool additive) :
		Effect(sound),
		m_additive(additive)
{
}

std::shared_ptr<IReader> Accumulator::createReader()
{
	return std::shared_ptr<IReader>(new CallbackIIRFilterReader(getReader(), 2, 2, m_additive ? accumulatorFilterAdditive : accumulatorFilter));
}

AUD_NAMESPACE_END
