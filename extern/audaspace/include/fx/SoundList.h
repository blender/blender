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
* @file SoundList.h
* @ingroup fx
* The SoundList class.
*/

#include "ISound.h"

#include <vector>
#include <memory>
#include <mutex>

AUD_NAMESPACE_BEGIN

/**
* This class allows to have a list of sound that will play sequentially or randomly with each playback.
*/
class AUD_API SoundList : public ISound
{
private:
	/**
	* The list of sounds that will play.
	*/
	std::vector<std::shared_ptr<ISound>> m_list;

	/**
	* Flag for random playback
	*/
	bool m_random = false;

	/**
	* Current sound index. -1 if no reader has been created.
	*/
	int m_index = -1;

	/**
	* Mutex to prevent multithreading crashes.
	*/
	std::recursive_mutex m_mutex;

	// delete copy constructor and operator=
	SoundList(const SoundList&) = delete;
	SoundList& operator=(const SoundList&) = delete;

public:
	/**
	* Creates a new, empty sound list.
	* Sounds must be added to the list using the addSound() method. 
	* \param random False if the sounds int he list must be played sequentially. True if random.
	*/
	SoundList(bool random = false);

	/**
	* Creates a new sound list and initializes it.
	* \param list A vector with sounds to initialize the list.
	* \param random False if the sounds int he list must be played sequentially. True if random.
	*/
	SoundList(std::vector<std::shared_ptr<ISound>>& list, bool random = false);

	virtual std::shared_ptr<IReader> createReader();

	/**
	* Adds a sound to the list.
	* The added sounds can be played sequentially or randomly dependig 
	* on the m_random flag
	* \param sound A shared_ptr to the sound.
	*/
	void addSound(std::shared_ptr<ISound> sound);

	/**
	* Sets the playback mode of the sound list.
	* There are two posible modes, random and sequential.
	* \param random True to activate the random mode, false to activate sequential mode.
	*/
	void setRandomMode(bool random);

	/**
	* Returns the playback mode of the sound list.
	* The two posible modes are random and sequential.
	* \return True if the random mode is activated, false otherwise.
	*/
	bool getRandomMode();

	/**
	* Returns the amount of sounds in the list.
	* \return The amount of sounds in the list.
	*/
	int getSize();

};

AUD_NAMESPACE_END
