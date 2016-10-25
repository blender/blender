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
	typedef float (*AUD_volumeFunction)(void *, void *, float);
	typedef void (*AUD_syncFunction)(void *, int, float);
#endif

/**
 * Initializes audio routines (FFMPEG/JACK if it is enabled).
 */
extern void AUD_initOnce(void);

/**
 * Unitinitializes an audio routines.
 */
extern void AUD_exitOnce(void);

/**
 * Initializes an audio device.
 * \param device The device type that should be used.
 * \param specs The audio specification to be used.
 * \param buffersize The buffersize for the device.
 * \return Whether the device has been initialized.
 */
extern AUD_Device* AUD_init(const char* device, AUD_DeviceSpecs specs, int buffersize, const char* name);

/**
 * Unitinitializes an audio device.
 */
extern void AUD_exit(AUD_Device* device);

/**
 * Locks the playback device.
 */
extern void AUD_Device_lock(AUD_Device* device);

/**
 * Unlocks the device.
 */
extern void AUD_Device_unlock(AUD_Device* device);

extern AUD_Channels AUD_Device_getChannels(AUD_Device* device);

extern AUD_SampleRate AUD_Device_getRate(AUD_Device* device);

/**
 * Returns information about a sound.
 * \param sound The sound to get the info about.
 * \return The AUD_SoundInfo structure with filled in data.
 */
extern AUD_SoundInfo AUD_getInfo(AUD_Sound *sound);

/**
 * Loads a sound file.
 * \param filename The filename of the sound file.
 * \return A handle of the sound file.
 */
extern AUD_Sound *AUD_Sound_file(const char *filename);

/**
 * Loads a sound file.
 * \param buffer The buffer which contains the sound file.
 * \param size The size of the buffer.
 * \return A handle of the sound file.
 */
extern AUD_Sound *AUD_Sound_bufferFile(unsigned char *buffer, int size);

/**
 * Buffers a sound.
 * \param sound The sound to buffer.
 * \return A handle of the sound buffer.
 */
extern AUD_Sound *AUD_Sound_cache(AUD_Sound *sound);

/**
 * Rechannels the sound to be mono.
 * \param sound The sound to rechannel.
 * \return The mono sound.
 */
extern AUD_Sound *AUD_Sound_rechannel(AUD_Sound *sound, AUD_Channels channels);

/**
 * Delays a sound.
 * \param sound The sound to dealy.
 * \param delay The delay in seconds.
 * \return A handle of the delayed sound.
 */
extern AUD_Sound *AUD_Sound_delay(AUD_Sound *sound, float delay);

/**
 * Limits a sound.
 * \param sound The sound to limit.
 * \param start The start time in seconds.
 * \param end The stop time in seconds.
 * \return A handle of the limited sound.
 */
extern AUD_Sound *AUD_Sound_limit(AUD_Sound *sound, float start, float end);

/**
 * Ping pongs a sound.
 * \param sound The sound to ping pong.
 * \return A handle of the ping pong sound.
 */
extern AUD_Sound *AUD_Sound_pingpong(AUD_Sound *sound);

/**
 * Loops a sound.
 * \param sound The sound to loop.
 * \return A handle of the looped sound.
 */
extern AUD_Sound *AUD_Sound_loop(AUD_Sound *sound);

/**
 * Sets a remaining loop count of a looping sound that currently plays.
 * \param handle The playback handle.
 * \param loops The count of remaining loops, -1 for infinity.
 * \return Whether the handle is valid.
 */
extern int AUD_Handle_setLoopCount(AUD_Handle *handle, int loops);

/**
 * Rectifies a sound.
 * \param sound The sound to rectify.
 * \return A handle of the rectified sound.
 */
extern AUD_Sound *AUD_rectifySound(AUD_Sound *sound);

/**
 * Unloads a sound of any type.
 * \param sound The handle of the sound.
 */
extern void AUD_Sound_free(AUD_Sound *sound);

/**
 * Plays back a sound file.
 * \param sound The handle of the sound file.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return A handle to the played back sound.
 */
extern AUD_Handle *AUD_Device_play(AUD_Device* device, AUD_Sound *sound, int keep);

/**
 * Pauses a played back sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been playing or not.
 */
extern int AUD_Handle_pause(AUD_Handle *handle);

