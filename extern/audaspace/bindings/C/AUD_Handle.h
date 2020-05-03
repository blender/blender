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

#pragma once

#include "AUD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Status of a playback handle.
typedef enum
{
	AUD_STATUS_INVALID = 0,			/// Invalid handle. Maybe due to stopping.
	AUD_STATUS_PLAYING,				/// Sound is playing.
	AUD_STATUS_PAUSED,				/// Sound is being paused.
	AUD_STATUS_STOPPED				/// Sound is stopped but kept in the device.
} AUD_Status;

/**
 * Pauses a played back sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been playing or not.
 */
extern AUD_API int AUD_Handle_pause(AUD_Handle* handle);

/**
 * Resumes a paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been paused or not.
 */
extern AUD_API int AUD_Handle_resume(AUD_Handle* handle);

/**
 * Stops a playing or paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been valid or not.
 */
extern AUD_API int AUD_Handle_stop(AUD_Handle* handle);

/**
 * Retrieves the attenuation of a handle.
 * param handle The handle to get the attenuation from.
 * return The attenuation of the handle.
 */
extern AUD_API float AUD_Handle_getAttenuation(AUD_Handle* handle);

/**
 * Sets the attenuation of a handle.
 * param handle The handle to set the attenuation from.
 * param value The new attenuation to set.
 */
extern AUD_API int AUD_Handle_setAttenuation(AUD_Handle* handle, float value);

/**
 * Retrieves the cone angle inner of a handle.
 * param handle The handle to get the cone angle inner from.
 * return The cone angle inner of the handle.
 */
extern AUD_API float AUD_Handle_getConeAngleInner(AUD_Handle* handle);

/**
 * Sets the cone angle inner of a handle.
 * param handle The handle to set the cone angle inner from.
 * param value The new cone angle inner to set.
 */
extern AUD_API int AUD_Handle_setConeAngleInner(AUD_Handle* handle, float value);

/**
 * Retrieves the cone angle outer of a handle.
 * param handle The handle to get the cone angle outer from.
 * return The cone angle outer of the handle.
 */
extern AUD_API float AUD_Handle_getConeAngleOuter(AUD_Handle* handle);

/**
 * Sets the cone angle outer of a handle.
 * param handle The handle to set the cone angle outer from.
 * param value The new cone angle outer to set.
 */
extern AUD_API int AUD_Handle_setConeAngleOuter(AUD_Handle* handle, float value);

/**
 * Retrieves the cone volume outer of a handle.
 * param handle The handle to get the cone volume outer from.
 * return The cone volume outer of the handle.
 */
extern AUD_API float AUD_Handle_getConeVolumeOuter(AUD_Handle* handle);

/**
 * Sets the cone volume outer of a handle.
 * param handle The handle to set the cone volume outer from.
 * param value The new cone volume outer to set.
 */
extern AUD_API int AUD_Handle_setConeVolumeOuter(AUD_Handle* handle, float value);

/**
 * Retrieves the distance maximum of a handle.
 * param handle The handle to get the distance maximum from.
 * return The distance maximum of the handle.
 */
extern AUD_API float AUD_Handle_getDistanceMaximum(AUD_Handle* handle);

/**
 * Sets the distance maximum of a handle.
 * param handle The handle to set the distance maximum from.
 * param value The new distance maximum to set.
 */
extern AUD_API int AUD_Handle_setDistanceMaximum(AUD_Handle* handle, float value);

/**
 * Retrieves the distance reference of a handle.
 * param handle The handle to get the distance reference from.
 * return The distance reference of the handle.
 */
extern AUD_API float AUD_Handle_getDistanceReference(AUD_Handle* handle);

/**
 * Sets the distance reference of a handle.
 * param handle The handle to set the distance reference from.
 * param value The new distance reference to set.
 */
extern AUD_API int AUD_Handle_setDistanceReference(AUD_Handle* handle, float value);

/**
 * Retrieves the keep of a handle.
 * param handle The handle to get the keep from.
 * return The keep of the handle.
 */
extern AUD_API int AUD_Handle_doesKeep(AUD_Handle* handle);

/**
 * Sets the keep of a handle.
 * param handle The handle to set the keep from.
 * param value The new keep to set.
 */
extern AUD_API int AUD_Handle_setKeep(AUD_Handle* handle, int value);

/**
 * Retrieves the location of a handle.
 * param handle The handle to get the location from.
 * return The location of the handle.
 */
extern AUD_API int AUD_Handle_getLocation(AUD_Handle* handle, float value[3]);

/**
 * Sets the location of a handle.
 * param handle The handle to set the location from.
 * param value The new location to set.
 */
