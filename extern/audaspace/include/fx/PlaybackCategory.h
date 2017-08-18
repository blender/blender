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
* @file PlaybackCategory.h
* @ingroup fx
* The PlaybackCategory class.
*/

#include "devices/IHandle.h"
#include "devices/IDevice.h"
#include "VolumeStorage.h"

#include <unordered_map>
#include <memory>

AUD_NAMESPACE_BEGIN

/**
* This class represents a category of related sounds which are currently playing and allows to control them easily.
*/
class AUD_API PlaybackCategory
{
private:
	/**
	* Next handle ID to be assigned.
	*/
	unsigned int m_currentID;

	/**
	* Vector of handles that belong to the category.
	*/
	std::unordered_map<unsigned int, std::shared_ptr<IHandle>> m_handles;

	/**
	* Device that will play the sounds.
	*/
	std::shared_ptr<IDevice> m_device;

	/**
	* Status of the category.
	*/
	Status m_status;

	/**
	* Volume of all the sounds of the category.
	*/
	std::shared_ptr<VolumeStorage> m_volumeStorage;

	// delete copy constructor and operator=
	PlaybackCategory(const PlaybackCategory&) = delete;
	PlaybackCategory& operator=(const PlaybackCategory&) = delete;

public:
	/**
	* Creates a new PlaybackCategory.
	* \param device A shared pointer to the device which will be used for playback.
	*/
	PlaybackCategory(std::shared_ptr<IDevice> device);
	~PlaybackCategory();

	/**
	* Plays a new sound in the category.
	* \param sound The sound to be played.
	* \return A handle for the playback. If the playback failed, nullptr will be returned.
	*/
	std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound);

	/**
	* Resumes all the paused sounds of the category.
	*/
	void resume();

	/**
	* Pauses all current played back sounds of the category.
	*/
	void pause();

	/**
	* Retrieves the volume of the category.
	* \return The volume.
	*/
	float getVolume();

	/**
	* Sets the volume for the category.
	* \param volume The volume.
	*/
	void setVolume(float volume);

	/**
	* Stops all the playing back or paused sounds.
	*/
	void stop();

	/**
	* Retrieves the shared volume of the category.
	* \return A shared pointer to the VolumeStorage object that represents the shared volume of the category.
	*/
	std::shared_ptr<VolumeStorage> getSharedVolume();

	/**
	* Cleans the category erasing all the invalid handles.
	* Only needed if individual sounds are stopped with their handles.
	*/
	void cleanHandles();

private:
	static void cleanHandleCallback(void* data);
};

AUD_NAMESPACE_END
