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

#include "sequence/DoubleReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

DoubleReader::DoubleReader(std::shared_ptr<IReader> reader1, std::shared_ptr<IReader> reader2) :
	m_reader1(reader1), m_reader2(reader2), m_finished1(false)
{
	Specs s1, s2;
	s1 = reader1->getSpecs();
	s2 = reader2->getSpecs();
}

DoubleReader::~DoubleReader()
{
}

bool DoubleReader::isSeekable() const
{
	return m_reader1->isSeekable() && m_reader2->isSeekable();
}

void DoubleReader::seek(int position)
{
	m_reader1->seek(position);

	int pos1 = m_reader1->getPosition();

	if((m_finished1 = (pos1 < position)))
		m_reader2->seek(position - pos1);
	else
		m_reader2->seek(0);
}

int DoubleReader::getLength() const
{
	int len1 = m_reader1->getLength();
	int len2 = m_reader2->getLength();
	if(len1 < 0 || len2 < 0)
		return -1;
	return len1 + len2;
}

int DoubleReader::getPosition() const
{
	return m_reader1->getPosition() + m_reader2->getPosition();
}

Specs DoubleReader::getSpecs() const
{
	return m_finished1 ? m_reader1->getSpecs() : m_reader2->getSpecs();
}

void DoubleReader::read(int& length, bool& eos, sample_t* buffer)
{
	eos = false;

	if(!m_finished1)
	{
		int len = length;

		m_reader1->read(len, m_finished1, buffer);

		if(len < length)
		{
			Specs specs1, specs2;
			specs1 = m_reader1->getSpecs();
			specs2 = m_reader2->getSpecs();
			if(AUD_COMPARE_SPECS(specs1, specs2))
			{
				int len2 = length - len;
				m_reader2->read(len2, eos, buffer + specs1.channels * len);
				length = len + len2;
			}
			else
				length = len;
		}
	}
	else
	{
		m_reader2->read(length, eos, buffer);
	}
}

AUD_NAMESPACE_END
