/*
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
 
#ifndef __AUD_C_API_H__
#define __AUD_C_API_H__

#ifdef WITH_PYTHON
#include "Python.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "AUD_Space.h"

/// Supported output devices.
typedef enum
{
	AUD_NULL_DEVICE = 0,
	AUD_SDL_DEVICE,
	AUD_OPENAL_DEVICE,
	AUD_JACK_DEVICE
} AUD_DeviceType;

/// Sound information structure.
typedef struct
{
	AUD_Specs specs;
	float length;
} AUD_SoundInfo;

#ifndef AUD_CAPI_IMPLEMENTATION
	typedef void AUD_Sound;
	typedef void AUD_Handle;
	typedef void AUD_Device;
	typedef void AUD_SEntry;
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
 * Rechannels the sound to be mono.
 * \param sound The sound to rechannel.
 * \return The mono sound.
 */
extern AUD_Sound* AUD_monoSound(AUD_Sound* sound);

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
extern int AUD_setLoop(AUD_Handle* handle, int loops);

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
 * \param handle The handle to the sound.
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
extern int AUD_setSourceLocation(AUD_Handle* handle, const float* location);

/**
 * Sets the velocity of a source.
 * \param handle The handle of the source.
 * \param velocity The new velocity.
 * \return Whether the action succeeded.
 */
extern int AUD_setSourceVelocity(AUD_Handle* handle, const float* velocity);

/**
 * Sets the orientation of a source.
 * \param handle The handle of the source.
 * \param orientation The new orientation as quaternion.
 * \return Whether the action succeeded.
 */
extern int AUD_setSourceOrientation(AUD_Handle* handle, const float* orientation);

/**
 * Sets whether the source location, velocity and orientation are relative
 * to the listener.
 * \param handle The handle of the source.
 * \param relative Whether the source is relative.
 * \return Whether the action succeeded.
 */
extern int AUD_setRelative(AUD_Handle* handle, int relative);

/**
 * Sets the maximum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new maximum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_setVolumeMaximum(AUD_Handle* handle, float volume);

/**
 * Sets the minimum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new minimum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_setVolumeMinimum(AUD_Handle* handle, float volume);

/**
 * Sets the maximum distance of a source.
 * If a source is further away from the reader than this distance, the
 * volume will automatically be set to 0.
 * \param handle The handle of the source.
 * \param distance The new maximum distance.
 * \return Whether the action succeeded.
 */
extern int AUD_setDistanceMaximum(AUD_Handle* handle, float distance);

/**
 * Sets the reference distance of a source.
 * \param handle The handle of the source.
 * \param distance The new reference distance.
 * \return Whether the action succeeded.
 */
extern int AUD_setDistanceReference(AUD_Handle* handle, float distance);

/**
 * Sets the attenuation of a source.
 * This value is used for distance calculation.
 * \param handle The handle of the source.
 * \param factor The new attenuation.
 * \return Whether the action succeeded.
 */
extern int AUD_setAttenuation(AUD_Handle* handle, float factor);

/**
 * Sets the outer angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new outer angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeAngleOuter(AUD_Handle* handle, float angle);

/**
 * Sets the inner angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new inner angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeAngleInner(AUD_Handle* handle, float angle);

/**
 * Sets the outer volume of the cone of a source.
 * The volume between inner and outer angle is interpolated between inner
 * volume and this value.
 * \param handle The handle of the source.
 * \param volume The new outer volume of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_setConeVolumeOuter(AUD_Handle* handle, float volume);

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
extern AUD_Handle* AUD_playDevice(AUD_Device* device, AUD_Sound* sound, float seek);

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
								  float sthreshold, double samplerate,
								  int* length);

/**
 * Pauses a playing sound after a specific amount of time.
 * \param handle The handle to the sound.
 * \param seconds The time in seconds.
 * \return The silence handle.
 */
extern AUD_Handle* AUD_pauseAfter(AUD_Handle* handle, float seconds);

/**
 * Creates a new sequenced sound scene.
 * \param fps The FPS of the scene.
 * \param muted Whether the scene is muted.
 * \return The new sound scene.
 */
extern AUD_Sound* AUD_createSequencer(float fps, int muted);

/**
 * Deletes a sound scene.
 * \param sequencer The sound scene.
 */
extern void AUD_destroySequencer(AUD_Sound* sequencer);

/**
 * Sets the muting state of the scene.
 * \param sequencer The sound scene.
 * \param muted Whether the scene is muted.
 */
extern void AUD_setSequencerMuted(AUD_Sound* sequencer, int muted);

