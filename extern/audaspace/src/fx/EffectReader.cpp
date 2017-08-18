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

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

EffectReader::EffectReader(std::shared_ptr<IReader> reader)
{
	m_reader = reader;
}

EffectReader::~EffectReader()
{
}

bool EffectReader::isSeekable() const
{
	return m_reader->isSeekable();
}

void EffectReader::seek(int position)
{
	m_reader->seek(position);
}

int EffectReader::getLength() const
{
	return m_reader->getLength();
}

int EffectReader::getPosition() const
{
	return m_reader->getPosition();
}

Specs EffectReader::getSpecs() const
{
	return m_reader->getSpecs();
}

void EffectReader::read(int& length, bool& eos, sample_t* buffer)
{
	m_reader->read(length, eos, buffer);
}

AUD_NAMESPACE_END
