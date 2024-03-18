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

#include "Audaspace.h"

#ifdef __cplusplus
using namespace aud;
#endif

#ifdef AUD_CAPI_IMPLEMENTATION
#include "ISound.h"
#include "devices/IHandle.h"
#include "devices/IDevice.h"
#include "sequence/SequenceEntry.h"
#include "fx/PlaybackManager.h"
#include "fx/DynamicMusic.h"
#include "fx/Source.h"
#include "util/ThreadPool.h"
#ifdef WITH_CONVOLUTION
#include "fx/ImpulseResponse.h"
#include "fx/HRTF.h"
#endif

typedef std::shared_ptr<aud::ISound> AUD_Sound;
typedef std::shared_ptr<aud::IHandle> AUD_Handle;
typedef std::shared_ptr<aud::IDevice> AUD_Device;
typedef std::shared_ptr<aud::SequenceEntry> AUD_SequenceEntry;
typedef std::shared_ptr<aud::PlaybackManager> AUD_PlaybackManager;
typedef std::shared_ptr<aud::DynamicMusic> AUD_DynamicMusic;
typedef std::shared_ptr<aud::ThreadPool> AUD_ThreadPool;
typedef std::shared_ptr<aud::Source> AUD_Source;
#ifdef WITH_CONVOLUTION
typedef std::shared_ptr<aud::ImpulseResponse> AUD_ImpulseResponse;
typedef std::shared_ptr<aud::HRTF> AUD_HRTF;
#endif
#else
typedef void AUD_Sound;
typedef void AUD_Handle;
typedef void AUD_Device;
typedef void AUD_SequenceEntry;
typedef void AUD_PlaybackManager;
typedef void AUD_DynamicMusic;
typedef void AUD_ThreadPool;
typedef void AUD_Source;
#ifdef WITH_CONVOLUTION
typedef void AUD_ImpulseResponse;
typedef void AUD_HRTF;
#endif
#endif

/// Container formats for writers.
typedef enum
{
	AUD_CONTAINER_INVALID = 0,
	AUD_CONTAINER_AC3,
	AUD_CONTAINER_FLAC,
	AUD_CONTAINER_MATROSKA,
	AUD_CONTAINER_MP2,
	AUD_CONTAINER_MP3,
	AUD_CONTAINER_OGG,
	AUD_CONTAINER_WAV
} AUD_Container;

/// Audio codecs for writers.
typedef enum
{
	AUD_CODEC_INVALID = 0,
	AUD_CODEC_AAC,
	AUD_CODEC_AC3,
	AUD_CODEC_FLAC,
	AUD_CODEC_MP2,
	AUD_CODEC_MP3,
	AUD_CODEC_PCM,
	AUD_CODEC_VORBIS,
	AUD_CODEC_OPUS
} AUD_Codec;

/**
 * The format of a sample.
 * The last 4 bit save the byte count of the format.
 */
typedef enum
{
	AUD_FORMAT_INVALID = 0x00,		/// Invalid sample format.
	AUD_FORMAT_U8      = 0x01,		/// 1 byte unsigned byte.
	AUD_FORMAT_S16     = 0x12,		/// 2 byte signed integer.
	AUD_FORMAT_S24     = 0x13,		/// 3 byte signed integer.
	AUD_FORMAT_S32     = 0x14,		/// 4 byte signed integer.
	AUD_FORMAT_FLOAT32 = 0x24,		/// 4 byte float.
	AUD_FORMAT_FLOAT64 = 0x28		/// 8 byte float.
} AUD_SampleFormat;

/// The channel count.
typedef enum
{
	AUD_CHANNELS_INVALID    = 0,	/// Invalid channel count.
	AUD_CHANNELS_MONO       = 1,	/// Mono.
	AUD_CHANNELS_STEREO     = 2,	/// Stereo.
	AUD_CHANNELS_STEREO_LFE = 3,	/// Stereo with LFE channel.
	AUD_CHANNELS_SURROUND4  = 4,	/// 4 channel surround sound.
	AUD_CHANNELS_SURROUND5  = 5,	/// 5 channel surround sound.
	AUD_CHANNELS_SURROUND51 = 6,	/// 5.1 surround sound.
	AUD_CHANNELS_SURROUND61 = 7,	/// 6.1 surround sound.
	AUD_CHANNELS_SURROUND71 = 8	/// 7.1 surround sound.
} AUD_Channels;

/**
 * The sample rate tells how many samples are played back within one second.
 * Some exotic formats may use other sample rates than provided here.
 */
typedef enum
{
	AUD_RATE_INVALID = 0,			/// Invalid sample rate.
	AUD_RATE_8000    = 8000,		/// 8000 Hz.
	AUD_RATE_16000   = 16000,		/// 16000 Hz.
	AUD_RATE_11025   = 11025,		/// 11025 Hz.
	AUD_RATE_22050   = 22050,		/// 22050 Hz.
	AUD_RATE_32000   = 32000,		/// 32000 Hz.
	AUD_RATE_44100   = 44100,		/// 44100 Hz.
	AUD_RATE_48000   = 48000,		/// 48000 Hz.
	AUD_RATE_88200   = 88200,		/// 88200 Hz.
	AUD_RATE_96000   = 96000,		/// 96000 Hz.
	AUD_RATE_192000  = 192000		/// 192000 Hz.
} AUD_DefaultSampleRate;

/// Sample rate type.
typedef double AUD_SampleRate;

/// Specification of a sound source.
typedef struct
{
	/// Sample rate in Hz.
	AUD_SampleRate rate;

	/// Channel count.
	AUD_Channels channels;
} AUD_Specs;

/// Specification of a sound device.
typedef struct
{
	/// Sample format.
	AUD_SampleFormat format;

	union
	{
		struct
		{
			/// Sample rate in Hz.
			AUD_SampleRate rate;

			/// Channel count.
			AUD_Channels channels;
		};
		AUD_Specs specs;
	};
} AUD_DeviceSpecs;

/// Sound information structure.
typedef struct
{
	AUD_Specs specs;
	float length;
} AUD_SoundInfo;

/// Specification of a sound source.
typedef struct
{
	/// Start time in seconds.
	double start;

	/// Duration in seconds. May be estimated or 0 if unknown.
	double duration;

	/// Audio data parameters.
	AUD_DeviceSpecs specs;
} AUD_StreamInfo;