/**
 * Sets the scene's FPS.
 * \param sequencer The sound scene.
 * \param fps The new FPS.
 */
extern void AUD_setSequencerFPS(AUD_Sound* sequencer, float fps);

/**
 * Adds a new entry to the scene.
 * \param sequencer The sound scene.
 * \param sound The sound this entry should play.
 * \param begin The start time.
 * \param end The end time or a negative value if determined by the sound.
 * \param skip How much seconds should be skipped at the beginning.
 * \return The entry added.
 */
extern AUD_SEntry* AUD_addSequence(AUD_Sound* sequencer, AUD_Sound* sound,
								   float begin, float end, float skip);

/**
 * Removes an entry from the scene.
 * \param sequencer The sound scene.
 * \param entry The entry to remove.
 */
extern void AUD_removeSequence(AUD_Sound* sequencer, AUD_SEntry* entry);

/**
 * Moves the entry.
 * \param entry The sequenced entry.
 * \param begin The new start time.
 * \param end The new end time or a negative value if unknown.
 * \param skip How many seconds to skip at the beginning.
 */
extern void AUD_moveSequence(AUD_SEntry* entry, float begin, float end, float skip);

/**
 * Sets the muting state of the entry.
 * \param entry The sequenced entry.
 * \param mute Whether the entry should be muted or not.
 */
extern void AUD_muteSequence(AUD_SEntry* entry, char mute);

/**
 * Sets whether the entrie's location, velocity and orientation are relative
 * to the listener.
 * \param entry The sequenced entry.
 * \param relative Whether the source is relative.
 * \return Whether the action succeeded.
 */
extern void AUD_setRelativeSequence(AUD_SEntry* entry, char relative);

/**
 * Sets the sound of the entry.
 * \param entry The sequenced entry.
 * \param sound The new sound.
 */
extern void AUD_updateSequenceSound(AUD_SEntry* entry, AUD_Sound* sound);

/**
 * Writes animation data to a sequenced entry.
 * \param entry The sequenced entry.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern void AUD_setSequenceAnimData(AUD_SEntry* entry, AUD_AnimateablePropertyType type, int frame, float* data, char animated);

/**
 * Writes animation data to a sequenced entry.
 * \param sequencer The sound scene.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern void AUD_setSequencerAnimData(AUD_Sound* sequencer, AUD_AnimateablePropertyType type, int frame, float* data, char animated);

/**
 * Updates all non-animated parameters of the entry.
 * \param entry The sequenced entry.
 * \param volume_max The maximum volume.
 * \param volume_min The minimum volume.
 * \param distance_max The maximum distance.
 * \param distance_reference The reference distance.
 * \param attenuation The attenuation.
 * \param cone_angle_outer The outer cone opening angle.
 * \param cone_angle_inner The inner cone opening angle.
 * \param cone_volume_outer The volume outside the outer cone.
 */
extern void AUD_updateSequenceData(AUD_SEntry* entry, float volume_max, float volume_min,
								   float distance_max, float distance_reference, float attenuation,
								   float cone_angle_outer, float cone_angle_inner, float cone_volume_outer);

/**
 * Updates all non-animated parameters of the entry.
 * \param sequencer The sound scene.
 * \param speed_of_sound The speed of sound for doppler calculation.
 * \param factor The doppler factor to control the effect's strength.
 * \param model The distance model for distance calculation.
 */
extern void AUD_updateSequencerData(AUD_Sound* sequencer, float speed_of_sound,
									float factor, AUD_DistanceModel model);

/**
 * Sets the audio output specification of the sound scene to the specs of the
 * current playback device.
 * \param sequencer The sound scene.
 */
extern void AUD_setSequencerDeviceSpecs(AUD_Sound* sequencer);

/**
 * Sets the audio output specification of the sound scene.
 * \param sequencer The sound scene.
 * \param specs The new specification.
 */
extern void AUD_setSequencerSpecs(AUD_Sound* sequencer, AUD_Specs specs);

/**
 * Seeks sequenced sound scene playback.
 * \param handle Playback handle.
 * \param time Time in seconds to seek to.
 */
extern void AUD_seekSequencer(AUD_Handle* handle, float time);

/**
 * Returns the current sound scene playback time.
 * \param handle Playback handle.
 * \return The playback time in seconds.
 */
extern float AUD_getSequencerPosition(AUD_Handle* handle);

/**
 * Starts the playback of jack transport if possible.
 */
extern void AUD_startPlayback(void);

