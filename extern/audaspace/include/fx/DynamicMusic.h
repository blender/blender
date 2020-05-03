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
* @file DynamicMusic.h
* @ingroup fx
* The DynamicMusic class.
*/

#include "devices/IHandle.h"
#include "devices/IDevice.h"
#include "ISound.h"

#include <memory>
#include <vector>
#include <thread>
#include <atomic> 
#include <condition_variable>
#include <mutex>

AUD_NAMESPACE_BEGIN

/**
* This class allows to play music depending on a current "scene", scene changes are managed by the class.
* The default scene is silent and has id 0.
*/
class AUD_API DynamicMusic
{
private:
	/**
	* Matrix of pointers which will store the sounds of the scenes and the transitions between them.
	*/
	std::vector<std::vector<std::shared_ptr<ISound>>> m_scenes;

	/**
	* Id of the current scene.
	*/
	std::atomic_int m_id;

	/**
	* Length of the crossfade transition in seconds, used when no custom transition has been set.
	*/
	double m_fadeTime;

	/**
	* Handle to the playback of the current scene.
	*/
	std::shared_ptr<IHandle> m_currentHandle;

	/**
	 * Handle used during transitions.
	 */
	std::shared_ptr<IHandle> m_transitionHandle;

	/**
	* Device used for playback.
	*/
	std::shared_ptr<IDevice> m_device;

	/**
	* Flag that is true when a transition is happening.
	*/
	std::atomic_bool m_transitioning;

	/**
	* Flag that is true when the music is paused.
	*/
	std::atomic_bool m_stopThread;

	/**
	* Id of the sound that will play with the next transition.
	*/
	std::atomic_int m_soundTarget;

	/**
	* Volume of the scenes.
	*/
	float m_volume;

	/**
	* A thread that manages the crossfade transition.
	*/
	std::thread m_fadeThread;

	// delete copy constructor and operator=
	DynamicMusic(const DynamicMusic&) = delete;
	DynamicMusic& operator=(const DynamicMusic&) = delete;

public:
	/**
	* Creates a new dynamic music manager with the default silent scene (id: 0).
	* \param device The device that will be used to play sounds.
	*/
	DynamicMusic(std::shared_ptr<IDevice> device);

	virtual ~DynamicMusic();

	/**
	* Adds a new scene to the manager.
	* \param sound The sound that will play when the scene is selected with the changeScene().
	* \return The identifier of the new scene.
	*/
	int addScene(std::shared_ptr<ISound> sound);

	/**
	* Changes to another scene.
	* \param id The id of the scene which should start playing the changeScene method.
	* \return
	*        - true if the change has been scheduled succesfully.
	*        - false if there already is a transition in course or the scene selected doesnt exist.
	*/
	bool changeScene(int id);

	/**
	* Retrieves the scene currently selected.
	* \return The identifier of the current scene.
	*/
	int getScene();

	/**
	* Adds a new transition between scenes
	* \param init The id of the initial scene that will allow the transition to play.
	* \param end The id if the target scene for the transition.
	* \param sound The sound that will play when the scene changes from init to end.
	* \return false if the init or end scenes don't exist.
	*/
	bool addTransition(int init, int end, std::shared_ptr<ISound> sound);

	/**
	* Sets the length of the crossfade transition (default 1 second).
	* \param seconds The time in seconds.
	*/
	void setFadeTime(double seconds);

	/**
	* Gets the length of the crossfade transition (default 1 second).
	* \return The length of the cressfade transition in seconds.
	*/
	double getFadeTime();

	/**
	* Resumes a paused sound.
	* \return
	*        - true if the sound has been resumed.
	*        - false if the sound isn't paused or the handle is invalid.
	*/
	bool resume();

	/**
	* Pauses the current played back sound.
	* \return
	*        - true if the sound has been paused.
	*        - false if the sound isn't playing back or the handle is invalid.
	*/
	bool pause();

	/**
	* Seeks in the current played back sound.
	* \param position The new position from where to play back, in seconds.
	* \return
	*        - true if the handle is valid.
	*        - false if the handle is invalid.
	* \warning Whether the seek works or not depends on the sound source.
	*/
	bool seek(double position);

	/**
	* Retrieves the current playback position of a sound.
	* \return The playback position in seconds, or 0.0 if the handle is
	*         invalid.
	*/
	double getPosition();

	/**
	* Retrieves the volume of the scenes.
	* \return The volume. 
	*/
	float getVolume();

	/**
	* Sets the volume for the scenes.
	* \param volume The volume.
	* \return
	*        - true if the handle is valid.
	*        - false if the handle is invalid.
	*/
	bool setVolume(float volume);

	/**
	* Returns the status of the current played back sound.
	* \return
	*        - STATUS_INVALID if the sound has stopped or the handle is
	*.         invalid
	*        - STATUS_PLAYING if the sound is currently played back.
	*        - STATUS_PAUSED if the sound is currently paused.
	*        - STATUS_STOPPED if the sound finished playing and is still
	*          kept in the device.
	* \see Status
	*/
	Status getStatus();

	/**
	* Stops any played back or paused sound and sets the dynamic music player to default silent state (scene 0)
	* \return
	*        - true if the sound has been stopped.
	*        - false if the handle is invalid.
	*/
	bool stop();

	private:
		//Callbacks used to schedule transitions after a sound ends.
		static void transitionCallback(void* player);
		static void sceneCallback(void* player);
		//These functions can fade sounds in and out if used with a thread.
		void crossfadeThread();
		void fadeInThread();
		void fadeOutThread();
};

AUD_NAMESPACE_END