/**
 * Resumes a paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been paused or not.
 */
extern int AUD_Handle_resume(AUD_Handle *handle);

/**
 * Stops a playing or paused sound.
 * \param handle The handle to the sound.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_Handle_stop(AUD_Handle *handle);

extern void AUD_Device_stopAll(void* device);

/**
 * Sets the end behaviour of a playing or paused sound.
 * \param handle The handle to the sound.
 * \param keep When keep is true the sound source will not be deleted but set to
 *             paused when its end has been reached.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_Handle_setKeep(AUD_Handle *handle, int keep);

/**
 * Seeks a playing or paused sound.
 * \param handle The handle to the sound.
 * \param seekTo From where the sound file should be played back in seconds.
 * \return Whether the handle has been valid or not.
 */
extern int AUD_Handle_setPosition(AUD_Handle *handle, float seekTo);

/**
 * Retrieves the playback position of a handle.
 * \param handle The handle to the sound.
 * \return The current playback position in seconds or 0.0 if the handle is
 *         invalid.
 */
extern float AUD_Handle_getPosition(AUD_Handle *handle);

/**
 * Returns the status of a playing, paused or stopped sound.
 * \param handle The handle to the sound.
 * \return The status of the sound behind the handle.
 */
extern AUD_Status AUD_Handle_getStatus(AUD_Handle *handle);

/**
 * Sets the listener location.
 * \param location The new location.
 */
extern int AUD_Device_setListenerLocation(const float location[3]);

/**
 * Sets the listener velocity.
 * \param velocity The new velocity.
 */
extern int AUD_Device_setListenerVelocity(const float velocity[3]);

/**
 * Sets the listener orientation.
 * \param orientation The new orientation as quaternion.
 */
extern int AUD_Device_setListenerOrientation(const float orientation[4]);

/**
 * Sets the speed of sound.
 * This value is needed for doppler effect calculation.
 * \param speed The new speed of sound.
 */
extern int AUD_Device_setSpeedOfSound(void* device, float speed);

/**
 * Sets the doppler factor.
 * This value is a scaling factor for the velocity vectors of sources and
 * listener which is used while calculating the doppler effect.
 * \param factor The new doppler factor.
 */
extern int AUD_Device_setDopplerFactor(void* device, float factor);

/**
 * Sets the distance model.
 * \param model distance model.
 */
extern int AUD_Device_setDistanceModel(void* device, AUD_DistanceModel model);

/**
 * Sets the location of a source.
 * \param handle The handle of the source.
 * \param location The new location.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setLocation(AUD_Handle *handle, const float location[3]);

/**
 * Sets the velocity of a source.
 * \param handle The handle of the source.
 * \param velocity The new velocity.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setVelocity(AUD_Handle *handle, const float velocity[3]);

/**
 * Sets the orientation of a source.
 * \param handle The handle of the source.
 * \param orientation The new orientation as quaternion.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setOrientation(AUD_Handle *handle, const float orientation[4]);

/**
 * Sets whether the source location, velocity and orientation are relative
 * to the listener.
 * \param handle The handle of the source.
 * \param relative Whether the source is relative.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setRelative(AUD_Handle *handle, int relative);

/**
 * Sets the maximum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new maximum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setVolumeMaximum(AUD_Handle *handle, float volume);

/**
 * Sets the minimum volume of a source.
 * \param handle The handle of the source.
 * \param volume The new minimum volume.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setVolumeMinimum(AUD_Handle *handle, float volume);

/**
 * Sets the maximum distance of a source.
 * If a source is further away from the reader than this distance, the
 * volume will automatically be set to 0.
 * \param handle The handle of the source.
 * \param distance The new maximum distance.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setDistanceMaximum(AUD_Handle *handle, float distance);

/**
 * Sets the reference distance of a source.
 * \param handle The handle of the source.
 * \param distance The new reference distance.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setDistanceReference(AUD_Handle *handle, float distance);

/**
 * Sets the attenuation of a source.
 * This value is used for distance calculation.
 * \param handle The handle of the source.
 * \param factor The new attenuation.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setAttenuation(AUD_Handle *handle, float factor);

/**
 * Sets the outer angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new outer angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setConeAngleOuter(AUD_Handle *handle, float angle);

/**
 * Sets the inner angle of the cone of a source.
 * \param handle The handle of the source.
 * \param angle The new inner angle of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setConeAngleInner(AUD_Handle *handle, float angle);

/**
 * Sets the outer volume of the cone of a source.
 * The volume between inner and outer angle is interpolated between inner
 * volume and this value.
 * \param handle The handle of the source.
 * \param volume The new outer volume of the cone.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setConeVolumeOuter(AUD_Handle *handle, float volume);

/**
 * Sets the volume of a played back sound.
 * \param handle The handle to the sound.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setVolume(AUD_Handle *handle, float volume);

/**
 * Sets the pitch of a played back sound.
 * \param handle The handle to the sound.
 * \param pitch The new pitch.
 * \return Whether the action succeeded.
 */