/**
 * Stops the playback of jack transport if possible.
 */
extern void AUD_stopPlayback(void);

#ifdef WITH_JACK
/**
 * Sets the sync callback for jack transport.
 * \param function The callback function.
 * \param data The data parameter for the callback.
 */
extern void AUD_setSyncCallback(AUD_syncFunction function, void* data);
#endif

/**
 * Returns whether jack transport is currently playing.
 * \return Whether jack transport is currently playing.
 */
extern int AUD_doesPlayback(void);

/**
 * Reads a sound into a buffer for drawing at a specific sampling rate.
 * \param sound The sound to read.
 * \param buffer The buffer to write to. Must have a size of 3*4*length.
 * \param length How many samples to read from the sound.
 * \param samples_per_second How many samples to read per second of the sound.
 * \return How many samples really have been read. Always <= length.
 */
extern int AUD_readSound(AUD_Sound* sound, sample_t* buffer, int length, int samples_per_second);

/**
 * Copies a sound.
 * \param sound Sound to copy.
 * \return Copied sound.
 */
extern AUD_Sound* AUD_copy(AUD_Sound* sound);

/**
 * Frees a handle.
 * \param channel Handle to free.
 */
extern void AUD_freeHandle(AUD_Handle* channel);

/**
 * Creates a new set.
 * \return The new set.
 */
extern void* AUD_createSet(void);

/**
 * Deletes a set.
 * \param set The set to delete.
 */
extern void AUD_destroySet(void* set);

/**
 * Removes an entry from a set.
 * \param set The set work on.
 * \param entry The entry to remove.
 * \return Whether the entry was in the set or not.
 */
extern char AUD_removeSet(void* set, void* entry);

/**
 * Adds a new entry to a set.
 * \param set The set work on.
 * \param entry The entry to add.
 */
extern void AUD_addSet(void* set, void* entry);

/**
 * Removes one entry from a set and returns it.
 * \param set The set work on.
 * \return The entry or NULL if the set is empty.
 */
extern void* AUD_getSet(void* set);

/**
 * Mixes a sound down into a file.
 * \param sound The sound scene to mix down.
 * \param start The start frame.
 * \param length The count of frames to write.
 * \param buffersize How many samples should be written at once.
 * \param filename The file to write to.
 * \param specs The file's audio specification.
 * \param format The file's container format.
 * \param codec The codec used for encoding the audio data.
 * \param bitrate The bitrate for encoding.
 * \return An error message or NULL in case of success.
 */
extern const char* AUD_mixdown(AUD_Sound* sound, unsigned int start, unsigned int length, unsigned int buffersize, const char* filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate);

/**
 * Mixes a sound down into multiple files.
 * \param sound The sound scene to mix down.
 * \param start The start frame.
 * \param length The count of frames to write.
 * \param buffersize How many samples should be written at once.
 * \param filename The file to write to, the channel number and an underscore are added at the beginning.
 * \param specs The file's audio specification.
 * \param format The file's container format.
 * \param codec The codec used for encoding the audio data.
 * \param bitrate The bitrate for encoding.
 * \return An error message or NULL in case of success.
 */
extern const char* AUD_mixdown_per_channel(AUD_Sound* sound, unsigned int start, unsigned int length, unsigned int buffersize, const char* filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate);

/**
 * Opens a read device and prepares it for mixdown of the sound scene.
 * \param specs Output audio specifications.
 * \param sequencer The sound scene to mix down.
 * \param volume The overall mixdown volume.
 * \param start The start time of the mixdown in the sound scene.
 * \return The read device for the mixdown.
 */
extern AUD_Device* AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound* sequencer, float volume, float start);

#ifdef WITH_PYTHON
/**
 * Retrieves the python factory of a sound.
 * \param sound The sound factory.
 * \return The python factory.
 */
extern PyObject* AUD_getPythonFactory(AUD_Sound* sound);

/**
 * Retrieves the sound factory of a python factory.
 * \param sound The python factory.
 * \return The sound factory.
 */
extern AUD_Sound* AUD_getPythonSound(PyObject* sound);
#endif

#ifdef __cplusplus
}

#include "AUD_Reference.h"
class AUD_IDevice;
class AUD_I3DDevice;

/**
 * Returns the current playback device.
 * \return The playback device.
 */
AUD_Reference<AUD_IDevice> AUD_getDevice();

/**
 * Returns the current playback 3D device.
 * \return The playback 3D device.
 */
AUD_I3DDevice* AUD_get3DDevice();
#endif

#endif //__AUD_C_API_H__
