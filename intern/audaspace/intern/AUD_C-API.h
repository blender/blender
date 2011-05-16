/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file AUD_C-API.h
 *  \ingroup audaspace
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
	typedef void AUD_Channel;
	typedef void AUD_Device;
	typedef void AUD_SequencerEntry;
	typedef float (*AUD_volumeFunction)(void*, void*, float);
	typedef void (*AUD_syncFunction)(void*, int, float);
#endif

/**
 * Initializes FFMPEG if it is enabled.
 */
extern void AUD_initOnce(void);

/**
 * Initializes an audio device.
 * \param device The device type that should be used.
 * \param specs The audio specification to be used.
 * \param buffersize The buffersize for the device.
 * \return Whether the device has been initialized.
 */
extern int AUD_init(AUD_DeviceType device, AUD_DeviceSpecs specs, int buffersize);

/**
 * Unitinitializes an audio device.
 */
extern void AUD_exit(void);

/**
 * Locks the playback device.
 */
extern void AUD_lock(void);

/**
 * Unlocks the device.
 */
extern void AUD_unlock(void);

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
 * Sets a remaining loop count of a looping sound that currently plays.
 * \param handle The playback handle.
 * \param loops The count of remaining loops, -1 for infinity.
 * \return Whether the handle is valid.
 */
extern int AUD_setLoop(AUD_Channel* handle, int loops);

/**
 * Rectifies a sound.
 * \param sound The sound to rectify.
 * \return A handle of the rectified sound.
 */
extern AUD_Sound* AUD_rectifySound(AUD_Sound* sound);

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
extern AUD_Channel* AUD_play(AUD_Sound* sound, int keep);

/**
 * Pauses a played back sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been playing or not.
 */
extern int AUD_pause(AUD_Channel* handle);

/**
 * Resumes a paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been paused or not.
 */
extern int AUD_resume(AUD_Channel* handle);

/**
 * Stops a playing or paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_stop(AUD_Channel* handle);

/**
 * Sets the end behaviour of a playing or paused sound.
 * \param handle The handle to the sound.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_setKeep(AUD_Channel* handle, int keep);

/**
 * Seeks a playing or paused sound.
 * \param handle The handle to the sound.
 * \param seekTo From where the sound file should be played back in seconds.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_seek(AUD_Channel* handle, float seekTo);

/**
 * Retrieves the playback position of a handle.
 * \param handle The handle to the sound.
 * \return The current playback position in seconds or 0.0 if the handle is
 *         invalid.
 */
extern float AUD_getPosition(AUD_Channel* handle);

/**
 * Returns the status of a playing, paused or stopped sound.
 * \param handle The handle to the sound.
 * \return The status of the sound behind the handle.
 */
extern AUD_Status AUD_getStatus(AUD_Channel* handle);

/**
 * Sets the listener location.
 * \param location The new location.
 */
extern int AUD_setListenerLocation(const float* location);

/**
 * Sets the listener velocity.
 * \param velocity The new velocity.
 */
extern int AUD_setListenerVelocity(const float* velocity);

/**
 * Sets the listener orientation.
 * \param orientation The new orientation as quaternion.
 */
extern int AUD_setListenerOrientation(const float* orientation);

/**
 * Sets the speed of sound.
 * This value is needed for doppler effect calculation.
 * \param speed The new speed of sound.
 */
extern int AUD_setSpeedOfSound(float speed);

/**
 * Sets the doppler factor.
 * This value is a scaling factor for the velocity vectors of sources and
 * listener which is used while calculating the doppler effect.
 * \param factor The new doppler factor.
 */
extern int AUD_setDopplerFactor(float factor);

/**
 * Sets the distance model.
 * \param model distance model.
 */
extern int AUD_setDistanceModel(AUD_DistanceModel model);

/**
 * Sets the location of a source.
 * \param handle The handle of the source.
 * \param location The new location.
 * \return Whether the action succeeded.
 */
extern int AUD_setSourceLocation(AUD_Channel* handle, const float* location);

/**
 * Sets the velocity of a source.
 * \param handle The handle of the source.
 * \param velocity The new velocity.
 * \return Whether the action succeeded.
 */
extern int AUD_setSourceVelocity(AUD_Channel* handle, const float* velocity);

/**
 * Sets the orientation of a source.
 * \param handle The handle of the source.
 * \param orientation The new orientation as quaternion.
 * \return Whether the action succeeded.
 */
extern int AUD_setSourceOrientation(AUD_Channel* handle, const float* orientation);

/**
 * Sets whether the source location, velocity and orientation are relative
 * to the listener.
 * \param handle The handle of the source.
 * \param relative Whether the source is relative.
 * \return Whether the action succeeded.
 */
extern int AUD_setRelative(AUD_Channel* handle, int relative);