extern int AUD_Handle_setPitch(AUD_Handle *handle, float pitch);

/**
 * Opens a read device, with which audio data can be read.
 * \param specs The specification of the audio data.
 * \return A device handle.
 */
extern AUD_Device *AUD_openReadDevice(AUD_DeviceSpecs specs);

/**
 * Sets the main volume of a device.
 * \param device The device.
 * \param volume The new volume, must be between 0.0 and 1.0.
 * \return Whether the action succeeded.
 */
extern int AUD_setDeviceVolume(AUD_Device *device, float volume);

/**
 * Plays back a sound file through a read device.
 * \param device The read device.
 * \param sound The handle of the sound file.
 * \param seek The position where the sound should be seeked to.
 * \return A handle to the played back sound.
 */
extern AUD_Handle *AUD_playDevice(AUD_Device *device, AUD_Sound *sound, float seek);

/**
 * Reads the next samples into the supplied buffer.
 * \param device The read device.
 * \param buffer The target buffer.
 * \param length The length in samples to be filled.
 * \return True if the reading succeeded, false if there are no sounds
 *         played back currently, in that case the buffer is filled with
 *         silence.
 */
extern int AUD_Device_read(AUD_Device *device, data_t *buffer, int length);

/**
 * Closes a read device.
 * \param device The read device.
 */
extern void AUD_Device_free(AUD_Device *device);

/**
 * Reads a sound file into a newly created float buffer.
 * The sound is therefore bandpassed, rectified and resampled.
 */
extern float *AUD_readSoundBuffer(const char *filename, float low, float high,
                                  float attack, float release, float threshold,
                                  int accumulate, int additive, int square,
                                  float sthreshold, double samplerate,
                                  int *length);

/**
 * Pauses a playing sound after a specific amount of time.
 * \param handle The handle to the sound.
 * \param seconds The time in seconds.
 * \return The silence handle.
 */
extern AUD_Handle *AUD_pauseAfter(AUD_Handle *handle, float seconds);

/**
 * Creates a new sequenced sound scene.
 * \param fps The FPS of the scene.
 * \param muted Whether the scene is muted.
 * \return The new sound scene.
 */
extern AUD_Sound *AUD_Sequence_create(float fps, int muted);

/**
 * Deletes a sound scene.
 * \param sequencer The sound scene.
 */
extern void AUD_Sequence_free(AUD_Sound *sequencer);

/**
 * Sets the muting state of the scene.
 * \param sequencer The sound scene.
 * \param muted Whether the scene is muted.
 */
extern void AUD_Sequence_setMuted(AUD_Sound *sequencer, int muted);

/**
 * Sets the scene's FPS.
 * \param sequencer The sound scene.
 * \param fps The new FPS.
 */
extern void AUD_Sequence_setFPS(AUD_Sound *sequencer, float fps);

/**
 * Adds a new entry to the scene.
 * \param sequencer The sound scene.
 * \param sound The sound this entry should play.
 * \param begin The start time.
 * \param end The end time or a negative value if determined by the sound.
 * \param skip How much seconds should be skipped at the beginning.
 * \return The entry added.
 */
extern AUD_SEntry *AUD_Sequence_add(AUD_Sound *sequencer, AUD_Sound *sound,
                                   float begin, float end, float skip);

/**
 * Removes an entry from the scene.
 * \param sequencer The sound scene.
 * \param entry The entry to remove.
 */
extern void AUD_Sequence_remove(AUD_Sound *sequencer, AUD_SEntry *entry);

