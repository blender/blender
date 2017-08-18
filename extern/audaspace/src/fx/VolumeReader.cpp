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

#include "fx/VolumeReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

VolumeReader::VolumeReader(std::shared_ptr<IReader> reader, std::shared_ptr<VolumeStorage> volumeStorage) :
	m_reader(reader), m_volumeStorage(volumeStorage)
{
}

bool VolumeReader::isSeekable() const
{
	return m_reader->isSeekable();
}

void VolumeReader::seek(int position)
{
	m_reader->seek(position);
}

int VolumeReader::getLength() const
{
	return m_reader->getLength();
}

int VolumeReader::getPosition() const
{
	return m_reader->getPosition();
}

Specs VolumeReader::getSpecs() const
{
	return m_reader->getSpecs();
}

void VolumeReader::read(int& length, bool& eos, sample_t* buffer)
{
	m_reader->read(length, eos, buffer);
	for(int i = 0; i < length * m_reader->getSpecs().channels; i++)
		buffer[i] = buffer[i] * m_volumeStorage->getVolume();
}

AUD_NAMESPACE_END