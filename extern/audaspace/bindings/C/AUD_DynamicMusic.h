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

#include "AUD_Types.h"
#include "AUD_Handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Creates a new dynamic music player.
* \param device The device that will be used to play sounds.
* \return The new DynamicMusic object.
*/
extern AUD_API AUD_DynamicMusic* AUD_DynamicMusic_create(AUD_Device* device);

/**
* Deletes a dynamic music player.
* \param player The DynamicMusic object to be deleted.
*/
extern AUD_API void AUD_DynamicMusic_free(AUD_DynamicMusic* player);

/**
* Adds a sound scene to a dynamic music player.
* \param player The DynamicMusic object.
* \param scene The sound to be added as a scene.
* \return The index of the new scene.
*/
extern AUD_API int AUD_DynamicMusic_addScene(AUD_DynamicMusic* player, AUD_Sound* scene);

/**
* Changes the current sound scene of a dynamic music player.
* \param player The DynamicMusic object.
* \param scene The index of the scene to be played.
* \return 0 if the target scene doesn't exist.
*/
extern AUD_API int AUD_DynamicMusic_setSecene(AUD_DynamicMusic* player, int scene);

/**
* Retrives the index of the current scene.
* \param player The DynamicMusic object.
* \return The index of the current scene.
*/
extern AUD_API int AUD_DynamicMusic_getScene(AUD_DynamicMusic* player);

/**
* Adds a new transition between two scenes.
* \param player The DynamicMusic object.
* \param ini The origin scene for the transition.
* \param end The end scene for the transition.
* \param transition A sound that will be used as transition between two scenes.
* \return 0 if the ini or end scenes don't exist.
*/
extern AUD_API int AUD_DynamicMusic_addTransition(AUD_DynamicMusic* player, int ini, int end, AUD_Sound* transition);

/**
* Changes the fade time for the default transitions of a dynamic music player.
* \param player The DynamicMusic object.
* \param seconds The amount of secods that the crossfade transition will take.
*/
extern AUD_API void AUD_DynamicMusic_setFadeTime(AUD_DynamicMusic* player, float seconds);

/**
* Retrieves the fade time of a dynamic music player.
* \param player The DynamicMusic object.
* \return The fade time of the player.
*/
extern AUD_API float AUD_DynamicMusic_getFadeTime(AUD_DynamicMusic* player);

/**
* Resumes the current scene playback of a dynamic music player if it is paused.
* \param player The DynamicMusic object.
* \return 0 if the playback wasn't resumed.
*/
extern AUD_API int AUD_DynamicMusic_resume(AUD_DynamicMusic* player);

/**
* Pauses the current scene of a dynamic music player.
* \param player The DynamicMusic object.
* \return 0 if the playback wasn't paused.
*/
extern AUD_API int AUD_DynamicMusic_pause(AUD_DynamicMusic* player);

/**
* Seeks the current playing scene of a dynamic music player.
* \param player The DynamicMusic object.
* \param position The new position from which to play back, in seconds.
* \return 0 if the seeking wasn't possible.
*/
extern AUD_API int AUD_DynamicMusic_seek(AUD_DynamicMusic* player, float position);

/**
* Retrieves the position of the current scene of a dynamic music player.
* \param player The DynamicMusic object.
* \return The position of the current playing scene.
*/
extern AUD_API float AUD_DynamicMusic_getPosition(AUD_DynamicMusic* player);

/**
* Retrieves the volume of the current scene of a dynamic music player.
* \param player The DynamicMusic object.
* \return The volume of the current playing scene.
*/
extern AUD_API float AUD_DynamicMusic_getVolume(AUD_DynamicMusic* player);

/**
* Changes the volume of the current scene in a dynamic music player.
* \param player The DynamicMusic object.
* \param 0 if the volume couldn't be changed.
*/
extern AUD_API int AUD_DynamicMusic_setVolume(AUD_DynamicMusic* player, float volume);

/**
* Retrieves the status of the current scene in a dynamic music player.
* \param player The DynamicMusic object.
* \return The Status of the current playing scene.
*/
extern AUD_API AUD_Status AUD_DynamicMusic_getStatus(AUD_DynamicMusic* player);

/**
* Stops the current scene of a dynamic music player.
* \param player The DynamicMusic object.
* \return 0 if the playback wasn't stopped.
*/
extern AUD_API int AUD_DynamicMusic_stop(AUD_DynamicMusic* player);

#ifdef __cplusplus
}
#endif