/**
 * Moves the entry.
 * \param entry The sequenced entry.
 * \param begin The new start time.
 * \param end The new end time or a negative value if unknown.
 * \param skip How many seconds to skip at the beginning.
 */
extern void AUD_SequenceEntry_move(AUD_SEntry *entry, float begin, float end, float skip);

/**
 * Sets the muting state of the entry.
 * \param entry The sequenced entry.
 * \param mute Whether the entry should be muted or not.
 */
extern void AUD_SequenceEntry_setMuted(AUD_SEntry *entry, char mute);

/**
 * Sets the sound of the entry.
 * \param entry The sequenced entry.
 * \param sound The new sound.
 */
extern void AUD_SequenceEntry_setSound(AUD_SEntry *entry, AUD_Sound *sound);

/**
 * Writes animation data to a sequenced entry.
 * \param entry The sequenced entry.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern void AUD_SequenceEntry_setAnimationData(AUD_SEntry *entry, AUD_AnimateablePropertyType type, int frame, float *data, char animated);

/**
 * Writes animation data to a sequenced entry.
 * \param sequencer The sound scene.
 * \param type The type of animation data.
 * \param frame The frame this data is for.
 * \param data The data to write.
 * \param animated Whether the attribute is animated.
 */
extern void AUD_Sequence_setAnimationData(AUD_Sound *sequencer, AUD_AnimateablePropertyType type, int frame, float *data, char animated);

/**
 * Sets the distance model of a sequence.
 * param sequence The sequence to set the distance model from.
 * param value The new distance model to set.
 */
extern void AUD_Sequence_setDistanceModel(AUD_Sound* sequence, AUD_DistanceModel value);

/**
 * Sets the doppler factor of a sequence.
 * param sequence The sequence to set the doppler factor from.
 * param value The new doppler factor to set.
 */
extern void AUD_Sequence_setDopplerFactor(AUD_Sound* sequence, float value);

/**
 * Sets the speed of sound of a sequence.
 * param sequence The sequence to set the speed of sound from.
 * param value The new speed of sound to set.
 */
extern void AUD_Sequence_setSpeedOfSound(AUD_Sound* sequence, float value);
/**
 * Sets the attenuation of a sequence_entry.
 * param sequence_entry The sequence_entry to set the attenuation from.
 * param value The new attenuation to set.
 */
