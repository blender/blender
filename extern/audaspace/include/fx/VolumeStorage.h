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
* @file VolumeStorage.h
* @ingroup fx
* The VolumeStorage class.
*/

#include "Audaspace.h"

#include <atomic>

AUD_NAMESPACE_BEGIN

/**
* This class stores a volume value and allows to change if for a number of sounds in one go.
*/
class AUD_API VolumeStorage
{
private:
	/**
	* Volume value.
	*/
	std::atomic<float> m_volume;

	// delete copy constructor and operator=
	VolumeStorage(const VolumeStorage&) = delete;
	VolumeStorage& operator=(const VolumeStorage&) = delete;

public:
	/**
	* Creates a new VolumeStorage instance with volume 1
	*/
	VolumeStorage();

	/**
	* Creates a VolumeStorage instance with an initial value.
	* \param volume The value of the volume.
	*/
	VolumeStorage(float volume);

	/**
	* Retrieves the current volume value.
	* \return The current volume.
	*/
	float getVolume();

	/**
	* Changes the volume value.
	* \param volume The new value for the volume.
	*/
	void setVolume(float volume);
};

AUD_NAMESPACE_END
