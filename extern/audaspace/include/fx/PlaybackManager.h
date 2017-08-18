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
* @file PlaybackManager.h
* @ingroup fx
* The PlaybackManager class.
*/

#include "PlaybackCategory.h"
#include "devices/IDevice.h"
#include "ISound.h"

#include <unordered_map>
#include <memory>

AUD_NAMESPACE_BEGIN

/**
* This class allows to control groups of playing sounds easily.
* The sounds are part of categories.
*/
class AUD_API PlaybackManager
{
private:
	/**
	* Unordered map of categories, each category has different name.
	*/
	std::unordered_map<unsigned int, std::shared_ptr<PlaybackCategory>> m_categories;

	/**
	* Device used for playback.
	*/
	std::shared_ptr<IDevice> m_device;

	/**
	* The current key used for new categories.
	*/
	unsigned int m_currentKey;

	// delete copy constructor and operator=
	PlaybackManager(const PlaybackManager&) = delete;
	PlaybackManager& operator=(const PlaybackManager&) = delete;

public:
	/**
	* Creates a new PlaybackManager.
	* \param device A shared pointer to the device which will be used for playback.
	*/
	PlaybackManager(std::shared_ptr<IDevice> device);

	/**
	* Adds an existent category to the manager and returns a key to access it.
	* \param category The category to be added.
	* \return The category key.
	*/
	unsigned int addCategory(std::shared_ptr<PlaybackCategory> category);

	/**
	* Adds an existent category to the manager and returns a key to access it.
	* \param volume The volume of the new category.
	* \return The category key.
	*/
	unsigned int addCategory(float volume);

	/**
	* Plays a sound and adds it to a new or existent category.
	* \param sound The sound to be played and added to a category.
	* \param catKey Key of the category.
	* \return The handle of the playback; nullptr if the sound couldn't be played.
	*/
	std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound, unsigned int catKey);

	/**
	* Resumes all the paused sounds of a category.
	* \param catKey Key of the category.
	* \return
	*        - true if succesful.
	*        - false if the category doesn't exist.
	*/
	bool resume(unsigned int catKey);

	/**
	* Pauses all current playing sounds of a category.
	* \param catKey Key of the category.
	* \return
	*        - true if succesful.
	*        - false if the category doesn't exist.
	*/
	bool pause(unsigned int catKey);

	/**
	* Retrieves the volume of a category.
	* \param catKey Key of the category.
	* \return The volume value of the category. If the category doesn't exist it returns a negative number.
	*/
	float getVolume(unsigned int catKey);

	/**
	* Sets the volume for a category.
	* \param volume The volume.
	* \param catKey Key of the category.
	* \return
	*        - true if succesful.
	*        - false if the category doesn't exist.
	*/
	bool setVolume(float volume, unsigned int catKey);

	/**
	* Stops and erases a category of sounds.
	* \param catKey Key of the category.
	* \return
	*        - true if succesful.
	*        - false if the category doesn't exist.
	*/
	bool stop(unsigned int catKey);

	/**
	* Removes all the invalid handles of all the categories.
	* Only needed if individual sounds are stopped with their handles.
	*/
	void clean();

	/**
	* Removes all the invalid handles of a category.
	* Only needed if individual sounds are stopped with their handles.
	* \param catKey Key of the category.
	* \return
	*        - true if succesful.
	*        - false if the category doesn't exist.
	*/
	bool clean(unsigned int catKey);

	/**
	* Retrieves the device of the PlaybackManager.
	* \return A shared pointer to the device used by the playback manager.
	*/
	std::shared_ptr<IDevice> getDevice();
};

AUD_NAMESPACE_END