extern void AUD_SequenceEntry_setAttenuation(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the cone angle inner of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone angle inner from.
 * param value The new cone angle inner to set.
 */
extern void AUD_SequenceEntry_setConeAngleInner(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the cone angle outer of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone angle outer from.
 * param value The new cone angle outer to set.
 */
extern void AUD_SequenceEntry_setConeAngleOuter(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the cone volume outer of a sequence_entry.
 * param sequence_entry The sequence_entry to set the cone volume outer from.
 * param value The new cone volume outer to set.
 */
extern void AUD_SequenceEntry_setConeVolumeOuter(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the distance maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the distance maximum from.
 * param value The new distance maximum to set.
 */
extern void AUD_SequenceEntry_setDistanceMaximum(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the distance reference of a sequence_entry.
 * param sequence_entry The sequence_entry to set the distance reference from.
 * param value The new distance reference to set.
 */
extern void AUD_SequenceEntry_setDistanceReference(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the relative of a sequence_entry.
 * param sequence_entry The sequence_entry to set the relative from.
 * param value The new relative to set.
 */
extern void AUD_SequenceEntry_setRelative(AUD_SEntry* sequence_entry, int value);

/**
 * Sets the volume maximum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the volume maximum from.
 * param value The new volume maximum to set.
 */
extern void AUD_SequenceEntry_setVolumeMaximum(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the volume minimum of a sequence_entry.
 * param sequence_entry The sequence_entry to set the volume minimum from.
 * param value The new volume minimum to set.
 */
extern void AUD_SequenceEntry_setVolumeMinimum(AUD_SEntry* sequence_entry, float value);

/**
 * Sets the audio output specification of the sound scene to the specs of the
 * current playback device.
 * \param sequencer The sound scene.
 */
extern void AUD_setSequencerDeviceSpecs(AUD_Sound *sequencer);

/**
 * Sets the audio output specification of the sound scene.
 * \param sequencer The sound scene.
 * \param specs The new specification.
 */
extern void AUD_Sequence_setSpecs(AUD_Sound *sequencer, AUD_Specs specs);

/**
 * Seeks sequenced sound scene playback.
 * \param handle Playback handle.
 * \param time Time in seconds to seek to.
 */
extern void AUD_seekSynchronizer(AUD_Handle *handle, float time);

/**
 * Returns the current sound scene playback time.
 * \param handle Playback handle.
 * \return The playback time in seconds.
 */
extern float AUD_getSynchronizerPosition(AUD_Handle *handle);

/**
 * Starts the playback of jack transport if possible.
 */
extern void AUD_playSynchronizer(void);

/**
 * Stops the playback of jack transport if possible.
 */
extern void AUD_stopSynchronizer(void);

#ifdef WITH_JACK
/**
 * Sets the sync callback for jack transport.
 * \param function The callback function.
 * \param data The data parameter for the callback.
 */
extern void AUD_setSynchronizerCallback(AUD_syncFunction function, void *data);
#endif

/**
 * Returns whether jack transport is currently playing.
 * \return Whether jack transport is currently playing.
 */
extern int AUD_isSynchronizerPlaying(void);

/**
 * Reads a sound into a buffer for drawing at a specific sampling rate.
 * \param sound The sound to read.
 * \param buffer The buffer to write to. Must have a size of 3*4*length.
 * \param length How many samples to read from the sound.
 * \param samples_per_second How many samples to read per second of the sound.
 * \return How many samples really have been read. Always <= length.
 */
extern int AUD_readSound(AUD_Sound *sound, sample_t *buffer, int length, int samples_per_second, short *interrupt);

/**
 * Copies a sound.
 * \param sound Sound to copy.
 * \return Copied sound.
 */
extern AUD_Sound *AUD_Sound_copy(AUD_Sound *sound);

/**
 * Frees a handle.
 * \param channel Handle to free.
 */
extern void AUD_Handle_free(AUD_Handle *channel);

/**
 * Creates a new set.
 * \return The new set.
 */
extern void *AUD_createSet(void);

/**
 * Deletes a set.
 * \param set The set to delete.
 */
extern void AUD_destroySet(void *set);

/**
 * Removes an entry from a set.
 * \param set The set work on.
 * \param entry The entry to remove.
 * \return Whether the entry was in the set or not.
 */
extern char AUD_removeSet(void *set, void *entry);

/**
 * Adds a new entry to a set.
 * \param set The set work on.
 * \param entry The entry to add.
 */
extern void AUD_addSet(void *set, void *entry);

/**
 * Removes one entry from a set and returns it.
 * \param set The set work on.
 * \return The entry or NULL if the set is empty.
 */
extern void *AUD_getSet(void *set);

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
extern const char *AUD_mixdown(AUD_Sound *sound, unsigned int start, unsigned int length,
                               unsigned int buffersize, const char *filename,
                               AUD_DeviceSpecs specs, AUD_Container format,
                               AUD_Codec codec, unsigned int bitrate);

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
extern const char *AUD_mixdown_per_channel(AUD_Sound *sound, unsigned int start, unsigned int length,
                                           unsigned int buffersize, const char *filename,
                                           AUD_DeviceSpecs specs, AUD_Container format,
                                           AUD_Codec codec, unsigned int bitrate);

/**
 * Opens a read device and prepares it for mixdown of the sound scene.
 * \param specs Output audio specifications.
 * \param sequencer The sound scene to mix down.
 * \param volume The overall mixdown volume.
 * \param start The start time of the mixdown in the sound scene.
 * \return The read device for the mixdown.
 */
extern AUD_Device *AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound *sequencer, float volume, float start);

#ifdef WITH_PYTHON
/**
 * Retrieves the python factory of a sound.
 * \param sound The sound factory.
 * \return The python factory.
 */
extern void *AUD_getPythonSound(AUD_Sound *sound);

/**
 * Retrieves the sound factory of a python factory.
 * \param sound The python factory.
 * \return The sound factory.
 */
extern AUD_Sound *AUD_getSoundFromPython(void *sound);
#endif

extern AUD_Device *AUD_Device_getCurrent(void);

extern int AUD_isJackSupported(void);

#ifdef __cplusplus
}
#endif

#endif //__AUD_C_API_H__