/**
 * Sets the maximum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new maximum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_setVolumeMaximum(AUD_Channel* handle, float volume);

/**
 * Sets the minimum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new minimum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_setVolumeMinimum(AUD_Channel* handle, float volume);

/**
 * Sets the maximum distance of a source.
 * If a source is further away from the reader than this distance, the
 * volume will automatically be set to 0.
 * \param handle The handle of the source.
 * \param distance The new maximum distance.
 * \return Whether the action succeeded.
 */
extern int AUD_setDistanceMaximum(AUD_Channel* handle, float distance);

/**
 * Sets the reference distance of a source.
 * \param handle The handle of the source.
 * \param distance The new reference distance.
 * \return Whether the action succeeded.
 */
extern int AUD_setDistanceReference(AUD_Channel* handle, float distance);

/**
 * Sets the attenuation of a source.
 * This value is used for distance calculation.
 * \param handle The handle of the source.
 * \param factor The new attenuation.
 * \return Whether the action succeeded.
 */
extern int AUD_setAttenuation(AUD_Channel* handle, float factor);

/**
 * Sets the outer angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new outer angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeAngleOuter(AUD_Channel* handle, float angle);

/**
 * Sets the inner angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new inner angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeAngleInner(AUD_Channel* handle, float angle);

/**
 * Sets the outer volume of the cone of a source.
 * The volume between inner and outer angle is interpolated between inner
 * volume and this value.
 * \param handle The handle of the source.
 * \param volume The new outer volume of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeVolumeOuter(AUD_Channel* handle, float volume);

/**
 * Sets the volume of a played back sound.
 * \param handle The handle to the sound.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_setSoundVolume(AUD_Channel* handle, float volume);

/**
 * Sets the pitch of a played back sound.
 * \param handle The handle to the sound.
 * \param pitch The new pitch.
 * \return Whether the action succeeded.
 */
extern int AUD_setSoundPitch(AUD_Channel* handle, float pitch);

/**
 * Opens a read device, with which audio data can be read.
 * \param specs The specification of the audio data.
 * \return A device handle.
 */
extern AUD_Device* AUD_openReadDevice(AUD_DeviceSpecs specs);

/**
 * Sets the main volume of a device.
 * \param device The device.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_setDeviceVolume(AUD_Device* device, float volume);

/**
 * Plays back a sound file through a read device.
 * \param device The read device.
 * \param sound The handle of the sound file.
 * \param seek The position where the sound should be seeked to.
 * \return A handle to the played back sound.
 */
extern AUD_Channel* AUD_playDevice(AUD_Device* device, AUD_Sound* sound, float seek);

/**
 * Sets the volume of a played back sound of a read device.
 * \param device The read device.
 * \param handle The handle to the sound.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_setDeviceSoundVolume(AUD_Device* device,
									AUD_Channel* handle,
									float volume);

/**
 * Reads the next samples into the supplied buffer.
 * \param device The read device.
 * \param buffer The target buffer.
 * \param length The length in samples to be filled.
 * \return True if the reading succeeded, false if there are no sounds
 *         played back currently, in that case the buffer is filled with
 *         silence.
 */
extern int AUD_readDevice(AUD_Device* device, data_t* buffer, int length);

/**
 * Closes a read device.
 * \param device The read device.
 */
extern void AUD_closeReadDevice(AUD_Device* device);

/**
 * Reads a sound file into a newly created float buffer.
 * The sound is therefore bandpassed, rectified and resampled.
 */
extern float* AUD_readSoundBuffer(const char* filename, float low, float high,
								  float attack, float release, float threshold,
								  int accumulate, int additive, int square,
								  float sthreshold, int samplerate,
								  int* length);

/**
 * Pauses a playing sound after a specific amount of time.
 * \param handle The handle to the sound.
 * \param time The time in seconds.
 * \return The silence handle.
 */
extern AUD_Channel* AUD_pauseAfter(AUD_Channel* handle, float seconds);

extern AUD_Sound* AUD_createSequencer(int muted, void* data, AUD_volumeFunction volume);

extern void AUD_destroySequencer(AUD_Sound* sequencer);

extern void AUD_setSequencerMuted(AUD_Sound* sequencer, int muted);

extern AUD_SequencerEntry* AUD_addSequencer(AUD_Sound** sequencer, AUD_Sound* sound,
										float begin, float end, float skip, void* data);

extern void AUD_removeSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry);

extern void AUD_moveSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry,
						  float begin, float end, float skip);

extern void AUD_muteSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry,
						  char mute);

extern int AUD_readSound(AUD_Sound* sound, sample_t* buffer, int length);

extern void AUD_startPlayback(void);

extern void AUD_stopPlayback(void);

extern void AUD_seekSequencer(AUD_Channel* handle, float time);

extern float AUD_getSequencerPosition(AUD_Channel* handle);

#ifdef WITH_JACK
extern void AUD_setSyncCallback(AUD_syncFunction function, void* data);
#endif

extern int AUD_doesPlayback(void);

#ifdef __cplusplus
}
#endif

#endif //AUD_CAPI
