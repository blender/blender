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

#include "sequence/SuperposeReader.h"
#include "Exception.h"

#include <algorithm>
#include <cstring>

AUD_NAMESPACE_BEGIN

SuperposeReader::SuperposeReader(std::shared_ptr<IReader> reader1, std::shared_ptr<IReader> reader2) :
	m_reader1(reader1), m_reader2(reader2)
{
}

SuperposeReader::~SuperposeReader()
{
}

bool SuperposeReader::isSeekable() const
{
	return m_reader1->isSeekable() && m_reader2->isSeekable();
}

void SuperposeReader::seek(int position)
{
	m_reader1->seek(position);
	m_reader2->seek(position);
}

int SuperposeReader::getLength() const
{
	int len1 = m_reader1->getLength();
	int len2 = m_reader2->getLength();
	if((len1 < 0) || (len2 < 0))
		return -1;
	return std::max(len1, len2);
}

int SuperposeReader::getPosition() const
{
	int pos1 = m_reader1->getPosition();
	int pos2 = m_reader2->getPosition();
	return std::max(pos1, pos2);
}

Specs SuperposeReader::getSpecs() const
{
	return m_reader1->getSpecs();
}

void SuperposeReader::read(int& length, bool& eos, sample_t* buffer)
{
	Specs specs = m_reader1->getSpecs();
	Specs s2 = m_reader2->getSpecs();
	if(!AUD_COMPARE_SPECS(specs, s2))
		AUD_THROW(StateException, "Two readers with different specifiactions cannot be superposed.");

	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer.assureSize(length * samplesize);

	int len1 = length;
	m_reader1->read(len1, eos, buffer);

	if(len1 < length)
		std::memset(buffer + len1 * specs.channels, 0, (length - len1) * samplesize);

	int len2 = length;
	bool eos2;
	sample_t* buf = m_buffer.getBuffer();
	m_reader2->read(len2, eos2, buf);

	for(int i = 0; i < len2 * specs.channels; i++)
		buffer[i] += buf[i];

	length = std::max(len1, len2);
	eos &= eos2;
}

AUD_NAMESPACE_END
