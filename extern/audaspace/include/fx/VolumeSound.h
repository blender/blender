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

#pragma once

/**
* @file VolumeSound.h
* @ingroup fx
* The VolumeSound class.
*/

#include "ISound.h"
#include "VolumeStorage.h"
#include <memory>

AUD_NAMESPACE_BEGIN

/**
* This class allows to create a sound with its own volume.
*/
class AUD_API VolumeSound : public ISound
{
private:
	/**
	* A pointer to a sound.
	*/
	std::shared_ptr<ISound> m_sound;

	/**
	* A pointer to the shared volume being used.
	*/
	std::shared_ptr<VolumeStorage> m_volumeStorage;

	// delete copy constructor and operator=
	VolumeSound(const VolumeSound&) = delete;
	VolumeSound& operator=(const VolumeSound&) = delete;

public:
	/**
	* Creates a new VolumeSound.
	* \param sound The sound in which shall have its own volume.
	* \param volumeStorage A shared pointer to a VolumeStorage object. It allows to change the volume of various sound in one go.
	*/
	VolumeSound(std::shared_ptr<ISound> sound, std::shared_ptr<VolumeStorage> volumeStorage);

	virtual std::shared_ptr<IReader> createReader();

	/**
	* Retrieves the shared volume of this sound.
	* \return A shared pointer to the VolumeStorage object that this sound is using.
	*/
	std::shared_ptr<VolumeStorage> getSharedVolume();

	/**
	* Changes the shared volume of this sound, it'll only affect newly created readers.
	* \param volumeStorage A shared pointer to the new VolumeStorage object.
	*/
	void setSharedVolume(std::shared_ptr<VolumeStorage> volumeStorage);
};

AUD_NAMESPACE_END