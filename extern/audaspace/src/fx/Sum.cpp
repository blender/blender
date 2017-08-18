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

#include "fx/Sum.h"
#include "fx/IIRFilterReader.h"

AUD_NAMESPACE_BEGIN

Sum::Sum(std::shared_ptr<ISound> sound) :
		Effect(sound)
{
}

std::shared_ptr<IReader> Sum::createReader()
{
	std::vector<float> a, b;
	a.push_back(1);
	a.push_back(-1);
	b.push_back(1);
	return std::shared_ptr<IReader>(new IIRFilterReader(getReader(), b, a));
}

AUD_NAMESPACE_END
