/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_CAPI
#define AUD_CAPI

#ifdef __cplusplus
extern "C" {
#endif

#include "AUD_Space.h"

typedef enum
{
	AUD_NULL_DEVICE = 0,
	AUD_SDL_DEVICE,
	AUD_OPENAL_DEVICE,
	AUD_JACK_DEVICE
} AUD_DeviceType;

typedef struct
{
	AUD_Specs specs;
	float length;
} AUD_SoundInfo;

#ifndef AUD_CAPI_IMPLEMENTATION
	typedef void AUD_Sound;
	typedef void AUD_Handle;
	typedef void AUD_Device;
#endif

/**
 * Initializes an audio device.
 * \param device The device type that should be used.
 * \param specs The audio specification to be used.
 * \param buffersize The buffersize for the device.
 * \return Whether the device has been initialized.
 */
extern int AUD_init(AUD_DeviceType device, AUD_Specs specs, int buffersize);

/**
 * Returns a integer list with available sound devices. The last one is always
 * AUD_NULL_DEVICE.
 */
extern int* AUD_enumDevices();

/**
 * Unitinitializes an audio device.
 */
extern void AUD_exit();

/**
 * Locks the playback device.
 */
extern void AUD_lock();

/**
 * Unlocks the device.
 */
extern void AUD_unlock();

/**
 * Returns information about a sound.
 * \param sound The sound to get the info about.
 * \return The AUD_SoundInfo structure with filled in data.
 */
extern AUD_SoundInfo AUD_getInfo(AUD_Sound* sound);

/**
 * Loads a sound file.
 * \param filename The filename of the sound file.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_load(const char* filename);

/**
 * Loads a sound file.
 * \param buffer The buffer which contains the sound file.
 * \param size The size of the buffer.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_loadBuffer(unsigned char* buffer, int size);

/**
 * Buffers a sound.
 * \param sound The sound to buffer.
 * \return A handle of the sound buffer.
 */
extern AUD_Sound* AUD_bufferSound(AUD_Sound* sound);

/**
 * Delays a sound.
 * \param sound The sound to dealy.
 * \param delay The delay in seconds.
 * \return A handle of the delayed sound.
 */
extern AUD_Sound* AUD_delaySound(AUD_Sound* sound, float delay);

/**
 * Limits a sound.
 * \param sound The sound to limit.
 * \param start The start time in seconds.
 * \param end The stop time in seconds.
 * \return A handle of the limited sound.
 */
extern AUD_Sound* AUD_limitSound(AUD_Sound* sound, float start, float end);

/**
 * Ping pongs a sound.
 * \param sound The sound to ping pong.
 * \return A handle of the ping pong sound.
 */
extern AUD_Sound* AUD_pingpongSound(AUD_Sound* sound);

/**
 * Loops a sound.
 * \param sound The sound to loop.
 * \return A handle of the looped sound.
 */
extern AUD_Sound* AUD_loopSound(AUD_Sound* sound);

/**
 * Stops a looping sound when the current playback finishes.
 * \param handle The playback handle.
 * \return Whether the handle is valid.
 */
extern int AUD_stopLoop(AUD_Handle* handle);

/**
 * Unloads a sound of any type.
 * \param sound The handle of the sound.
 */
extern void AUD_unload(AUD_Sound* sound);

/**
 * Plays back a sound file.
 * \param sound The handle of the sound file.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return A handle to the played back sound.
 */
extern AUD_Handle* AUD_play(AUD_Sound* sound, int keep);

/**
 * Pauses a played back sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been playing or not.
 */
extern int AUD_pause(AUD_Handle* handle);

/**
 * Resumes a paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been paused or not.
 */
extern int AUD_resume(AUD_Handle* handle);

/**
 * Stops a playing or paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_stop(AUD_Handle* handle);

/**
 * Sets the end behaviour of a playing or paused sound.
 * \param handle The handle to the sound.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_setKeep(AUD_Handle* handle, int keep);

/**
 * Seeks a playing or paused sound.
 * \param handle The handle to the sound.
 * \param seekTo From where the sound file should be played back in seconds.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_seek(AUD_Handle* handle, float seekTo);

/**
 * Retrieves the playback position of a handle.
 * \return The current playback position in seconds or 0.0 if the handle is
 *         invalid.
 */
extern float AUD_getPosition(AUD_Handle* handle);

/**
 * Returns the status of a playing, paused or stopped sound.
 * \param handle The handle to the sound.
 * \return The status of the sound behind the handle.
 */
extern AUD_Status AUD_getStatus(AUD_Handle* handle);

/**
 * Plays a 3D sound.
 * \param sound The handle of the sound file.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return A handle to the played back sound.
 * \note The factory must provide a mono (single channel) source and the device
 *       must support 3D audio, otherwise the sound is played back normally.
 */
extern AUD_Handle* AUD_play3D(AUD_Sound* sound, int keep);

/**
 * Updates the listener 3D data.
 * \param data The 3D data.
 * \return Whether the action succeeded.
 */
extern int AUD_updateListener(AUD_3DData* data);

/**
 * Sets a 3D device setting.
 * \param setting The setting type.
 * \param value The new setting value.
 * \return Whether the action succeeded.
 */
extern int AUD_set3DSetting(AUD_3DSetting setting, float value);

/**
 * Retrieves a 3D device setting.
 * \param setting The setting type.
 * \return The setting value.
 */
extern float AUD_get3DSetting(AUD_3DSetting setting);

/**
 * Updates a listeners 3D data.
 * \param handle The source handle.
 * \param data The 3D data.
 * \return Whether the action succeeded.
 */
extern int AUD_update3DSource(AUD_Handle* handle, AUD_3DData* data);

/**
 * Sets a 3D source setting.
 * \param handle The source handle.
 * \param setting The setting type.
 * \param value The new setting value.
 * \return Whether the action succeeded.
 */
extern int AUD_set3DSourceSetting(AUD_Handle* handle,
								  AUD_3DSourceSetting setting, float value);

/**
 * Retrieves a 3D source setting.
 * \param handle The source handle.
 * \param setting The setting type.
 * \return The setting value.
 */
extern float AUD_get3DSourceSetting(AUD_Handle* handle,
									AUD_3DSourceSetting setting);

/**
 * Sets the volume of a played back sound.
 * \param handle The handle to the sound.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_setSoundVolume(AUD_Handle* handle, float volume);

/**
 * Sets the pitch of a played back sound.
 * \param handle The handle to the sound.
 * \param pitch The new pitch.
 * \return Whether the action succeeded.
 */
extern int AUD_setSoundPitch(AUD_Handle* handle, float pitch);

/**
 * Opens a read device, with which audio data can be read.
 * \param specs The specification of the audio data.
 * \return A device handle.
 */
extern AUD_Device* AUD_openReadDevice(AUD_Specs specs);

/**
 * Plays back a sound file through a read device.
 * \param device The read device.
 * \param sound The handle of the sound file.
 * \return Whether the sound could be played back.
 */
extern int AUD_playDevice(AUD_Device* device, AUD_Sound* sound);

/**
 * Reads the next samples into the supplied buffer.
 * \param device The read device.
 * \param buffer The target buffer.
 * \param length The length in samples to be filled.
 * \return True if the reading succeeded, false if there are no sounds
 *         played back currently, in that case the buffer is filled with
 *         silence.
 */
extern int AUD_readDevice(AUD_Device* device, sample_t* buffer, int length);

/**
 * Closes a read device.
 * \param device The read device.
 */
extern void AUD_closeReadDevice(AUD_Device* device);

#ifdef __cplusplus
}
#endif

#endif //AUD_CAPI
