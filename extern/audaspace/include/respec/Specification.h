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

/**
 * @file Specification.h
 * @ingroup respec
 * Defines all important macros and basic data structures for stream format descriptions.
 */

#include "Audaspace.h"

/// The size of a format in bytes.
#define AUD_FORMAT_SIZE(format) (format & 0x0F)
/// The size of a sample in the specified device format in bytes.
#define AUD_DEVICE_SAMPLE_SIZE(specs) (specs.channels * (specs.format & 0x0F))
/// The size of a sample in the specified format in bytes.
#define AUD_SAMPLE_SIZE(specs) (specs.channels * sizeof(sample_t))

/// Compares two audio data specifications.
#define AUD_COMPARE_SPECS(s1, s2) ((s1.rate == s2.rate) && (s1.channels == s2.channels))

/// Returns the bit for a channel mask.
#define AUD_CHANNEL_BIT(channel) (0x01 << channel)

AUD_NAMESPACE_BEGIN

/**
 * The format of a sample.
 * The last 4 bit save the byte count of the format.
 */
enum SampleFormat
{
	FORMAT_INVALID = 0x00,		/// Invalid sample format.
	FORMAT_U8      = 0x01,		/// 1 byte unsigned byte.
	FORMAT_S16     = 0x12,		/// 2 byte signed integer.
	FORMAT_S24     = 0x13,		/// 3 byte signed integer.
	FORMAT_S32     = 0x14,		/// 4 byte signed integer.
	FORMAT_FLOAT32 = 0x24,		/// 4 byte float.
	FORMAT_FLOAT64 = 0x28		/// 8 byte float.
};

/// The channel count.
enum Channels
{
	CHANNELS_INVALID    = 0,	/// Invalid channel count.
	CHANNELS_MONO       = 1,	/// Mono.
	CHANNELS_STEREO     = 2,	/// Stereo.
	CHANNELS_STEREO_LFE = 3,	/// Stereo with LFE channel.
	CHANNELS_SURROUND4  = 4,	/// 4 channel surround sound.
	CHANNELS_SURROUND5  = 5,	/// 5 channel surround sound.
	CHANNELS_SURROUND51 = 6,	/// 5.1 surround sound.
	CHANNELS_SURROUND61 = 7,	/// 6.1 surround sound.
	CHANNELS_SURROUND71 = 8	/// 7.1 surround sound.
};

/// The channel names.
enum Channel
{
	CHANNEL_FRONT_LEFT = 0,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_FRONT_CENTER,
	CHANNEL_LFE,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT,
	CHANNEL_REAR_CENTER,
	CHANNEL_SIDE_LEFT,
	CHANNEL_SIDE_RIGHT,
	CHANNEL_MAX
};

/**
 * The sample rate tells how many samples are played back within one second.
 * Some exotic formats may use other sample rates than provided here.
 */
enum DefaultSampleRate
{
	RATE_INVALID = 0,			/// Invalid sample rate.
	RATE_8000    = 8000,		/// 8000 Hz.
	RATE_16000   = 16000,		/// 16000 Hz.
	RATE_11025   = 11025,		/// 11025 Hz.
	RATE_22050   = 22050,		/// 22050 Hz.
	RATE_32000   = 32000,		/// 32000 Hz.
	RATE_44100   = 44100,		/// 44100 Hz.
	RATE_48000   = 48000,		/// 48000 Hz.
	RATE_88200   = 88200,		/// 88200 Hz.
	RATE_96000   = 96000,		/// 96000 Hz.
	RATE_192000  = 192000		/// 192000 Hz.
};

/// Sample rate type.
typedef double SampleRate;

/// Specification of a sound source.
struct Specs
{
	/// Sample rate in Hz.
	SampleRate rate;

	/// Channel count.
	Channels channels;
};

/// Specification of a sound device.
struct DeviceSpecs
{
	/// Sample format.
	SampleFormat format;

	union
	{
		struct
		{
			/// Sample rate in Hz.
			SampleRate rate;

			/// Channel count.
			Channels channels;
		};
		Specs specs;
	};
};

AUD_NAMESPACE_END
