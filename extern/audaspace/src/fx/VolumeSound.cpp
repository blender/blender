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

#include "fx/VolumeSound.h"
#include "fx/VolumeReader.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

VolumeSound::VolumeSound(std::shared_ptr<ISound> sound, std::shared_ptr<VolumeStorage> volumeStorage) :
	m_sound(sound), m_volumeStorage(volumeStorage)
{
}

std::shared_ptr<IReader> VolumeSound::createReader()
{
	return std::make_shared<VolumeReader>(m_sound->createReader(), m_volumeStorage);
}

std::shared_ptr<VolumeStorage> VolumeSound::getSharedVolume()
{
	return m_volumeStorage;
}

void VolumeSound::setSharedVolume(std::shared_ptr<VolumeStorage> volumeStorage)
{
	m_volumeStorage = volumeStorage;
}

AUD_NAMESPACE_END