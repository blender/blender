/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
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

#include "fx/MutableReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

MutableReader::MutableReader(std::shared_ptr<ISound> sound) :
m_sound(sound)
{
	m_reader = m_sound->createReader();
}

bool MutableReader::isSeekable() const
{
	return m_reader->isSeekable();
}

void MutableReader::seek(int position)
{
	if(position < m_reader->getPosition())
	{
		m_reader = m_sound->createReader();
	}
	else
		m_reader->seek(position);
}

int MutableReader::getLength() const
{
	return m_reader->getLength();
}

int MutableReader::getPosition() const
{
	return m_reader->getPosition();
}

Specs MutableReader::getSpecs() const
{
	return m_reader->getSpecs();
}

void MutableReader::read(int& length, bool& eos, sample_t* buffer)
{
	m_reader->read(length, eos, buffer);
}

AUD_NAMESPACE_END
