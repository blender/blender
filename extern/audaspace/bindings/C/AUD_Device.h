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

/// Possible distance models for the 3D device.
typedef enum
{
	AUD_DISTANCE_MODEL_INVALID = 0,
	AUD_DISTANCE_MODEL_INVERSE,
	AUD_DISTANCE_MODEL_INVERSE_CLAMPED,
	AUD_DISTANCE_MODEL_LINEAR,
	AUD_DISTANCE_MODEL_LINEAR_CLAMPED,
	AUD_DISTANCE_MODEL_EXPONENT,
	AUD_DISTANCE_MODEL_EXPONENT_CLAMPED
} AUD_DistanceModel;

typedef void (*AUD_syncFunction)(void*, int, float);

/**
 * Opens a new sound device.
 * \param type       The name of the device.
 *                   Can be NULL to open the default device with default settings or return the handle to the already opened one.
 *                   Can be "" to open the a default factory device with given settings.
 *                   Can be "read" to open a readable device.
 * \param specs      Specification of the device parameters.
 * \param buffersize Size of the mixing buffer.
 * \param name       Custom name of the device.
 * \return A handle to the opened device or NULL on failure.
 */
extern AUD_API AUD_Device* AUD_Device_open(const char* type, AUD_DeviceSpecs specs, int buffersize, const char* name);

/**
 * Locks the playback device.
 */
extern AUD_API void AUD_Device_lock(AUD_Device* device);

/**
 * Plays back a sound file.
 * \param sound The handle of the sound file.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return A handle to the played back sound.
 */
extern AUD_API AUD_Handle* AUD_Device_play(AUD_Device* device, AUD_Sound* sound, int keep);

/**
 * Stops all sounds playing.
 */
extern AUD_API void AUD_Device_stopAll(AUD_Device* device);

/**
 * Unlocks the device.
 */
extern AUD_API void AUD_Device_unlock(AUD_Device* device);

/**
 * Retrieves the channels of a device.
 * param device The device to get the channels from.
 * return The channels of the device.
 */
extern AUD_API AUD_Channels AUD_Device_getChannels(AUD_Device* device);

/**
 * Retrieves the distance model of a device.
 * param device The device to get the distance model from.
 * return The distance model of the device.
 */
extern AUD_API AUD_DistanceModel AUD_Device_getDistanceModel(AUD_Device* device);

/**
 * Sets the distance model of a device.
 * param device The device to set the distance model from.
 * param value The new distance model to set.
 */
extern AUD_API void AUD_Device_setDistanceModel(AUD_Device* device, AUD_DistanceModel value);

/**
 * Retrieves the doppler factor of a device.
 * param device The device to get the doppler factor from.
 * return The doppler factor of the device.
 */
extern AUD_API float AUD_Device_getDopplerFactor(AUD_Device* device);

/**
 * Sets the doppler factor of a device.
 * param device The device to set the doppler factor from.
 * param value The new doppler factor to set.
 */
extern AUD_API void AUD_Device_setDopplerFactor(AUD_Device* device, float value);

/**
 * Retrieves the format of a device.
 * param device The device to get the format from.
 * return The format of the device.
 */
extern AUD_API AUD_SampleFormat AUD_Device_getFormat(AUD_Device* device);

/**
 * Retrieves the listener location of a device.
 * param device The device to get the listener location from.
 * return The listener location of the device.
 */
extern AUD_API void AUD_Device_getListenerLocation(AUD_Device* device, float value[3]);

/**
 * Sets the listener location of a device.
 * param device The device to set the listener location from.
 * param value The new listener location to set.
 */
extern AUD_API void AUD_Device_setListenerLocation(AUD_Device* device, const float value[3]);

/**
 * Retrieves the listener orientation of a device.
 * param device The device to get the listener orientation from.
 * return The listener orientation of the device.
 */
extern AUD_API void AUD_Device_getListenerOrientation(AUD_Device* device, float value[4]);

/**
 * Sets the listener orientation of a device.
 * param device The device to set the listener orientation from.
 * param value The new listener orientation to set.
 */
extern AUD_API void AUD_Device_setListenerOrientation(AUD_Device* device, const float value[4]);

/**
 * Retrieves the listener velocity of a device.
 * param device The device to get the listener velocity from.
 * return The listener velocity of the device.
 */
extern AUD_API void AUD_Device_getListenerVelocity(AUD_Device* device, float value[3]);

/**
 * Sets the listener velocity of a device.
 * param device The device to set the listener velocity from.
 * param value The new listener velocity to set.
 */
extern AUD_API void AUD_Device_setListenerVelocity(AUD_Device* device, const float value[3]);

/**
 * Retrieves the rate of a device.
 * param device The device to get the rate from.
 * return The rate of the device.
 */
extern AUD_API double AUD_Device_getRate(AUD_Device* device);

/**
 * Retrieves the speed of sound of a device.
 * param device The device to get the speed of sound from.
 * return The speed of sound of the device.
 */
extern AUD_API float AUD_Device_getSpeedOfSound(AUD_Device* device);

/**
 * Sets the speed of sound of a device.
 * param device The device to set the speed of sound from.
 * param value The new speed of sound to set.
 */
extern AUD_API void AUD_Device_setSpeedOfSound(AUD_Device* device, float value);

/**
 * Retrieves the volume of a device.
 * param device The device to get the volume from.
 * return The volume of the device.
 */
extern AUD_API float AUD_Device_getVolume(AUD_Device* device);

/**
 * Sets the volume of a device.
 * param device The device to set the volume from.
 * param value The new volume to set.
 */
extern AUD_API void AUD_Device_setVolume(AUD_Device* device, float value);

/**
 * Reads the next samples into the supplied buffer.
 * \param device The readable device.
 * \param buffer The target buffer.
 * \param length The length in samples to be filled.
 * \return True if the reading succeeded, false if there are no sounds
 *         played back currently, in that case the buffer is filled with
 *         silence.
 */
extern AUD_API int AUD_Device_read(AUD_Device* device, unsigned char* buffer, int length);

/**
 * Closes a device. Handle becomes invalid afterwards.
 * \param device The device to close.
 */
extern AUD_API void AUD_Device_free(AUD_Device* device);

/**
 * Retrieves the current device of the DeviceManager.
 * \return A pointer to the current device, which needs to be freed with
 *         AUD_Device_free.
 */
extern AUD_API AUD_Device* AUD_Device_getCurrent();

/**
 * Seeks sequenced sound scene playback.
 * \param handle Playback handle.
 * \param time Time in seconds to seek to.
 */
extern AUD_API void AUD_seekSynchronizer(AUD_Handle* handle, double time);

/**
 * Returns the current sound scene playback time.
 * \param handle Playback handle.
 * \return The playback time in seconds.
 */
extern AUD_API double AUD_getSynchronizerPosition(AUD_Handle* handle);

/**
 * Starts the playback of jack transport if possible.
 */
extern AUD_API void AUD_playSynchronizer();

/**
 * Stops the playback of jack transport if possible.
 */
extern AUD_API void AUD_stopSynchronizer();

/**
 * Sets the sync callback for jack transport.
 * \param function The callback function.
 * \param data The data parameter for the callback.
 */
extern AUD_API void AUD_setSynchronizerCallback(AUD_syncFunction function, void* data);

/**
 * Returns whether jack transport is currently playing.
 * \return Whether jack transport is currently playing.
 */
extern AUD_API int AUD_isSynchronizerPlaying();

#ifdef __cplusplus
}
#endif