extern AUD_API int AUD_Handle_setLocation(AUD_Handle* handle, const float value[3]);

/**
 * Retrieves the loop count of a handle.
 * param handle The handle to get the loop count from.
 * return The loop count of the handle.
 */
extern AUD_API int AUD_Handle_getLoopCount(AUD_Handle* handle);

/**
 * Sets the loop count of a handle.
 * param handle The handle to set the loop count from.
 * param value The new loop count to set.
 */
extern AUD_API int AUD_Handle_setLoopCount(AUD_Handle* handle, int value);

/**
 * Retrieves the orientation of a handle.
 * param handle The handle to get the orientation from.
 * return The orientation of the handle.
 */
extern AUD_API int AUD_Handle_getOrientation(AUD_Handle* handle, float value[4]);

/**
 * Sets the orientation of a handle.
 * param handle The handle to set the orientation from.
 * param value The new orientation to set.
 */
extern AUD_API int AUD_Handle_setOrientation(AUD_Handle* handle, const float value[4]);

/**
 * Retrieves the pitch of a handle.
 * param handle The handle to get the pitch from.
 * return The pitch of the handle.
 */
extern AUD_API float AUD_Handle_getPitch(AUD_Handle* handle);

/**
 * Sets the pitch of a handle.
 * param handle The handle to set the pitch from.
 * param value The new pitch to set.
 */
extern AUD_API int AUD_Handle_setPitch(AUD_Handle* handle, float value);

/**
 * Retrieves the position of a handle.
 * param handle The handle to get the position from.
 * return The position of the handle.
 */
extern AUD_API double AUD_Handle_getPosition(AUD_Handle* handle);

/**
 * Sets the position of a handle.
 * param handle The handle to set the position from.
 * param value The new position to set.
 */
extern AUD_API int AUD_Handle_setPosition(AUD_Handle* handle, double value);

/**
 * Retrieves the relative of a handle.
 * param handle The handle to get the relative from.
 * return The relative of the handle.
 */
extern AUD_API int AUD_Handle_isRelative(AUD_Handle* handle);

/**
 * Sets the relative of a handle.
 * param handle The handle to set the relative from.
 * param value The new relative to set.
 */
extern AUD_API int AUD_Handle_setRelative(AUD_Handle* handle, int value);

/**
 * Retrieves the status of a handle.
 * param handle The handle to get the status from.
 * return The status of the handle.
 */
extern AUD_API AUD_Status AUD_Handle_getStatus(AUD_Handle* handle);

/**
 * Retrieves the velocity of a handle.
 * param handle The handle to get the velocity from.
 * return The velocity of the handle.
 */
extern AUD_API int AUD_Handle_getVelocity(AUD_Handle* handle, float value[3]);

/**
 * Sets the velocity of a handle.
 * param handle The handle to set the velocity from.
 * param value The new velocity to set.
 */
extern AUD_API int AUD_Handle_setVelocity(AUD_Handle* handle, const float value[3]);

/**
 * Retrieves the volume of a handle.
 * param handle The handle to get the volume from.
 * return The volume of the handle.
 */
extern AUD_API float AUD_Handle_getVolume(AUD_Handle* handle);

/**
 * Sets the volume of a handle.
 * param handle The handle to set the volume from.
 * param value The new volume to set.
 */
extern AUD_API int AUD_Handle_setVolume(AUD_Handle* handle, float value);

/**
 * Retrieves the volume maximum of a handle.
 * param handle The handle to get the volume maximum from.
 * return The volume maximum of the handle.
 */
extern AUD_API float AUD_Handle_getVolumeMaximum(AUD_Handle* handle);

/**
 * Sets the volume maximum of a handle.
 * param handle The handle to set the volume maximum from.
 * param value The new volume maximum to set.
 */
extern AUD_API int AUD_Handle_setVolumeMaximum(AUD_Handle* handle, float value);

/**
 * Retrieves the volume minimum of a handle.
 * param handle The handle to get the volume minimum from.
 * return The volume minimum of the handle.
 */
extern AUD_API float AUD_Handle_getVolumeMinimum(AUD_Handle* handle);

/**
 * Sets the volume minimum of a handle.
 * param handle The handle to set the volume minimum from.
 * param value The new volume minimum to set.
 */
extern AUD_API int AUD_Handle_setVolumeMinimum(AUD_Handle* handle, float value);

/**
 * Frees a handle.
 * \param channel Handle to free.
 */
extern AUD_API void AUD_Handle_free(AUD_Handle* channel);

#ifdef __cplusplus
}
#endif
