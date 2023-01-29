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

/**
 * Returns information about a sound.
 * \param sound The sound to get the info about.
 * \return The AUD_SoundInfo structure with filled in data.
 */
extern AUD_API AUD_SoundInfo AUD_getInfo(AUD_Sound* sound);

/**
 * Reads a sound file into a newly created float buffer.
 * The sound is therefore bandpassed, rectified and resampled.
 */
extern AUD_API float* AUD_readSoundBuffer(const char* filename, float low, float high,
								  float attack, float release, float threshold,
								  int accumulate, int additive, int square,
								  float sthreshold, double samplerate,
								  int* length, int stream);

/**
 * Pauses a playing sound after a specific amount of time.
 * \param handle The handle to the sound.
 * \param seconds The time in seconds.
 * \return The silence handle.
 */
extern AUD_API AUD_Handle* AUD_pauseAfter(AUD_Handle* handle, double seconds);

/**
 * Reads a sound into a buffer for drawing at a specific sampling rate.
 * \param sound The sound to read.
 * \param buffer The buffer to write to. Must have a size of 3*4*length.
 * \param length How many samples to read from the sound.
 * \param samples_per_second How many samples to read per second of the sound.
 * \param interrupt Must point to a short that equals 0. If it is set to a non-zero value, the method will be interrupted and return 0.
 * \return How many samples really have been read. Always <= length.
 */
extern AUD_API int AUD_readSound(AUD_Sound* sound, float* buffer, int length, int samples_per_second, short* interrupt);

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
 * \param callback A callback function that is called periodically during mixdown, reporting progress if length > 0. Can be NULL.
 * \param data Pass through parameter that is passed to the callback.
 * \param error String buffer to copy the error message to in case of failure.
 * \param errorsize The size of the error buffer.
 * \return Whether or not the operation succeeded.
 */
extern AUD_API int AUD_mixdown(AUD_Sound* sound, unsigned int start, unsigned int length,
							   unsigned int buffersize, const char* filename,
							   AUD_DeviceSpecs specs, AUD_Container format,
							   AUD_Codec codec, unsigned int bitrate,
							   void(*callback)(float, void*), void* data, char* error, size_t errorsize);

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
 * \param callback A callback function that is called periodically during mixdown, reporting progress if length > 0. Can be NULL.
 * \param data Pass through parameter that is passed to the callback.
 * \param error String buffer to copy the error message to in case of failure.
 * \param errorsize The size of the error buffer.
 * \return Whether or not the operation succeeded.
 */
extern AUD_API int AUD_mixdown_per_channel(AUD_Sound* sound, unsigned int start, unsigned int length,
										   unsigned int buffersize, const char* filename,
										   AUD_DeviceSpecs specs, AUD_Container format,
										   AUD_Codec codec, unsigned int bitrate,
										   void(*callback)(float, void*), void* data, char* error, size_t errorsize);

/**
 * Opens a read device and prepares it for mixdown of the sound scene.
 * \param specs Output audio specifications.
 * \param sequencer The sound scene to mix down.
 * \param volume The overall mixdown volume.
 * \param start The start time of the mixdown in the sound scene.
 * \return The read device for the mixdown.
 */
extern AUD_API AUD_Device* AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound* sequencer, float volume, double start);

/**
 * Initializes audio routines (FFMPEG/JACK if it is enabled).
 */
extern AUD_API void AUD_initOnce();

/**
 * Unitinitializes an audio routines.
 */
extern AUD_API void AUD_exitOnce();

/**
 * Initializes an audio device.
 * \param device The device type that should be used.
 * \param specs The audio specification to be used.
 * \param buffersize The buffersize for the device.
 * \return Whether the device has been initialized.
 */
extern AUD_API AUD_Device* AUD_init(const char* device, AUD_DeviceSpecs specs, int buffersize, const char* name);

/**
 * Unitinitializes an audio device.
 * \param device The device to free.
 */
extern AUD_API void AUD_exit(AUD_Device* device);

/**
 * Retrieves available devices. Note that all memory returned has to be freed!
 */
extern AUD_API char** AUD_getDeviceNames();

#ifdef __cplusplus
}
#endif
